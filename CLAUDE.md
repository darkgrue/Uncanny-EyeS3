# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Uncanny-EyeS3 is a port of the Adafruit M4 Eyes project to the LilyGo T-Display S3 AMOLED (466×466, CO5300 via QSPI) and T-RGB (480×480, ST7701S via DPI). It renders animated eyes on round displays with autonomous saccadic movement, blinking, WiiChuck puppeteering, light-driven pupil dilation, and multi-device synchronization over ESP-NOW.

## Build Commands

```bash
# Build
pio run -e amoled        # T-Display S3 AMOLED (466x466)
pio run -e trgb          # T-RGB (480x480)

# Upload
pio run -e amoled --target upload
pio run -e trgb --target upload

# Serial monitor (115200 baud)
pio device monitor

# Generate eye headers from .eye configs (required when adding/modifying eyes)
python resources/eyes/tablegen.py include/ --all amoled
python resources/eyes/tablegen.py include/ --all trgb
```

## Architecture

### Execution Model

The firmware runs on two FreeRTOS cores:
- **Core 1** (`renderLoopTask`): Eye animation at ~120 FPS. Calls `EyeAnimator::update()`, polls the light sensor, broadcasts ESP-NOW state, and calls `EyeRenderer::renderFrame()` when dirty.
- **Core 0** (Arduino `loop()`): Lightweight status reporting every 5 seconds and serial command handling (`E<n>` to switch eye index).

### Subsystem Layers

```
main.cpp
├── EyeAnimator          — central state machine (eye/EyeAnimator.h)
│   ├── EyeRenderer      — per-pixel polar-map rendering, double-buffered PSRAM (eye/EyeRenderer.h)
│   │   └── EyelidRenderer — eyelid shape rendering (eye/EyelidRenderer.h)
│   ├── EyeMovement      — saccadic movement, lognormal distribution (animation/EyeMovement.h)
│   ├── BlinkFSM         — blink state machine NOBLINK/ENBLINK/DEBLINK (animation/BlinkFSM.h)
│   ├── InputBase        — abstract input interface (input/InputBase.h)
│   │   └── WiiChuckInput — Wii Nunchuk I2C driver (input/WiiChuck.h)
│   ├── LightSensor      — photoresistor pupil control (input/LightSensor.h)
│   └── EyeSyncManager   — ESP-NOW controller/follower sync (network/EyeSync.h)
├── DisplayHAL           — abstract display interface (common/DisplayHAL.h)
│   ├── AMOLEDDisplay    — CO5300 QSPI driver (display/AMOLEDDisplay.h)
│   └── TRGBDisplay      — ST7701S DPI driver (display/TRGBDisplay.h)
└── DebugOverlay         — FPS/battery HUD (debug/DebugOverlay.h)
```

### Eye Definition Pipeline

Eye definitions go through a two-step process before use:

1. **JSON config** (`.eye` files in `resources/eyes/<name>/`): Fractional values (0.0–1.0) relative to display size. Texture/eyelid images referenced as relative paths in the same directory.

2. **Code generation** (`tablegen.py`): Reads `.eye` and images, computes polar angle/distance maps and displacement tables, outputs `include/eyes/<name>_466.h` (AMOLED) and `include/eyes/<name>_480.h` (T-RGB). These headers define an `EyeDefinition` struct in a namespace matching the eye name.

3. **Registration** (`include/EyeLibrary.h`): `#include`s the generated headers and adds them to `s_eyeRegistry[]`. Board selection is done with `#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)`.

4. **Runtime switching**: Serial command `E<n>` calls `switchEye(n)` → `EyeAnimator::setEyeIndex()`. Currently registered eyes: `default_eye`, `eagle`, `human_eye`.

### Key Data Structures

- **`EyeDefinition`** (`include/eyes.h`): Top-level eye descriptor — contains `PupilConfig`, `IrisConfig`, `ScleraConfig`, `EyelidConfig`, `PolarMapInfo`, and a `dispMap` pointer to the spherical displacement table.
- **`EyeSyncMessage`** (`common/EyeState.h`): ESP-NOW payload — eye X/Y, pupil factor, blink state, timestamp, and expression command.
- **`EyeProjectConfig`** (`eye/EyeConfig.h`): Runtime config struct passed during initialization.

### Multi-Eye Sync (ESP-NOW)

The first device with an active WiiChuck input (joystick beyond dead zone) becomes the controller and broadcasts `EyeSyncMessage` to followers. Followers call `EyeInterpolator::updateTarget()` and interpolate received state. If no WiiChuck is present, all devices run autonomous animation independently.

### Board Selection

Board-specific code is gated on:
- `ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED` — set by `-D` flag in `[env:amoled]`
- `ARDUINO_LILYGO_T_RGB` — set by `-D` flag in `[env:trgb]`

Pin definitions are in `include/BoardPins.h`. I2C pins differ between boards (AMOLED: SDA=7/SCL=6; T-RGB: SDA=48/SCL=8).

## Customization Hook

`src/user/user_hooks.cpp` provides `user_setup()` and `user_loop()` for custom extensions without modifying core files. Available expression calls from either function: `eyesBlink()`, `eyesBoop()`, `eyesClose()`, `eyesWide()`, `eyesNormal()`.

## Build Flags

Key flags set in `[esp32base]`:
- `-D FDEBUG` — enables debug logging
- `-D DEBUG_OVERLAY_ENABLED` — enables FPS/battery HUD
- `-D CORE_DEBUG_LEVEL=3` — ESP-IDF log verbosity

Both environments inherit from `[esp32base]` via `extends`.

## Pre/Post Build Scripts

- `scripts/remove_defs.py` — removes stale macro definitions before build
- `scripts/rename_firmware.py` — renames output `.bin` with version/board suffix
- `scripts/patch_libdeps.py` — patches library dependencies post-install
- `scripts/build_unified_binary.py` — merges bootloader + partition table + app into a single flashable binary
