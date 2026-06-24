# Upside-Down Mounting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a physically upside-down-mounted display be compensated in firmware, settable at runtime per device, with a boot-time default from the SD card config.

**Architecture:** Reuse the existing `DisplayHAL::setRotation(uint8_t)` interface method with `rotation=2` meaning "180°". `AMOLEDDisplay` already handles this correctly via the CO5300's hardware MADCTL register — no change needed there. `TRGBDisplay::setRotation()` gains a stored flag that `directTransfer()` consults to copy the buffer reversed (rows back-to-front, pixels within each row reversed) instead of a straight `memcpy`. A new `U<0|1>` serial command and a new `display.upsideDown` field in `/eyes_config.json` both end up calling the same `setRotation()` path.

**Tech Stack:** C++ (Arduino/ESP32-S3 firmware, PlatformIO), ArduinoJson for SD config parsing. No host-side unit test framework exists in this repo — verification is compile-time (`pio run`) plus manual hardware checks over serial, matching the project's existing testing approach.

## Global Constraints

- `rotation=2` is the only value this feature uses; `rotation=0` is "normal". Do not invent a different encoding.
- Runtime changes are session-only — never write to the SD card from firmware as part of this feature.
- `EyeRenderer`, `EyelidRenderer`, `EyeAnimator`, `eyes.h`, and any generated eye headers must not change for this feature.
- `AMOLEDDisplay.h`/`.cpp` must not change — the existing `setRotation()` call already does the right thing in hardware.
- Verify every task against both build environments: `pio run -e amoled` and `pio run -e trgb` (compile-only; no upload, no hardware required for these checks).

---

### Task 1: SD config — `display.upsideDown` field

**Files:**
- Modify: `src/config/SDConfig.h`
- Modify: `src/config/SDConfig.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `DeviceConfig::displayUpsideDown` (bool, default `false`) — consumed by Task 3 in `main.cpp`.

- [ ] **Step 1: Add the field to `DeviceConfig`**

In `src/config/SDConfig.h`, add a field to the `DeviceConfig` struct (after `allowedMacCount`):

```cpp
struct DeviceConfig
{
  bool loaded;                                           ///< true if a config file was successfully parsed
  int      startEyeIndex;                               ///< Eye index to load at startup (0 = first registered eye)
  uint8_t  networkPmk[16];                              ///< 16-byte ESP-NOW PMK (all-zero = unset)
  uint32_t networkToken;                                 ///< App-layer auth token derived from key (0 = disabled)
  uint8_t  networkChannel;                               ///< ESP-NOW WiFi channel (1-13)
  uint8_t  allowedMacs[SD_CONFIG_MAX_ALLOWED_MACS][6];  ///< Sender MAC allowlist
  int      allowedMacCount;                              ///< Number of entries in allowedMacs (0 = allow all)
  bool     displayUpsideDown;                            ///< Boot-time default: true = display mounted rotated 180°
};
```

- [ ] **Step 2: Document the new config section in the file header comment**

In `src/config/SDConfig.h`, update the `@code` block in the file-level doc comment (the one that currently shows `"eye"` and `"network"` sections) to also show the new section:

```cpp
 * Config file format (/eyes_config.json):
 * @code
 * {
 *   "eye": {
 *     "startIndex": 1
 *   },
 *   "display": {
 *     "upsideDown": false
 *   },
 *   "network": {
 *     "channel": 1,
 *     "key": "SharedPassphrase",
 *     "allowed_macs": [
 *       "AA:BB:CC:DD:EE:FF"
 *     ]
 *   }
 * }
 * @endcode
```

- [ ] **Step 3: Set the default in `applyDefaults()`**

In `src/config/SDConfig.cpp`, in `SDConfig::applyDefaults()`, add the default next to the other scalar defaults:

```cpp
void SDConfig::applyDefaults(DeviceConfig &cfg)
{
  cfg.loaded            = false;
  cfg.startEyeIndex     = 0;
  cfg.networkChannel    = 1;
  cfg.networkToken      = 0;
  cfg.allowedMacCount   = 0;
  cfg.displayUpsideDown = false;
  memset(cfg.networkPmk,   0, sizeof(cfg.networkPmk));
  memset(cfg.allowedMacs,  0, sizeof(cfg.allowedMacs));
}
```

- [ ] **Step 4: Parse the new `display` section in `load()`**

In `src/config/SDConfig.cpp`, in `SDConfig::load()`, add a new section block after the existing `--- eye section ---` block and before `--- network section ---`:

```cpp
  // --- display section ---
  JsonObject disp = doc["display"];
  if (!disp.isNull())
  {
    cfg.displayUpsideDown = disp["upsideDown"] | false;
    Serial.printf("[SDConfig] Display upside-down: %s.\n", cfg.displayUpsideDown ? "true" : "false");
  }

