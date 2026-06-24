# Upside-Down Mounting Design

**Date:** 2026-06-24
**Status:** Approved
**Scope:** `DisplayHAL`, `AMOLEDDisplay`, `TRGBDisplay`, `SDConfig`, `EyeConfig`, `main.cpp`

---

## Problem

Some physical builds need a display mounted rotated 180° relative to its native
orientation (e.g. cable routing constraints in the housing). The firmware needs a
way to compensate for this — flipping the displayed image 180° — without adding
per-pixel cost to the 120 FPS render/transfer hot path.

The setting must be:
- Per physical display (a property of how *this device* is mounted, not of which
  eye graphic or eye index is currently loaded).
- Settable at runtime via serial command, taking effect immediately.
- Configurable with a boot-time default via `/eyes_config.json` on the SD card.
- Session-only — runtime changes do not persist back to the SD card.

---

## Solution: Flip in the display layer, not the renderer

`EyeRenderer` always renders in standard top-left-origin orientation — unchanged.
Each `DisplayHAL` implementation is already responsible for getting that buffer onto
its physical panel; the 180° compensation belongs there; the two backends need
different mechanisms because their hardware differs.

`DisplayHAL::setRotation(uint8_t rotation)` already exists in the interface
([DisplayHAL.h:54](../../../src/common/DisplayHAL.h)). `rotation = 2` is reused as
the "180°" value on both backends, consistent with the existing Arduino_GFX
convention. No new interface method is added.

### AMOLED (CO5300, QSPI) — already correct, no code change

`AMOLEDDisplay::setRotation()` ([AMOLEDDisplay.cpp:247-254](../../../src/display/AMOLEDDisplay.cpp))
calls `m_gfx->setRotation(rotation)`. For the CO5300, `Arduino_CO5300::setRotation(2)`
writes the controller's MADCTL register with both X- and Y-axis flip bits
([Arduino_CO5300.cpp:41-62](../../../.pio/libdeps/amoled/GFX%20Library%20for%20Arduino/src/display/Arduino_CO5300.cpp)).
This is a single register write — the display controller itself flips the image as
it scans into the physical panel. Because `EyeRenderer`'s async transfer always blits
the full `0,0 → displaySize,displaySize` buffer ([EyeRenderer.cpp:415](../../../src/eye/EyeRenderer.cpp)),
one register write at orientation-change time reorients everything (eye content and
debug overlay) with zero ongoing per-frame cost. `_xStart`/`_yStart` offset handling
for rotation 2 resolves to `(COL_OFFSET2, ROW_OFFSET2)` = `(0, 0)` for this panel's
constructor parameters, so no addressing-window correction is needed.

### T-RGB (ST7701S, DPI) — flip applied during the existing per-row copy

`TRGBDisplay::setRotation()` ([TRGBDisplay.cpp:128-134](../../../src/display/TRGBDisplay.cpp))
currently only forwards to `gfx->setRotation()`, which sets a software variable in
the base `Arduino_GFX` class — it does **not** send any MADCTL-equivalent command
(`Arduino_RGB_Display` does not override `setRotation`), and it has no effect on
`directTransfer`'s raw `memcpy` into the PSRAM framebuffer.

Change `TRGBDisplay::setRotation()` to:
1. Keep calling `gfx->setRotation(rotation)` — this keeps the debug overlay
   (`fillRect`/`drawString`, which go through normal Arduino_GFX draw calls) correctly
   placed after a flip. Low frequency; cost is irrelevant.
2. Store `m_rotation180 = (rotation == 2)` on the `TRGBDisplay` instance.

Change `TRGBDisplay::directTransfer()` ([TRGBDisplay.cpp:190-209](../../../src/display/TRGBDisplay.cpp))
to branch on `m_rotation180`:
- **Normal (false):** unchanged — per-row `memcpy`.
- **Flipped (true):** iterate source rows back-to-front (`srcH-1` down to `0`) when
  computing the destination row — free, same total `memcpy` byte count, just a
  different row-to-row mapping — and within each row, copy pixels in reverse order
  (`dstRow[w-1-i] = srcRow[i]`). This is the one place that adds real per-frame cost:
  a per-pixel copy loop instead of a single `memcpy` for each row, on a ≤480-pixel
  row, once per frame. Negligible next to the per-pixel rendering cost already paid
  earlier in the same frame, and it does not touch `EyeRenderer`'s hot loop at all.

