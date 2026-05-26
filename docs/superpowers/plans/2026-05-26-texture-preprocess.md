# Texture Pre-processing Optimizations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transpose texture storage to angle-major layout and add per-eye resolution caps so the eagle sclera fits in DRAM, eliminating its PSRAM cache thrashing penalty.

**Architecture:** Two independent changes to `geneye.py` (Python pre-processor): (A) change pixel iteration from row-major to column-major so `T[angle * texH + radius]` replaces `T[radius * texW + angle]`, and (B) expose per-texture `maxTexW`/`maxTexH` overrides in `.eye` JSON with a DRAM budget warning. A matching two-line change in `EyeRenderer.cpp` updates the runtime lookup formula. Headers in `include/eyes/` are auto-generated artifacts — regenerated manually before the build to avoid a two-build cycle.

**Tech Stack:** Python 3 + Pillow (geneye.py), C++17 (EyeRenderer.cpp), PlatformIO (build), ESP32-S3 target

---

> ⚠️ **Build safety note:** Do NOT run `pio run` between Tasks 1–4 and Task 5. The post-build script regenerates headers after compilation, so an intermediate build would compile with new angle-major headers + old row-major lookup (wrong colors). Task 5 pre-generates headers manually *before* the build, ensuring the first build is correct.

---

## File Map

| File | Change |
|------|--------|
| `resources/tools/geneye.py` | Column-major iteration; `max_w`/`max_h` params; `DRAM_BUDGET_BYTES` constant + budget check |
| `resources/eyes/eagle/eagle_466.eye` | Add `"maxTexW": 128, "maxTexH": 64` to sclera section |
| `resources/eyes/eagle/eagle_480.eye` | Same |
| `src/eye/EyeRenderer.cpp` | Two lookup lines: `T[texV*texW+texU]` → `T[texU*texH+texV]` |
| `include/eyes/*.h` | Regenerated artifact — no manual edit |

---

## Task 1: Write verification test for geneye.py texture output

**Files:**
- Create: `resources/tools/test_geneye_texture.py`

- [ ] **Step 1: Create the test file**

```python
#!/usr/bin/env python3
"""Verification tests for geneye.py texture pre-processing changes."""
import sys
from pathlib import Path
import tempfile

sys.path.insert(0, str(Path(__file__).parent))
from geneye import convert_texture_to_rgb565

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"

def test_column_major_output_order():
    """After transpose: col0 pixels come before col1 pixels in the flat array."""
    from PIL import Image
    # 2-column × 3-row image: column 0 = pure red, column 1 = pure blue
    img = Image.new('RGB', (2, 3))
    for y in range(3):
        img.putpixel((0, y), (248, 0, 0))   # red  → RGB565 0xF800
        img.putpixel((1, y), (0, 0, 248))   # blue → RGB565 0x001F
    with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
        tmp = Path(f.name)
    img.save(tmp)
    try:
        data, w, h = convert_texture_to_rgb565(tmp)
        assert w == 2 and h == 3, f"Expected 2x3, got {w}x{h}"
        # Column-major: [col0_r0, col0_r1, col0_r2, col1_r0, col1_r1, col1_r2]
        assert all(v == 0xF800 for v in data[:3]), \
            f"First 3 entries should be col0 (red 0xF800), got {[hex(v) for v in data[:3]]}"
        assert all(v == 0x001F for v in data[3:]), \
            f"Last 3 entries should be col1 (blue 0x001F), got {[hex(v) for v in data[3:]]}"
        print(f"  {PASS}: column-major output order")
    finally:
        tmp.unlink()

def test_max_w_max_h_downscaling():
    """max_w/max_h params cap the output dimensions."""
    from PIL import Image
    img = Image.new('RGB', (300, 200))
    with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
        tmp = Path(f.name)
    img.save(tmp)
    try:
        data, w, h = convert_texture_to_rgb565(tmp, max_w=128, max_h=64)
        assert w <= 128 and h <= 64, \
            f"Expected at most 128x64, got {w}x{h}"
        assert len(data) == w * h, \
            f"Expected {w*h} pixels, got {len(data)}"
        print(f"  {PASS}: max_w/max_h downscaling → {w}x{h}")
    finally:
        tmp.unlink()

def test_default_size_unchanged():
    """No max_w/max_h args: existing behaviour preserved (caps at 256x128)."""
    from PIL import Image
    img = Image.new('RGB', (100, 50))
    with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
        tmp = Path(f.name)
    img.save(tmp)
    try:
        data, w, h = convert_texture_to_rgb565(tmp)
        assert w == 100 and h == 50, \
            f"Small image should be kept at original size, got {w}x{h}"
        assert len(data) == 100 * 50, f"Expected 5000 pixels, got {len(data)}"
        print(f"  {PASS}: default size unchanged for small image")
    finally:
        tmp.unlink()

if __name__ == '__main__':
    print("Running geneye texture tests...")
    test_column_major_output_order()
    test_max_w_max_h_downscaling()
    test_default_size_unchanged()
    print("All tests passed.")
```