```

- [ ] **Step 5: Compile-check both environments**

Run: `pio run -e amoled`
Expected: `SUCCESS` (no errors/warnings about `DeviceConfig` or `SDConfig`)

Run: `pio run -e trgb`
Expected: `SUCCESS`

- [ ] **Step 6: Commit**

```bash
git add src/config/SDConfig.h src/config/SDConfig.cpp
git commit -m "feat: add display.upsideDown field to SD config"
```

---

### Task 2: T-RGB reversed-copy flip

**Files:**
- Modify: `src/display/TRGBDisplay.h`
- Modify: `src/display/TRGBDisplay.cpp`

**Interfaces:**
- Consumes: nothing new from other tasks.
- Produces: `TRGBDisplay::setRotation(uint8_t)` now actually affects `directTransfer()` output (previously it only affected `gfx`-level draws). No signature changes — `DisplayHAL`'s interface is untouched, so Task 3 calls `s_display->setRotation(...)` exactly as before.

- [ ] **Step 1: Add the `m_rotation180` member**

In `src/display/TRGBDisplay.h`, add a member next to the other state flags:

```cpp
private:
  Arduino_DataBus *bus = nullptr;
  Arduino_ESP32RGBPanel *rgbpanel = nullptr;
  Arduino_GFX *gfx = nullptr;
  int m_width = 480;
  int m_height = 480;
  bool m_initialized = false;
  bool m_rotation180 = false;
```

- [ ] **Step 2: Update `setRotation()` to store the flag**

In `src/display/TRGBDisplay.cpp`, replace the existing `setRotation()` body:

```cpp
/** @brief Set display rotation (0-3); also drives the directTransfer() flip for 180°. */
void TRGBDisplay::setRotation(uint8_t rotation)
{
  m_rotation180 = (rotation == 2);
  if (gfx)
  {
    gfx->setRotation(rotation);
  }
}
```

- [ ] **Step 3: Update `directTransfer()` to branch on the flag**

In `src/display/TRGBDisplay.cpp`, replace the existing `directTransfer()` body:

```cpp
void TRGBDisplay::directTransfer(uint16_t *buffer, int destX, int destY,
                                 int srcX, int srcY, int srcW, int srcH)
{
  if (!gfx || !buffer)
    return;

  // Cast to Arduino_RGB_Display to access getFramebuffer()
  Arduino_RGB_Display *rgbDisplay = static_cast<Arduino_RGB_Display *>(gfx);
  uint16_t *fb = rgbDisplay->getFramebuffer();
  if (!fb)
    return;

  int fbWidth = gfx->width();

  if (!m_rotation180)
  {
    for (int row = 0; row < srcH; row++)
    {
      uint16_t *srcRow = buffer + (srcY + row) * fbWidth + srcX;
      uint16_t *dstRow = fb + (destY + row) * fbWidth + destX;
      memcpy(dstRow, srcRow, srcW * sizeof(uint16_t));
    }
    return;
  }

  // 180° mount compensation: the destination position is mirrored through the
  // center of the *whole* physical panel (m_width x m_height), not just the
  // sub-rect being transferred, since the orientation is a property of how the
  // panel itself is mounted.
  for (int row = 0; row < srcH; row++)
  {
    uint16_t *srcRow = buffer + (srcY + row) * fbWidth + srcX;
    int dstRowIdx = (m_height - 1) - (destY + row);
    uint16_t *dstRow = fb + dstRowIdx * fbWidth;
    for (int col = 0; col < srcW; col++)
    {
      int dstColIdx = (m_width - 1) - (destX + col);
      dstRow[dstColIdx] = srcRow[col];
    }
  }
}
```

- [ ] **Step 4: Compile-check both environments**

Run: `pio run -e amoled`
Expected: `SUCCESS`

Run: `pio run -e trgb`
Expected: `SUCCESS`

- [ ] **Step 5: Commit**

```bash
git add src/display/TRGBDisplay.h src/display/TRGBDisplay.cpp
git commit -m "feat: flip directTransfer output 180° on T-RGB when rotation=2"
```

---

### Task 3: Runtime command + boot-time default wiring

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `DeviceConfig::displayUpsideDown` (Task 1), `DisplayHAL::setRotation(uint8_t)` (existing interface; behavior fixed for T-RGB by Task 2).
- Produces: nothing consumed by later tasks — this is the last task.

- [ ] **Step 1: Apply the boot-time default after display init**

In `src/main.cpp`, in `setup()`, right after the existing `setupDisplay();` call (currently followed by `setupNetwork();`), add:

```cpp
  setupDisplay();

  if (s_display)
  {
    s_display->setRotation(s_deviceConfig.displayUpsideDown ? 2 : 0);
  }

  setupNetwork();
