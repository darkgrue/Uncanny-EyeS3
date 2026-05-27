# Uncanny-EyeS3

A port of the [Adafruit M4 Eyes](https://github.com/adafruit/Adafruit_Learning_System_Guides/tree/main/M4_Eyes) project to the LilyGo T-Display S3 AMOLED and T-RGB, with ESP32-specific rendering optimizations, WiiChuck puppeteering, light-driven pupil control, Gesture & Face Detection Sensor support for gaze direction, and multi-device synchronization over ESP-NOW.

---

## Supported Hardware

| Board                      | Display                | Controller | Resolution |
| -------------------------- | ---------------------- | ---------- | ---------- |
| LilyGo T-Display S3 AMOLED | CO5300 (QSPI @ 80 MHz) | ESP32-S3   | 466×466    |
| LilyGo T-RGB               | ST7701S (DPI)          | ESP32-S3   | 480×480    |

**Optional peripherals:**

- Nintendo Wii Nunchuk (WiiChuck) — I2C puppeteering controller
- LDR photoresistor — ambient light-driven pupil dilation
- DFRobot Gravity Gesture & Face Detection Sensor — AI face tracking for eye targeting
- MicroSD card — boot-time configuration via `/eyes_config.json` (ESP-NOW channel, security key, MAC allowlist)
- SY6970 — battery fuel gauge / charger (on-board, auto-initialized)

---

## Features

- **Autonomous animation** — saccadic eye movement with a lognormal amplitude distribution, centering bias, and sigmoid velocity easing for naturalistic motion
- **Realistic blinking** — three-phase FSM (close → pause → open) with randomized timing and burst probability
- **WiiChuck puppeteering** — joystick-driven eye targeting with edge-triggered blink/boop and hold commands for close/wide expressions
- **Face tracking** — AI gesture & face sensor directs gaze to the nearest detected face, overriding autonomous movement while yielding to WiiChuck joystick
- **Light sensor pupil control** — photoresistor drives pupil dilation; falls back to autonomous iris animation when not connected
- **Multi-eye ESP-NOW sync** — one device acts as controller, others follow with interpolated state at up to ~120 FPS; optional shared-key authentication prevents unauthorized devices from joining
- **SD card configuration** — optional `/eyes_config.json` at boot sets ESP-NOW channel, shared key, and MAC allowlist; firmware runs with safe defaults when no card is present
- **Runtime eye switching** — switch between registered eye designs over serial without reflashing
- **Double-buffered PSRAM rendering** — two RGB565 frame buffers with dirty-region tracking minimize display transfer overhead
- **User extension hooks** — `user_setup()` / `user_loop()` for custom sensor integrations

---

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run -e amoled          # T-Display S3 AMOLED (466×466)
pio run -e trgb            # T-RGB (480×480)

# Build and upload
pio run -e amoled --target upload
pio run -e trgb --target upload

# Serial monitor (115200 baud)
pio device monitor
```

---

## How It Works

### Rendering Pipeline

The firmware runs two FreeRTOS tasks on separate cores:

- **Core 1 — render task (~120 FPS):** `EyeAnimator::update()` advances the movement, blink, and iris state machines each frame. When `needsRender()` is true, `EyeRenderer::renderFrame()` draws the eye into the current PSRAM render buffer column by column using precomputed polar and displacement lookup tables. Dirty region tracking limits each display transfer to only the pixels that changed.
- **Core 0 — Arduino `loop()`:** Handles serial commands and prints a status line every 5 seconds.

### Eye Rendering

Each eye is defined by a precomputed set of lookup tables baked into a C++ header by `tablegen.py`:

- **Polar angle / distance maps** — map every pixel in the display circle to a polar coordinate in texture space
- **Spherical displacement map** — simulates the curvature of an eyeball
- **Eyelid tables** (optional) — per-column upper/lower eyelid Y positions for custom lid shapes

At runtime, `EyeRenderer` uses these tables to look up the correct iris, sclera, or pupil color for each pixel in O(1) per pixel. The circular clipping bounds are precomputed once into per-scanline start/end X values, avoiding `sqrt()` per pixel.

### Eye Movement

`EyeMovement` generates saccades using a lognormal amplitude distribution (matching natural human eye statistics), applies a centering bias after peripheral movements, and eases each movement with a sigmoid curve. Between saccades a configurable fixation pause (default 4–8 s) holds the eye still. When a WiiChuck is attached, direct joystick input overrides the autonomous mode.

### Multi-Eye Sync (ESP-NOW)

Any number of devices running the same firmware can synchronize over ESP-NOW:

1. The device with an active WiiChuck (joystick moved beyond the dead zone) becomes the controller.
2. The controller broadcasts an `EyeSyncMessage` each frame — eye X/Y, pupil factor, blink state, and expression command.
3. Follower devices receive the message and apply it via `EyeInterpolator`, which performs linear interpolation between received states for smooth local playback.
4. If no controller message is received within 100 ms, followers return to autonomous animation.

If no WiiChuck is connected to any device, all units run independently in autonomous mode.

ESP-NOW channel and security (shared key, MAC allowlist) are configured via the SD card config file; see [SD Card Configuration](#sd-card-configuration).

---

## Eye Configuration

### File Layout

Eye definitions live in `resources/eyes/`. Each eye has its own subdirectory:

```text
resources/eyes/
  default_eye/
    default_eye_466.eye    # AMOLED config
    default_eye_480.eye    # T-RGB config
    iris.png               # Optional iris texture
    sclera.png             # Optional sclera texture
    upper_lid.png          # Optional custom eyelid shape (grayscale)
    lower_lid.png
```

### Generating C++ Headers

Run `tablegen.py` to generate C++ header files with precomputed polar maps and displacement maps. These tables are shared across all eyes for a given display size. Header files are placed in `include/display/`.

```bash
# Regenerate for a single display type
python tablegen.py [display_type]

# Regenerate all display types
python tablegen.py -all
```

`display_type` is `default` (240x240), `amoled` (466x466), or `trgb` (480x480).

Output headers are named `display_240.h` (default), `display_466.h` (AMOLED), and `display_480.h` (T-RGB).

### `.eye` Config Format

All size values are fractions (0.0–1.0) of the display's smaller dimension, so a single config works across display sizes.

```json
{
    "name": "default_eye",
    "radiusFraction": 0.5,
    "backColor": 0,
    "squint": 0,
    "pupil": {
        "color": 0,
        "slitRadius": 0,
        "minFraction": 0.35,
        "maxFraction": 1.67
    },
    "iris": {
        "radiusFraction": 0.5,
        "color": 65281,
        "angle": 0,
        "spin": 0,
        "iSpin": 0,
        "mirror": false
    },
    "sclera": {
        "color": 65535,
        "angle": 0,
        "spin": 0,
        "iSpin": 0,
        "mirror": false
    },
    "eyelid": {
        "color": 0,
        "normalClosure": 0.15,
        "tracking": true
    }
}
```

| Field                               | Description                                                                        |
| ----------------------------------- | ---------------------------------------------------------------------------------- |
| `radiusFraction`                    | Eye radius as fraction of the smaller screen dimension                             |
| `backColor`                         | Background color behind the eye (RGB565)                                           |
| `pupil.slitRadius`                  | `0.0` = round; fraction of iris radius for slit vertical half-height (e.g. `0.75`) |
| `pupil.minFraction` / `maxFraction` | Pupil size range as fraction of iris radius                                        |
| `iris.radiusFraction`               | Iris radius as fraction of eye radius                                              |
| `iris.spin` / `iSpin`               | Continuous spin / fixed per-frame spin override                                    |
| `iris.filename`                     | Optional PNG/BMP texture (relative path, auto-converted to RGB565)                 |
| `sclera.filename`                   | Optional sclera texture                                                            |
| `eyelid.upperFilename`              | Optional custom lid images (upper, lower; must match display)                      |
| `eyelid.normalClosure`              | Eyelid coverage fraction at rest (0.0–1.0). Default: `0.0`                         |
| `eyelid.tracking`                   | Eyelids track pupil vertical position. Default: `true`                             |

`eyelid.normalClosure` sets how much the lids close over the eye in the resting-open position. A value of `0.15` means the lids cover 15 % of the eye radius at rest. `eyesWide()` bypasses this offset and retracts the lids fully to 1.0, making the expression visually distinct from the normal resting gap. The supplied eye definitions use `0.15` (default\_eye), `0.20` (human\_eye), and `0.05` (eagle).

### Adding a New Eye

1. Create `resources/eyes/<name>/` and write `<name>_466.eye` and `<name>_480.eye`.

2. Generate headers:

   ```bash
   python geneye.py -eye <eye_name>    Generate header for specific eye
   python geneye.py -all               Generate headers for all eyes
   python geneye.py -list              List available eyes
   ```

3. Register in `include/EyeLibrary.h` under both board sections:

   ```cpp
   #include "eyes/<name>_466.h"   // in the AMOLED block
   // add &<name>::eye to s_eyeRegistry[] and increment s_eyeCount
   ```

4. Build and test: `pio run -e amoled` / `pio run -e trgb`.

---

## Runtime Eye Switching

Switch the active eye over serial without reflashing:

```text
E0    → switch to eye index 0 (default_eye)
E1    → switch to eye index 1 (eagle)
E2    → switch to eye index 2 (human_eye)
```

The command is parsed in `loop()` and calls `EyeAnimator::setEyeIndex()`, which reinitializes the renderer with the new eye definition.

---

## WiiChuck Controller

### Wiring

| WiiChuck Pin | T-Display S3 AMOLED | T-RGB   |
| ------------ | ------------------- | ------- |
| Data (SDA)   | GPIO 7              | GPIO 48 |
| Clock (SCL)  | GPIO 6              | GPIO 8  |
| 3.3V         | 3.3V                | 3.3V    |
| GND          | GND                 | GND     |

I2C address: `0x52`, bus speed: 400 kHz.

### Controls

| Input               | Behavior                                                       |
| ------------------- | -------------------------------------------------------------- |
| Joystick (active)   | Direct eye position; overrides autonomous movement             |
| Joystick (centered) | Returns to autonomous wandering after current saccade finishes |
| Z press             | Single blink (edge-triggered)                                  |
| C hold              | Eyes go wide — eyelids retract beyond the normal resting gap   |
| C + Z (chord)       | Boop expression (fires when the second button lands)           |

Buttons work independently of the joystick: you can blink or go wide while the joystick is centred.

Joystick values are raw 0–255 with center at 128. A ±10 dead zone prevents drift. Control returns to autonomous mode when the joystick is centered.

The WiiChuck is optional — the firmware prints a warning at startup if not found and continues with autonomous animation.

---

## Light Sensor (Pupil Control)

Connect an LDR voltage divider to GPIO 5 (`LIGHT_PIN`). The sensor is auto-detected at startup by sampling for minimum variance; if not found, autonomous iris animation is used instead.

When connected:

- Bright light → constricted pupil
- Dim light → dilated pupil
- Polled at 10 Hz with configurable min/max calibration and power curve

---

## Gesture & Face Detection Sensor

The [DFRobot Gravity Offline Edge AI Gesture & Face Detection](https://www.dfrobot.com/product-2914.html) sensor connects to the same I2C bus as the WiiChuck (see [WiiChuck Controller](#wiichuck-controller) for pin assignments). It is auto-detected at startup; if not found, the firmware continues without face tracking.

I2C address: `0x72`.

When at least one face is detected with a confidence score at or above the minimum threshold (default 50 out of 100), the eye tracks the nearest face. When no face is visible the eye returns to autonomous random movement.

Face position is reported in camera pixel coordinates. The default normalization assumes a 320×240 frame; pass custom width/height values to the `GestureFaceInput` constructor if the sensor reports a different resolution. Call `setMinScore()` to adjust the confidence threshold.

---

## SD Card Configuration

An optional MicroSD card provides boot-time configuration without reflashing. The card is read once at startup and then released — it is not required while the firmware is running.

### Pinout

Both boards use the same dedicated SPI pins for the SD card:

| SD Pin | GPIO |
| ------ | ---- |
| CS     | 38   |
| MOSI   | 39   |
| MISO   | 40   |
| SCLK   | 41   |

### Config File

Place `/eyes_config.json` in the root directory of the card:

```json
{
  "network": {
    "channel": 1,
    "key": "SharedPassphrase",
    "allowed_macs": [
      "AA:BB:CC:DD:EE:FF",
      "11:22:33:44:55:66"
    ]
  }
}
```

| Field                  | Type   | Default  | Description                                                             |
| ---------------------- | ------ | -------- | ----------------------------------------------------------------------- |
| `network.channel`      | int    | `1`      | ESP-NOW WiFi channel (1–13); must match across all synchronized devices |
| `network.key`          | string | _(none)_ | Shared passphrase or 32-hex-digit key; omit to disable authentication  |
| `network.allowed_macs` | array  | _(none)_ | MAC allowlist; omit to accept any device with the correct key           |

### Key Formats

The `key` field accepts two formats:

- **Passphrase** — any string up to 16 characters, zero-padded internally to 16 bytes.
- **Hex key** — exactly 32 hexadecimal characters (e.g. `"0123456789ABCDEF0123456789ABCDEF"`), decoded directly to 16 raw bytes.

Either format can be used interchangeably as long as the resulting 16-byte value is identical on all devices.

### Security Model

When a `key` is configured, the firmware derives a 32-bit token from it and stamps every outgoing ESP-NOW packet. Incoming packets are silently discarded if:

- The token does not match (sender has a different or absent key).
- An `allowed_macs` list is configured and the sender MAC is not in it.

Devices sharing the same key communicate normally. Devices with different keys — or no key — silently ignore each other without producing errors. If no key is configured, any device on the channel is accepted.

**All devices in a synchronized group must use matching keys.**

The `allowed_macs` list provides an additional layer of access control independent of the key. If both are configured, a packet must pass both checks.

### Graceful Degradation

The firmware starts normally regardless of SD card state:

- No SD card inserted
- Card present but `/eyes_config.json` missing
- Config file present but malformed JSON

In all cases the defaults are used: channel 1, no authentication, all senders accepted. A message is printed to serial indicating which condition was detected.

---

## Input Priority

### Movement Priority

The first active source in the following order controls eye position each frame:

1. **WiiChuck joystick** — when the joystick is tilted beyond the ±10 dead zone, it takes exclusive control and suppresses all other movement sources.
2. **Gesture & face detection** — when a face is detected with sufficient confidence, the eye tracks the face. Control is released immediately when no face is visible.
3. **ESP-NOW network controller** — a paired controller device broadcasts its state; followers apply it when data is fresher than 100 ms.
4. **Autonomous random saccades** — lognormal amplitude distribution with centering bias and sigmoid easing; the default when no input is active.

### Pupil Sizing Priority

Pupil size is driven by whichever source is available:

1. **Light sensor (LDR)** — maps ambient light to pupil dilation at 10 Hz when connected.
2. **Autonomous iris animation** — simulates natural pupillary unrest using lognormal targets and smoothstep easing when no light sensor is present.

Face detection and ESP-NOW do not affect pupil size.

---

## User Extension Hooks

Implement `user_setup()` and `user_loop()` in `src/user/user_hooks.cpp` to add custom behavior without modifying core code. `user_loop()` runs during the render task — keep it short.

Available expression calls from either hook:

```cpp
eyesBlink();    // trigger a single blink
eyesBoop();     // trigger a boop expression
eyesClose();    // hold eyelids closed
eyesWide();     // hold eyelids wide
eyesNormal();   // return to normal tracking
```

---

## Project Structure

```text
src/
  main.cpp                  # Entry point, FreeRTOS task setup
  eye/
    EyeAnimator             # Central animation state machine
    EyeRenderer             # Per-pixel polar-map rendering, double-buffered PSRAM
    EyelidRenderer          # Eyelid shape rendering (procedural or custom)
    EyeConfig.h             # Runtime configuration struct
  animation/
    EyeMovement             # Saccadic movement with lognormal distribution
    BlinkFSM                # Three-phase blink state machine
  display/
    AMOLEDDisplay           # CO5300 QSPI driver (466×466)
    TRGBDisplay             # ST7701S DPI driver (480×480)
  input/
    WiiChuck                # Wii Nunchuk I2C driver
    LightSensor             # LDR photoresistor pupil control
    GestureFaceInput        # DFRobot Gravity Gesture & Face Detection sensor
  network/
    EyeSync                 # ESP-NOW controller/follower synchronization
  config/
    SDConfig                # SD card boot-time configuration reader
  common/
    DisplayHAL.h            # Abstract display interface
    EyeState.h              # Shared state structs and enums
    DisplayGeometry.h       # Polar maps, circular clipping
  debug/
    DebugOverlay            # FPS + battery HUD (build flag: DEBUG_OVERLAY_ENABLED)
  user/
    user_hooks.cpp          # User extension template

include/
  eyes.h                    # EyeDefinition struct and helper macros
  EyeLibrary.h              # Eye registry (s_eyeRegistry[], s_eyeCount)
  BoardPins.h               # GPIO pin definitions per board
  eyes/
    *_466.h                 # Generated AMOLED eye headers
    *_480.h                 # Generated T-RGB eye headers

resources/eyes/
  tablegen.py               # Eye header generator
  <eye_name>/
    <eye_name>_466.eye      # AMOLED eye config
    <eye_name>_480.eye      # T-RGB eye config
    *.png / *.bmp           # Optional textures and eyelid images
```

---

## Build Flags

Defined in `platformio.ini` under `[esp32base]`:

| Flag                    | Purpose                             |
| ----------------------- | ----------------------------------- |
| `FDEBUG`                | Enable debug serial output          |
| `DEBUG_OVERLAY_ENABLED` | Show FPS/battery HUD on display     |
| `CORE_DEBUG_LEVEL=3`    | ESP-IDF log verbosity (info)        |
| `BUILDVER=0.0.1`        | Embedded firmware version string    |