- [ ] **Step 2: Run the tests — expect FAIL on column-major test**

```
cd c:\Users\darkg\Documents\GitHub\Uncanny-EyeS3
python resources/tools/test_geneye_texture.py
```

Expected output (before any implementation changes):
```
Running geneye texture tests...
  FAIL: column-major output order
AssertionError: First 3 entries should be col0 (red 0xF800)...
```

The default-size test should pass; the column-major and max_w/max_h tests should fail.

---

## Task 2: Transpose `convert_texture_to_rgb565` to column-major + add size params

**Files:**
- Modify: `resources/tools/geneye.py`

- [ ] **Step 1: Replace `convert_texture_to_rgb565` with the updated version**

In `resources/tools/geneye.py`, find the `convert_texture_to_rgb565` function (currently around line 115) and replace it entirely:

```python
def convert_texture_to_rgb565(
    image_path: Path,
    max_w: int = MAX_TEX_W,
    max_h: int = MAX_TEX_H,
) -> tuple:
    """Convert a PNG to an angle-major RGB565 uint16 list, capped at max_w × max_h.

    Stored column-major: T[angle_index * height + radius_index].
    This matches the render loop access pattern (radius changes faster than angle),
    converting dominant 512-byte stride jumps to sequential 2-byte steps.

    Returns (pixel_list, width, height).
    """
    img = Image.open(image_path).convert('RGB')
    w, h = img.size

    if w > max_w or h > max_h:
        scale = min(max_w / w, max_h / h)
        w = max(1, int(w * scale))
        h = max(1, int(h * scale))
        img = img.resize((w, h), Image.LANCZOS)

    pixels = img.load()
    data = []
    for x in range(w):          # angle is the outer (slow-changing) dimension
        for y in range(h):      # radius is the inner (fast-changing) dimension
            r, g, b = pixels[x, y]
            data.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

    return data, w, h
```

- [ ] **Step 2: Update `_build_texture_array_lines` comment**

Find `_build_texture_array_lines` (around line 139). Change the comment line from:

```python
        f'  // {name} ({width}x{height} px, RGB565 polar map)',
```

to:

```python
        f'  // {name} ({width}x{height} px, RGB565 angle-major: T[angle*{height}+radius])',
```

- [ ] **Step 3: Update `_load_texture` inside `generate_header` to pass size params**

Find the nested `_load_texture` function inside `generate_header` (around line 360). Replace it:

```python
    def _load_texture(filename_key: str, cfg: dict, label: str):
        """Return (data, w, h) or (None, 0, 0) if the file is absent."""
        fname = cfg.get(filename_key)
        if not fname:
            return None, 0, 0
        tex_path = eye_file.parent / fname
        if tex_path.exists():
            print(f"    Loading {label} texture from {fname}")
            max_w = cfg.get('maxTexW', MAX_TEX_W)
            max_h = cfg.get('maxTexH', MAX_TEX_H)
            data, tw, th = convert_texture_to_rgb565(tex_path, max_w, max_h)
            return data, tw, th
        print(f"    Warning: {label} texture not found: {tex_path}")
        return None, 0, 0
```