```

- [ ] **Step 2: Add the `U<0|1>` serial command**

In `src/main.cpp`, in the serial command loop (the `while (Serial.available())` block that currently only handles `'E'`), add a new `else if` branch:

```cpp
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == 'E')
    {
      int eyeIndex = Serial.parseInt();
      if (eyeIndex >= 0 && eyeIndex < s_eyeCount)
      {
        switchEye(eyeIndex);
      }
      else
      {
        Serial.printf("Invalid eye index. Available: 0-%d\n", s_eyeCount - 1);
      }
    }
    else if (c == 'U')
    {
      int flag = Serial.parseInt();
      if (s_display)
      {
        s_display->setRotation(flag ? 2 : 0);
        Serial.printf("Display %s.\n", flag ? "upside-down" : "normal");
      }
    }
  }
```

- [ ] **Step 3: Compile-check both environments**

Run: `pio run -e amoled`
Expected: `SUCCESS`

Run: `pio run -e trgb`
Expected: `SUCCESS`

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: add U<0|1> serial command and boot-time default for display orientation"
```

- [ ] **Step 5: Manual hardware verification (perform on actual boards — not automatable)**

On AMOLED hardware:
1. Flash and open the serial monitor.
2. Send `U1`. Confirm the eye (and debug overlay, if `DEBUG_OVERLAY_ENABLED` is on) appears rotated 180°.
3. Send `U0`. Confirm it returns to normal.
4. Confirm FPS is unaffected (check `DEBUG_FPS_ENABLED` output before/after) — expected, since this is a hardware-only register write on this board.

On T-RGB hardware:
1. Flash and open the serial monitor.
2. Send `U1`. Confirm the eye appears rotated 180° and the debug overlay (if enabled) is also correctly repositioned.
3. Send `U0`. Confirm restoration.
4. If `DEBUG_TIMING_ENABLED` is on, compare `directTransfer` duration with `U0` vs `U1` to confirm the reversed-copy cost is negligible relative to total frame time.

On either board, with an SD card present:
1. Write `/eyes_config.json` containing `{"display": {"upsideDown": true}}`.
2. Power-cycle. Confirm the device boots already flipped.
3. Send `U0` over serial. Confirm it overrides for the session.
4. Power-cycle again (without touching the SD card). Confirm it boots flipped again — the SD card was never rewritten.

---

## Self-Review Notes

- **Spec coverage:** AMOLED hardware path (no task needed, confirmed in Global Constraints and Task descriptions), T-RGB reversed copy (Task 2), runtime `U<0|1>` command (Task 3 Step 2), boot-time `display.upsideDown` default (Task 1 + Task 3 Step 1), session-only persistence (no SD write code anywhere in this plan — verified by omission), debug overlay correctness on T-RGB (handled automatically since `setRotation()` still forwards to `gfx->setRotation()` in Task 2 Step 2).
- **Placeholder scan:** none found — every step has literal, complete code.
- **Type consistency:** `DeviceConfig::displayUpsideDown` (Task 1) matches the exact name used in Task 3 Step 1 (`s_deviceConfig.displayUpsideDown`). `setRotation(uint8_t)` signature is unchanged from the existing `DisplayHAL` interface throughout.
