# Uncanny-EyeS3

A port of the Adafruit M4 Eyes (<https://github.com/adafruit/Adafruit_Learning_System_Guides/tree/main/M4_Eyes>) to the Lilygo T-Display S3 AMOLED (466x466) and T-RGB (480x480).

Supports WiiChuck manual puppeteering of the eyes.

TODO: Support for the Gravity Offline Edge AI Gesture & Face Detection Sensor (<https://www.dfrobot.com/product-2914.html>).


## Supported Displays

| Display | Resolution | Map Radius |
|---------|------------|------------|
| T-Display S3 AMOLED | 466x466 | 233 |
| T-RGB | 480x480 | 240 |
| Default (testing) | 240x240 | 120 |

**Note:** Eye size is now configured as a fraction of the display's smaller dimension, making eye definitions work across all display sizes without modification.


## Eye Configuration Files

Eye definitions are stored as JSON files (`.eye`) in subdirectories under `resources/eyes/`. Each eye configuration has its own folder containing the `.eye` file and any associated textures.

```
resources/eyes/
  default_eye/
    default_eye.eye   # Eye configuration
    iris.png          # Optional iris texture (in same folder)
    sclera.png        # Optional sclera texture (in same folder)
  another_eye/
    another_eye.eye
    upper_lid.png     # Optional custom eyelid shapes
    lower_lid.png
```

Copy `default_eye/` to create new eye designs.


### Configuration Format

All size values are expressed as fractions (0.0 to 1.0) relative to the display's smaller dimension. This ensures eye configurations work across all supported display sizes without modification.

```json
{
    "name": "eye_name",
    "radiusFraction": 0.5,
    "backColor": 31759,
    "tracking": true,
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
        "color": 0
    }
}
```


### Field Descriptions

**Core:**
- `name`: Eye identifier (used in filename)
- `radiusFraction`: Eye radius as fraction of smaller screen dimension (default: 0.5 = 50%)
- `backColor`: Background color behind eye sphere (RGB565)
- `tracking`: Enable eye tracking (look at cursor)
- `squint`: Squint amount 0-255

**Pupil:**
- `color`: Pupil color (RGB565)
- `slitRadius`: 0 = round pupil, >0 = slit pupil
- `minFraction`: Minimum pupil size as fraction of iris radius
- `maxFraction`: Maximum pupil size as fraction of iris radius

**Iris:**
- `radiusFraction`: Iris radius as fraction of eye radius
- `color`: Iris color (RGB565)
- `angle`: Initial rotation (0-1023)
- `spin`: Continuous spin rate
- `iSpin`: Fixed per-frame spin override
- `mirror`: Mirror iris horizontally

**Sclera:**
- `color`: Sclera (white of eye) color
- `angle`: Initial rotation (0-1023)
- `spin`: Continuous spin rate
- `iSpin`: Fixed per-frame spin override
- `mirror`: Mirror sclera horizontally

**Eyelid:**
- `color`: Eyelid color (RGB565)


## Generating Eye Headers

Run `tablegen.py` to generate C++ header files from `.eye` configurations. The same `.eye` file uses fractional values that work across all display sizes, but you need to generate headers for each target display to get the appropriate polar map tables.

```bash
# Generate for all eyes in resources/eyes/ subdirectories
python resources/eyes/tablegen.py include/ --all amoled
python resources/eyes/tablegen.py include/ --all trgb

# Generate for a specific eye
python resources/eyes/tablegen.py include/ resources/eyes/default_eye/default_eye.eye amoled
```

The script discovers all `.eye` files recursively in `resources/eyes/` subdirectories. Image paths in config files are resolved relative to the `.eye` file's directory, so textures can be placed in the same folder.


### Output Location

Generated headers go into `include/` and are named by display type. The header contains the precomputed polar maps and displacement tables specific to each display size:
- `eye_name_466.h` for AMOLED (466x466)
- `eye_name_480.h` for T-RGB (480x480)
- `eye_name_240.h` for default (240x240)


## Custom Eyelid Shapes

Eyelid shapes are defined by grayscale images where white = eyelid, black = eye visible. Place images in the same directory as the `.eye` config:

```json
{
    "eyelid": {
        "upper": "upper_lid.png",
        "lower": "lower_lid.png",
        "color": 0
    }
}
```

**Resolution Requirements:**
- Images **must** match the target display resolution exactly
- For AMOLED (466x466): use 466x466 pixel images
- For T-RGB (480x480): use 480x480 pixel images
- The generator rejects mismatched sizes to prevent visual artifacts


## Custom Textures (Iris/Sclera)

Image textures for iris or sclera are placed in the same directory as the `.eye` config and referenced with relative paths:

```json
{
    "iris": {
        "filename": "iris_texture.png",
        "radiusFraction": 0.5,
        "color": 65281
    },
    "sclera": {
        "filename": "sclera_texture.png"
    }
}
```

**Supported formats:** PNG, BMP (24-bit RGB). Images are converted to RGB565 automatically.

**Resolution Recommendations:**
- Texture images can be any size but should approximate the rendered pixel size for best quality
- With default `radiusFraction: 0.5` and `iris.radiusFraction: 0.5`:
  - AMOLED (466x466): iris renders at ~233 pixels diameter
  - T-RGB (480x480): iris renders at ~240 pixels diameter
- Size textures to match these rendered dimensions (233x233 or 240x240 respectively)
- The same `.eye` config works across all displays (uses fractional sizing), but you generate separate headers per display type


## Building

```bash
# Build for AMOLED
pio run -e amoled

# Build for T-RGB
pio run -e trgb

# Upload
pio run -e amoled --target upload
```


## Runtime Eye Switching

Eyes can be switched at runtime via serial commands without recompiling.

### Serial Commands

| Command | Description |
|---------|-------------|
| `E0` | Switch to first eye (default_eye) |
| `E1` | Switch to second eye (eagle) |
| `E<n>` | Switch to eye at index n |

### How It Works

Eye definitions are registered in `include/EyeLibrary.h` which provides:
- `s_eyeRegistry[]` - array of available eye definitions
- `s_eyeCount` - total number of eyes
- `getEyeName(index)` - returns eye name string

When you send `E1` via serial, the command is parsed in `loop()`:
```cpp
while (Serial.available()) {
    char c = Serial.read();
    if (c == 'E') {
        int eyeIndex = Serial.parseInt();
        if (eyeIndex >= 0 && eyeIndex < s_eyeCount) {
            switchEye(eyeIndex);
        }
    }
}
```

The `switchEye()` function calls `EyeAnimator::setEyeIndex()` which:
1. Validates the index
2. Updates `m_eyeDef` to point to the new eye
3. Reinitializes the renderer with the new eye definition
4. Sets `m_needsRender = true` to trigger a refresh

### Adding New Eyes

**Important:** Each eye configuration requires headers for **both** display types (466 and 480) because the polar map tables and displacement maps are resolution-specific. The same `.eye` config uses fractional sizing that works across all displays, but the generated C++ headers contain precomputed tables for a specific resolution.

1. **Create eye config** in `resources/eyes/<name>/<name>.eye`
   - Use fractional values (0.0-1.0) for radius and sizing to work across display sizes
   - Reference texture files (PNG/BMP) with relative paths in the same directory
   - Reference eyelid images (grayscale) with relative paths for custom shapes

2. **Generate headers for both displays:**
   ```bash
   # Generate for AMOLED (466x466)
   python resources/eyes/tablegen.py include/ resources/eyes/<name>/<name>.eye amoled

   # Generate for T-RGB (480x480)
   python resources/eyes/tablegen.py include/ resources/eyes/<name>/<name>.eye trgb
   ```
   This creates:
   - `include/<name>_466.h` for AMOLED
   - `include/<name>_480.h` for T-RGB

3. **Add to EyeLibrary.h registry** under the appropriate display section:
   ```cpp
   #if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)
       #include "<name>_466.h"  // AMOLED header

       static const EyeDefinition* const s_eyeRegistry[] = {
           &default_eye::eye,
           &eagle::eye,
           &<name>::eye  // Add your eye here
       };
       static constexpr int s_eyeCount = 3;

   #elif defined(ARDUINO_LILYGO_T_RGB)
       #include "<name>_480.h"  // T-RGB header

       static const EyeDefinition* const s_eyeRegistry[] = {
           &default_eye::eye,
           &eagle::eye,
           &<name>::eye  // Add your eye here
       };
       static constexpr int s_eyeCount = 3;
   #endif
   ```

**Build and test:**
```bash
pio run -e amoled   # Test on AMOLED 466x466
pio run -e trgb     # Test on T-RGB 480x480
```

## WiiChuck Controller Connection

The WiiChuck (Nunchuk-style controller) connects to the T-Display S3 AMOLED or T-RGB via the I2C bus.


### Wiring

| WiiChuck Pin | T-Display S3 AMOLED | T-RGB |
|--------------|---------------------|-------|
| Data (white) | GPIO 21 (SDA) | GPIO 21 (SDA) |
| Clock (green) | GPIO 22 (SCL) | GPIO 22 (SCL) |
| 3.3V (red) | 3.3V | 3.3V |
| GND (black) | GND | GND |


### Connector

Use a WiiChuck extension cable or adapter. The controller side uses a 6-pin connector:

```
+---+---+
| 1 | 2 |  <- Key side
+---+---+
| 3 | 4 |
+---+---+
| 5 | 6 |
+---+---+
```

Pin assignments (viewed from controller connector):
- 1: Data (SDA)
- 2: Clock (SCL)
- 3: +3.3V
- 4: GND
- 5, 6: Key/not used


### I2C Configuration

- **I2C Address**: `0x52` (default WiiChuck address)
- **Bus Speed**: 400kHz (Fast Mode)
- **Pins**: SDA=21, SCL=22


### Initialization Sequence

The code initializes the controller with:
1. `0xF0/0x55` - Enable encryption
2. `0xFB/0x00` - Initialize controller mode


### Joystick

The analog joystick controls eye movement direction:

| Joystick Position | Eye Movement |
|-------------------|--------------|
| Center (idle) | Eyes return to autonomous wandering |
| Up | Eyes look up |
| Down | Eyes look down |
| Left | Eyes look left |
| Right | Eyes look right |
| Diagonal | Combined direction |

- Values are normalized to -1.0 to +1.0 range
- Dead zone of ±10 prevents drift when centered
- Movement is smoothed through EyeMovement interpolation


### Buttons

The Z and C buttons provide puppeteering commands:

| Button | Action Type | Behavior |
|--------|-------------|----------|
| Z | Edge-triggered | Causes eyes to blink once on press |
| Z (held) | Continuous | Eyes close while held |
| C | Edge-triggered | Causes eyes to "boop" on press |
| C (held) | Continuous | Eyes go wide (enlarged) while held |

**Priority**: close > wide > blink > boop (when multiple conditions true)


### Edge vs Hold Detection

- **Edge-triggered**: Action fires once when button is pressed, not repeated while held
- **Hold commands**: Action continues while button is held, stops on release


### Code Architecture

**Joystick Decoding** (`src/input/WiiChuck.cpp:51-53`):
- Raw values are 0-255, center is 0x80 (128)
- `joyX = m_status[0] - 0x80` gives -128 to +127
- Normalized to -1.0 to +1.0 (dead zone applied)

**Button Active-Low Detection** (`src/input/WiiChuck.cpp:55-59`):
- WiiChuck buttons are active-LOW (pressed = 0)
- `m_zPressed = !(m_status[5] & 0x01);`  // Z on bit 0
- `m_cPressed = !((m_status[5] >> 1) & 0x01);`  // C on bit 1

**Edge Detection** (`src/input/WiiChuck.cpp:71-81`):
- Trigger on button press, not hold
- `if (m_zPressed && !m_lastZPressed) { m_wantsBlink = true; }`
- `m_lastZPressed = m_zPressed;`  // Remember for next frame


### Initialization in Code

The WiiChuck is auto-initialized in `setupInput()` (`src/main.cpp:117-126`):

```cpp
void setupInput() {
    static WiiChuckInput chuck;
    if (chuck.begin()) {
        s_wiiChuck = &chuck;
        Serial.println("WiiChuck initialized");
    } else {
        Serial.println("WiiChuck not found (this is normal if not connected)");
    }
}
```

If no controller is connected, the system continues with autonomous eye movement.


### Multi-Eye Controller Selection

When multiple eyes are running (connected via ESP-NOW), the eye with an **active WiiChuck** becomes the controller:

1. **Any eye can have a WiiChuck connected** — all eyes run the same firmware with `setupInput()`
2. **The first eye to send ESP-NOW data becomes the controller** — detected in `src/main.cpp:138-143`:
   ```cpp
   sync.onDataReceived([](const EyeSyncMessage& msg, const uint8_t* mac) {
       sync.setControllerMac(mac);
   });
   ```
3. **Controller broadcasts its state** — position, pupil size, and commands (blink/boop/close/wide)
4. **Follower eyes ignore their own input** and follow the controller's state via `processNetworkInput()` (`src/eye/EyeAnimator.cpp:234-259`)

**Controller determination:**
- `isController()` (`src/eye/EyeAnimator.cpp:48`) returns true when `m_input && m_input->hasExclusiveControl()`
- `hasExclusiveControl()` returns true only when the joystick has active input (beyond dead zone)

| Scenario | Result |
|----------|--------|
| Eye A has WiiChuck, Eye B doesn't | Eye A is controller, Eye B follows |
| Eye B has WiiChuck, Eye A doesn't | Eye B is controller, Eye A follows |
| Neither has WiiChuck | Both run autonomous wandering |
| Both have WiiChuck | First to send data becomes controller |


## File Structure

```
resources/eyes/
  tablegen.py         # Eye data generator
  default_eye/
    default_eye.eye  # Sample eye configuration
  another_eye/
    another_eye.eye  # Custom eye configuration

include/
  *_466.h          # Generated AMOLED eye headers
  *_480.h          # Generated T-RGB eye headers
```