- [ ] **Step 4: Run the tests — all three should now pass**

```
python resources/tools/test_geneye_texture.py
```

Expected:
```
Running geneye texture tests...
  PASS: column-major output order
  PASS: max_w/max_h downscaling → 128x64
  PASS: default size unchanged for small image
All tests passed.
```

- [ ] **Step 5: Commit**

```
git add resources/tools/geneye.py resources/tools/test_geneye_texture.py
git commit -m "feat: transpose texture to angle-major layout, add per-texture size params"
```

---

## Task 3: Add DRAM budget constant and per-eye budget check

**Files:**
- Modify: `resources/tools/geneye.py`

- [ ] **Step 1: Add the `DRAM_BUDGET_BYTES` constant near the top of the file**

Find the block containing `MAX_TEX_W` and `MAX_TEX_H` (around line 110):

```python
# Maximum texture dimensions to cap PROGMEM usage (~64 KB per texture at 256×128).
MAX_TEX_W = 256
MAX_TEX_H = 128
```

Add the budget constant immediately after:

```python
# Maximum texture dimensions to cap PROGMEM usage (~64 KB per texture at 256×128).
MAX_TEX_W = 256
MAX_TEX_H = 128

# Estimated DRAM available for lookup-table caches at runtime.
# Angle map + radius map = map_radius² × 2 bytes (uint8_t each).
# Textures = (iris_w × iris_h + sclera_w × sclera_h) × 2 bytes (uint16_t).
# EyeRenderer.begin() falls back to PSRAM when DRAM allocation fails, so
# exceeding this triggers a build-time warning rather than a hard error.
DRAM_BUDGET_BYTES = 170 * 1024
```

- [ ] **Step 2: Add the budget check inside `generate_header` after textures are loaded**

In `generate_header`, find the two `_load_texture` calls and the lines immediately after them (around line 373–376):

```python
    iris_tex_data, iris_tex_w, iris_tex_h = _load_texture(
        'filename', iris_cfg, 'iris')
    sclera_tex_data, sclera_tex_w, sclera_tex_h = _load_texture(
        'filename', sclera_cfg, 'sclera')
```

Add the budget check immediately after those four lines:

```python
    iris_tex_data, iris_tex_w, iris_tex_h = _load_texture(
        'filename', iris_cfg, 'iris')
    sclera_tex_data, sclera_tex_w, sclera_tex_h = _load_texture(
        'filename', sclera_cfg, 'sclera')

    # DRAM budget check: warn when combined cache would exceed available DRAM.
    # Maps are uint8_t (1 byte/pixel); textures are uint16_t (2 bytes/pixel).
    map_cache_bytes = map_radius * map_radius * 2   # angle map + radius map
    tex_cache_bytes = (iris_tex_w * iris_tex_h + sclera_tex_w * sclera_tex_h) * 2
    total_cache_bytes = map_cache_bytes + tex_cache_bytes
    if total_cache_bytes > DRAM_BUDGET_BYTES:
        print(f"  Warning: DRAM cache estimate {total_cache_bytes // 1024} KB "
              f"exceeds {DRAM_BUDGET_BYTES // 1024} KB budget — "
              f"texture(s) may fall back to PSRAM")
        print(f"    maps={map_cache_bytes // 1024} KB  "
              f"iris={iris_tex_w * iris_tex_h * 2 // 1024} KB  "
              f"sclera={sclera_tex_w * sclera_tex_h * 2 // 1024} KB")
```

- [ ] **Step 3: Verify the budget warning fires for the unmodified eagle eye**

Run geneye for just the eagle eye (before updating the `.eye` configs):

```
python resources/tools/geneye.py -eye eagle
```

