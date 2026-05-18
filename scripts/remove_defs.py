import sys
import re
Import("env")

# Read the list of defines to remove from custom ini key.
names = env.GetProjectOption("custom_remove_defines", "")
to_remove = {n.strip() for n in names.split(",") if n.strip()}
if to_remove:
    print("[remove_defs] Will remove defines:", to_remove)

    # Pull user-declared build_flags from platformio.ini file.
    raw = env.GetProjectOption("build_flags", [])
    print("[remove_defs] Original build_flags:", raw)

    # Scan & filter out selected define(s).
    kept = []
    removed = []
    i = 0
    while i < len(raw):
        token = raw[i]

        # Match “-DMACRO” or “-DMACRO=value”.
        m = re.match(r"-D\s*([A-Za-z_]\w*)(?:=.*)?$", token)
        if m and m.group(1) in to_remove:
            removed.append(token)
            i += 1
            continue

        # Otherwise, keep it.
        kept.append(token)
        i += 1

    # Push the filtered list back into BUILD_FLAGS.
    if removed:
        print("[remove_defs] Stripped out flags:", removed)
        env.Replace(BUILD_FLAGS=kept)
    else:
        print("[remove_defs] No matching defines found, keeping all flags.")

    print("[remove_defs] New build_flags:", env['BUILD_FLAGS'])
else:
    print("[remove_defs] No defines specified → skipping.")