---

## Runtime control

Add a new single-character serial command to the existing dispatcher in
`main.cpp` ([main.cpp:677-692](../../../src/main.cpp)), alongside the existing `E<n>`
eye-switch command:

```cpp
else if (c == 'U')
{
  int flag = Serial.parseInt();
  s_display->setRotation(flag ? 2 : 0);
  Serial.printf("Display %s\n", flag ? "upside-down" : "normal");
}
```

`U1` sets upside-down, `U0` restores normal. Takes effect immediately. This is
session-only — it does not write back to the SD card.

---

## Boot-time default: `/eyes_config.json`

Add a new top-level `display` section (separate from the existing `eye` section,
since this is a property of the physical device, not of the eye graphic or index):

```json
{
  "display": {
    "upsideDown": false
  }
}
```

**`SDConfig` changes:**
- `DeviceConfig` ([SDConfig.h:47-56](../../../src/config/SDConfig.h)) gains
  `bool displayUpsideDown;` (default `false`, set in `applyDefaults()`).
- `SDConfig::load()` ([SDConfig.cpp:25-112](../../../src/config/SDConfig.cpp)) gains a
  `display` section parse block, mirroring the existing `eye`/`network` block style:
  ```cpp
  JsonObject disp = doc["display"];
  if (!disp.isNull())
  {
    cfg.displayUpsideDown = disp["upsideDown"] | false;
  }
  ```

**Applying the default:** in `main.cpp` setup, after `s_display` is assigned and
`begin()` has run, call:
```cpp
s_display->setRotation(s_config.displayUpsideDown ? 2 : 0);
```
This is the same code path as the runtime `U` command, so behavior is identical
whether the orientation is set at boot or live.

`EyeProjectConfig` ([EyeConfig.h](../../../src/eye/EyeConfig.h)) is not changed —
orientation is consumed directly from `DeviceConfig` into `DisplayHAL`, not routed
through the animator/renderer config struct, since `EyeRenderer` is intentionally
untouched by this feature.

---

## Files Changed

| File | Nature of change |
|------|-------------------|
| `src/display/TRGBDisplay.h` / `.cpp` | `setRotation()` stores `m_rotation180`; `directTransfer()` branches to a reversed row+pixel copy when flipped |
| `src/display/AMOLEDDisplay.h` / `.cpp` | No change — existing `setRotation()` already sufficient |
| `src/common/DisplayHAL.h` | No change — reuses existing `setRotation(uint8_t)` |
| `src/config/SDConfig.h` / `.cpp` | New `DeviceConfig::displayUpsideDown` field; new `display` JSON section parsing |
| `src/main.cpp` | New `U<0|1>` serial command; apply `s_config.displayUpsideDown` to `s_display` once at startup |

No changes to `EyeRenderer`, `EyelidRenderer`, `EyeAnimator`, `eyes.h`, or any
generated eye headers — this feature is entirely contained in the display/config
layer.

---

## Testing

- **AMOLED:** send `U1` over serial, confirm the eye and debug overlay (if enabled)
  both appear rotated 180°; send `U0`, confirm restoration. Confirm FPS is unaffected
  (expected — hardware-only change).
- **T-RGB:** same `U1`/`U0` check. Measure `directTransfer` duration before/after to
  confirm the reversed-copy cost is negligible relative to total frame time.
- **SD config:** set `"display": {"upsideDown": true}` in `/eyes_config.json`, confirm
  boot starts flipped; confirm a runtime `U0` overrides it for the session without
  rewriting the file.
- **No SD card / missing `display` section:** confirm default is normal orientation
  (false), matching existing `applyDefaults()` behavior for absent sections.
