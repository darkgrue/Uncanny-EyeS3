import hashlib
import re
import os
Import("env")

# ─── Configuration ────────────────────────────────────────────────────────────

PATCHES_DIR = os.path.join(env.get("PROJECT_DIR"), "patches")

# ─── Helpers ──────────────────────────────────────────────────────────────────


def get_libdeps_dir():
    """Returns the resolved libdeps directory for the current env."""
    return env.subst("$PROJECT_LIBDEPS_DIR/$PIOENV")


def sentinel_path(patch_file, libdeps_dir):
    """
    Returns the path for a sentinel file that marks a patch as applied.

    Sentinels are stored inside the environment's libdeps directory rather
    than alongside the patch file, so each PlatformIO environment tracks its
    own applied state independently.  They are also automatically removed when
    PlatformIO cleans or rebuilds libdeps for that environment.

    A short hash of the full patch path is included in the filename to avoid
    collisions when two patches in different subdirectories share the same
    base filename.
    """
    patch_hash = hashlib.md5(patch_file.encode()).hexdigest()[:8]
    basename = os.path.basename(patch_file)
    sentinel_name = f".patch_applied.{basename}.{patch_hash}"
    return os.path.join(libdeps_dir, sentinel_name)


def parse_patch(patch_text):
    """
    Parse a unified diff patch into a list of file operations.
    Returns a list of dicts: {filename, hunks: [{old_start, old_count, new_start, new_count, old_lines, new_lines}]}
    """
    files = []
    current_file = None
    current_hunk = None
    lines = patch_text.splitlines()
    i = 0

    while i < len(lines):
        line = lines[i]

        # Detect target file from '--- ...' / '+++ ...' headers.
        #
        # We support two formats:
        #
        # 1. git diff:
        #      --- a/SomeLib/src/File.cpp
        #      +++ b/SomeLib/src/File.cpp
        #    → strip the leading a/ or b/ prefix; path is already libdeps-relative.
        #
        # 2. Plain diff (e.g. "diff -u orig modified"):
        #      --- ../../.pio/libdeps/mitail/SomeLib/src/File.cpp   <timestamp>
        #      +++ ./File.cpp                                        <timestamp>
        #    → the --- line contains the full path to the original file inside
        #      .pio/libdeps; extract everything after "libdeps/<env>/" to get
        #      the libdeps-relative path.  The +++ line is just a local working
        #      copy and is intentionally ignored for path resolution.
        if line.startswith("--- "):
            i += 1
            if i < len(lines) and lines[i].startswith("+++ "):
                # drop "--- " + timestamp
                minus_raw = line[4:].split("\t")[0].strip()
                # Normalize to forward slashes for consistent regex matching
                minus_norm = minus_raw.replace("\\", "/")

                # ── Format 1: git diff  (--- a/Lib/src/File.cpp) ──────────────
                m_git = re.match(r"^[ab]/(.+)$", minus_norm)
                if m_git:
                    rel = m_git.group(1)

                # ── Format 2: plain diff with .pio/libdeps/<env>/ prefix ──────
                # Matches any path containing .pio/libdeps/<something>/ and
                # captures everything after the <env>/ segment.
                else:
                    m_pio = re.search(r"\.pio/libdeps/[^/]+/(.+)$", minus_norm)
                    if m_pio:
                        rel = m_pio.group(1)
                    else:
                        # Fallback: strip leading ./ or ../ components and hope
                        # for the best — better than silently skipping the file.
                        rel = re.sub(r"^(?:\.\./|\./)+", "", minus_norm)

                # Re-apply the OS separator so os.path.join works correctly
                rel = rel.replace("/", os.sep)
                current_file = {"filename": rel, "hunks": []}
                files.append(current_file)
            continue

        # Hunk header: @@ -old_start,old_count +new_start,new_count @@
        if line.startswith("@@") and current_file is not None:
            m = re.match(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", line)
            if m:
                current_hunk = {
                    "old_start": int(m.group(1)),
                    "old_count": int(m.group(2) if m.group(2) is not None else 1),
                    "new_start": int(m.group(3)),
                    "new_count": int(m.group(4) if m.group(4) is not None else 1),
                    "old_lines": [],
                    "new_lines": [],
                }
                current_file["hunks"].append(current_hunk)
            i += 1
            continue

        # Hunk body
        if current_hunk is not None:
            if line.startswith("-"):
                current_hunk["old_lines"].append(line[1:])
            elif line.startswith("+"):
                current_hunk["new_lines"].append(line[1:])
            elif line.startswith(" "):
                current_hunk["old_lines"].append(line[1:])
                current_hunk["new_lines"].append(line[1:])
            elif line.startswith("\\"):
                pass  # "\ No newline at end of file" — ignore
            else:
                current_hunk = None  # End of hunk

        i += 1

    return files


def apply_hunk(content_lines, hunk):
    """
    Apply a single hunk to content_lines (list of strings without newlines).
    Searches for the old_lines block starting near the hinted line number,
    then expands outward to handle line-number drift from previous hunks.
    Returns the modified list, or raises RuntimeError on failure.
    """
    old = hunk["old_lines"]
    new = hunk["new_lines"]

    if not old:
        # Pure insertion — insert at hunk position
        pos = max(0, hunk["new_start"] - 1)
        return content_lines[:pos] + new + content_lines[pos:]

    hint = hunk["old_start"] - 1  # convert 1-based to 0-based
    max_pos = len(content_lines) - len(old)

    if max_pos < 0:
        raise RuntimeError(
            f"File is shorter than the hunk context ({len(content_lines)} lines, "
            f"hunk needs at least {len(old)})."
        )

    # Build search order: start at hint, then alternate outward
    visited = set()
    search_order = []
    for delta in range(0, len(content_lines) + 1):
        for candidate in (hint + delta, hint - delta):
            if 0 <= candidate <= max_pos and candidate not in visited:
                visited.add(candidate)
                search_order.append(candidate)

    for start in search_order:
        if content_lines[start: start + len(old)] == old:
            return content_lines[:start] + new + content_lines[start + len(old):]

    raise RuntimeError(
        f"Could not find hunk context near line {hunk['old_start']}.\n"
        f"  First expected line: {repr(old[0]) if old else '(empty)'}"
    )


def apply_patch_to_file(target_path, file_patch):
    """
    Apply all hunks for one file entry from a parsed patch.
    Hunks are applied sequentially; each hunk result feeds the next.
    Returns True on full success, False if the file was missing or any hunk failed.
    """
    if not os.path.isfile(target_path):
        print(f"    [WARN] Target file not found, skipping: {target_path}")
        return False

    with open(target_path, "r", encoding="utf-8", errors="replace") as f:
        original = f.read()

    lines = original.splitlines()

    for idx, hunk in enumerate(file_patch["hunks"]):
        try:
            lines = apply_hunk(lines, hunk)
        except RuntimeError as e:
            print(
                f"    [ERROR] Hunk {idx + 1} failed in {target_path}:\n      {e}")
            return False

    patched = "\n".join(lines)
    # Preserve trailing newline if the original had one
    if original.endswith("\n") and not patched.endswith("\n"):
        patched += "\n"

    with open(target_path, "w", encoding="utf-8", newline="") as f:
        f.write(patched)

    return True


def apply_patch_file(patch_path, libdeps_dir):
    """
    Apply a single .patch file to the files it targets inside libdeps_dir.
    Uses a per-environment sentinel file in libdeps_dir to avoid re-applying.
    """
    sentinel = sentinel_path(patch_path, libdeps_dir)
    if os.path.exists(sentinel):
        print(f"  [SKIP]  Already applied: {os.path.basename(patch_path)}")
        return True

    print(f"  [APPLY] {os.path.basename(patch_path)}")

    with open(patch_path, "r", encoding="utf-8", errors="replace") as f:
        patch_text = f.read()

    file_patches = parse_patch(patch_text)

    if not file_patches:
        print(f"  [WARN]  No file hunks parsed from: {patch_path}")
        return False

    all_ok = True
    for fp in file_patches:
        # Patch filenames are relative to the libdeps env directory
        target = os.path.normpath(os.path.join(libdeps_dir, fp["filename"]))
        print(f"    -> {os.path.relpath(target, libdeps_dir)}")
        if not apply_patch_to_file(target, fp):
            all_ok = False

    if all_ok:
        # Write sentinel to prevent re-application on subsequent builds
        with open(sentinel, "w") as f:
            f.write("patched\n")
        print(f"  [OK]    {os.path.basename(patch_path)}")
    else:
        print(
            f"  [FAIL]  One or more hunks could not be applied: {os.path.basename(patch_path)}")

    return all_ok


def run_patches(source=None, target=None, env=env):
    """
    Walk the patches directory and apply every .patch file found.
    Safe to call multiple times — sentinel files make it idempotent.
    """
    libdeps_dir = get_libdeps_dir()

    if not os.path.isdir(PATCHES_DIR):
        return  # No patches directory — nothing to do

    if not os.path.isdir(libdeps_dir):
        # Clean build: libdeps haven't been fetched yet; the AddPreAction
        # hook will fire again after they are.
        print("[patch_libdeps] libdeps directory not yet present — deferring.")
        return

    print(f"\n[patch_libdeps] Patches : {PATCHES_DIR}")
    print(f"[patch_libdeps] libdeps : {libdeps_dir}\n")

    patches_found = 0
    patches_failed = 0

    for root, dirs, files in os.walk(PATCHES_DIR):
        dirs.sort()   # deterministic traversal order
        for fname in sorted(files):
            if not fname.endswith(".patch"):
                continue
            patches_found += 1
            ok = apply_patch_file(os.path.join(root, fname), libdeps_dir)
            if not ok:
                patches_failed += 1

    if patches_found == 0:
        print("[patch_libdeps] No .patch files found.\n")
    elif patches_failed:
        print(
            f"\n[patch_libdeps] WARNING: {patches_failed}/{patches_found} patch(es) failed.\n")
    else:
        print(
            f"\n[patch_libdeps] All {patches_found} patch(es) processed successfully.\n")


# ─── Trigger 1: immediate ─────────────────────────────────────────────────────
# Runs during build-graph construction. Handles incremental builds where
# libdeps are already on disk from a previous run.
run_patches()

# ─── Trigger 2: pre-compilation hook ─────────────────────────────────────────
# Runs before the first source file in src/ is compiled. Handles clean builds
# where PlatformIO fetches libdeps during this same invocation, after the
# script body above already executed.
#
# The glob catches the first .c/.cpp/.ino object target SCons registers,
# ensuring we fire before any compilation touches the (now-patched) headers.
env.AddPreAction(
    "$BUILD_DIR/src/*.o",
    env.Action(
        run_patches, "[patch_libdeps] Checking patches before compilation...")
)