Expected output (warning must appear):
```
Generating header for eagle
    Loading custom eyelids from upper_466.png, lower_466.png
    Loading iris texture from iris.png
    Loading sclera texture from sclera.png
  Warning: DRAM cache estimate 216 KB exceeds 170 KB budget — texture(s) may fall back to PSRAM
    maps=106 KB  iris=46 KB  sclera=64 KB
    Written: include/eyes/eagle_466.h
```

*(Exact KB values may vary slightly due to integer rounding.)*

- [ ] **Step 4: Commit**

```
git add resources/tools/geneye.py
git commit -m "feat: add DRAM budget warning to geneye.py"
```

---

## Task 4: Update eagle `.eye` configs and verify budget check clears

**Files:**
- Modify: `resources/eyes/eagle/eagle_466.eye`
- Modify: `resources/eyes/eagle/eagle_480.eye`

- [ ] **Step 1: Add sclera size cap to `eagle_466.eye`**

In `resources/eyes/eagle/eagle_466.eye`, replace the `"sclera"` section:

```json
    "sclera": {
        "filename": "sclera.png",
        "maxTexW": 128,
        "maxTexH": 64,
        "color": 65535,
        "angle": 0,
        "spin": 0,
        "mirror": false
    },
```

- [ ] **Step 2: Add sclera size cap to `eagle_480.eye`**

In `resources/eyes/eagle/eagle_480.eye`, replace the `"sclera"` section:

```json
    "sclera": {
        "filename": "sclera.png",
        "maxTexW": 128,
        "maxTexH": 64,
        "color": 65535,
        "angle": 0,
        "spin": 0,
        "mirror": false
    },
```

- [ ] **Step 3: Verify budget warning is gone and new sclera dimensions appear**

```
python resources/tools/geneye.py -eye eagle
```

Expected output (no warning, sclera shows 128×64):
```
Generating header for eagle
    Loading custom eyelids from upper_466.png, lower_466.png
    Loading iris texture from iris.png
    Loading sclera texture from sclera.png
    Written: include/eyes/eagle_466.h
    Loading custom eyelids from upper_480.png, lower_480.png
    Loading iris texture from iris.png
    Loading sclera texture from sclera.png
    Written: include/eyes/eagle_480.h
```

No "Warning: DRAM cache" line should appear.

Confirm the new sclera dimensions in the generated header:

```
grep "sclera_texture" include/eyes/eagle_466.h
```

Expected:
```
  // sclera_texture (128x64 px, RGB565 angle-major: T[angle*64+radius])
  const uint16_t sclera_texture[8192] PROGMEM = {
```

*(8192 = 128 × 64)*

- [ ] **Step 4: Commit**

```
git add resources/eyes/eagle/eagle_466.eye resources/eyes/eagle/eagle_480.eye
git commit -m "feat: reduce eagle sclera to 128x64 to fit in DRAM"
```

---

## Task 5: Update `EyeRenderer.cpp` lookup formula + build

**Files:**
- Modify: `src/eye/EyeRenderer.cpp`

> This is the final code change. After it, pre-generate headers then build once.

- [ ] **Step 1: Change the iris texture lookup line**

In `src/eye/EyeRenderer.cpp`, find the iris texture block (search for `irisTexData`). The current line reads:

```cpp
          color = irisTexData[texV * irisTexW + texU];
```

Replace with:

```cpp
          color = irisTexData[texU * irisTexH + texV];
```

The full iris block for context (lines around 420–438) should look like this after the change:

```cpp
        if (hasIrisTex)
        {
          int qx = dx < 0 ? -dx : dx;
          if (qx >= m_mapRadius) qx = m_mapRadius - 1;
          uint8_t ta = angleRow[qx];

          uint8_t fullAngle;
          if      (dx >= 0 && dy > 0) fullAngle = (uint8_t)(128 - (ta >> 1));
          else if (dx <  0 && dy > 0) fullAngle = (uint8_t)(128 + (ta >> 1));
          else if (dx <  0)           fullAngle = (uint8_t)(     - (ta >> 1));
          else                        fullAngle = (uint8_t)(       (ta >> 1));

          int r = radiusRow ? (int)radiusRow[qx] : (int)sqrtf((float)distSq);
          int texU = (uint8_t)(fullAngle + irisRot) * irisTexW / 256;
          int texV = (int)(((uint32_t)r * irisTexVMul) >> 16);
          if (texV >= irisTexH) texV = irisTexH - 1;
          color = irisTexData[texU * irisTexH + texV];
        }
```

- [ ] **Step 2: Change the sclera texture lookup line**

Find the sclera texture block (search for `scleraTexData`). The current line reads:

```cpp
          color = scleraTexData[texV * scleraTexW + texU];
```

Replace with:

```cpp
          color = scleraTexData[texU * scleraTexH + texV];
```

The full sclera block for context (lines around 447–465) should look like this after the change:

```cpp
        if (hasScleraTex)
        {
          int qx = dx < 0 ? -dx : dx;
          if (qx >= m_mapRadius) qx = m_mapRadius - 1;
          uint8_t ta = angleRow[qx];

          uint8_t fullAngle;
          if      (dx >= 0 && dy > 0) fullAngle = (uint8_t)(128 - (ta >> 1));
          else if (dx <  0 && dy > 0) fullAngle = (uint8_t)(128 + (ta >> 1));
          else if (dx <  0)           fullAngle = (uint8_t)(     - (ta >> 1));
          else                        fullAngle = (uint8_t)(       (ta >> 1));

          int r = radiusRow ? (int)radiusRow[qx] : (int)sqrtf((float)distSq);
          int texU = (uint8_t)(fullAngle + scleraRot) * scleraTexW / 256;
          int rv = r - irisRadius;
          if (rv < 0) rv = 0;
          int texV = (int)(((uint32_t)rv * scleraTexVMul) >> 16);
          if (texV >= scleraTexH) texV = scleraTexH - 1;
          color = scleraTexData[texU * scleraTexH + texV];
        }
```

- [ ] **Step 3: Pre-generate all eye headers before building**

The post-build script regenerates headers *after* compilation. Pre-generate now so the build uses the new angle-major layout on the first pass:

```
python resources/tools/geneye.py -all
```

Expected: all eye headers regenerated, no DRAM budget warnings for any eye.

- [ ] **Step 4: Build**

```
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e amoled
```

Expected: `SUCCESS` with no compile errors. The build will also re-run `geneye.py -all` as a post-build step (idempotent).

- [ ] **Step 5: Commit**

```
git add src/eye/EyeRenderer.cpp
git commit -m "feat: update texture lookup to angle-major: T[texU*texH+texV]"
```

---

## Task 6: Flash and verify timing

- [ ] **Step 1: Flash to device**

```
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e amoled --target upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
```

- [ ] **Step 2: Observe eagle eye timing**

Switch to eagle eye with serial command `E1` (or whichever index eagle occupies). Check `[Timing]` output.

Before this change:
```
[EyeRenderer] Sclera texture cached in PSRAM: 65536 bytes
[Timing] render=73000us transfer=20000us total=93000us (~11 FPS)
```

After this change — sclera must be in DRAM and render time must drop:
```
[EyeRenderer] Sclera texture cached in DRAM: 16384 bytes
[Timing] render=~42000us transfer=~20000us total=~62000us (~16 FPS)
```

Key acceptance criteria:
- Log line reads `Sclera texture cached in **DRAM**` (not PSRAM)
- Sclera size is `16384 bytes` (128 × 64 × 2)
- Eagle render time ≤ 50 ms (down from 73 ms)
- Eagle FPS ≥ 14 (up from 11)
- Colors look correct on display (no green/magenta artifacts from wrong byte order — the textures are pre-byte-swapped during `begin()` as before)

- [ ] **Step 3: Confirm default eye is unaffected**

Switch to default eye with `E0`. Expected:
```
[Timing] render=~28000us transfer=~20000us total=~48000us (~20 FPS)
```

Default eye has no textures; render time should be unchanged.
