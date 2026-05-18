#ifndef EYE_STATE_H
#define EYE_STATE_H

#include <stdint.h>

// Eye movement state machine states (mirrors M4_Eyes)
enum EyeBlinkState : uint8_t {
    NOBLINK = 0,   // Not currently blinking
    ENBLINK = 1,   // Eyelid closing
    DEBLINK = 2    // Eyelid opening
};

// Configuration for a single eye
struct EyeConfig {
    int16_t  eyeRadius;       // Eye radius in pixels (0 = auto)
    int16_t  irisRadius;      // Iris radius (default 60)
    int16_t  pupilMin;        // Minimum pupil radius
    int16_t  pupilMax;        // Maximum pupil radius
    uint16_t scleraColor;     // Sclera color (565 RGB)
    uint16_t irisColor;       // Default iris color if no texture
    uint16_t pupilColor;     // Pupil color
    uint16_t backColor;       // Back of eye color
    uint16_t eyelidColor;     // Eyelid color
    float    coverage;        // Polar map coverage (0.0-1.0, default 0.6)
    bool     tracking;        // Eyelids track pupil position
    float    trackFactor;     // How much eyelids follow pupil
    float    spin;            // Iris spin rate (RPM * 1024)
    int16_t  iSpin;           // Fixed per-frame spin override
    uint16_t mirror;         // 0 = normal, 1023 = flip X
    uint8_t  rotation;        // Display rotation (0-3)
};

// Runtime state for a single eye
struct EyeRuntime {
    float   eyeX;            // Current X position (0 = center)
    float   eyeY;            // Current Y position
    float   targetX;         // Target X position
    float   targetY;         // Target Y position
    float   pupilFactor;     // Current pupil scale (1.0 = full, smaller = contracted)
    float   upperLidFactor;  // Upper eyelid position (0.0-1.0)
    float   lowerLidFactor;  // Lower eyelid position (0.0-1.0)
    float   blinkFactor;     // Current blink progress (0.0 = open, 1.0 = closed)
    EyeBlinkState blinkState;// Current blink state
    uint32_t blinkStartTime; // Blink state start time (micros)
    uint32_t blinkDuration;  // Current blink duration
    uint16_t irisAngle;      // Current iris rotation angle (0-1023)
    uint16_t scleraAngle;    // Current sclera rotation angle (0-1023)
};

// Network sync message for ESP-NOW
struct EyeSyncMessage {
    uint8_t  macAddress[6];  // Sender MAC address
    uint8_t  eyeIndex;       // Which eye is this (0, 1, etc.)
    float    eyeX;            // Eye position X (-1.0 to +1.0)
    float    eyeY;            // Eye position Y (-1.0 to +1.0)
    float    pupilFactor;     // Pupil dilation (0.0 to 1.0)
    uint8_t  blinkState;      // EyeBlinkState enum
    uint32_t timestamp;       // Timestamp for interpolation
    uint8_t  command;        // Special commands (blink, boop, etc.)
};

// ESP-NOW command types
enum EyeCommand : uint8_t {
    CMD_NONE = 0,
    CMD_BLINK = 1,
    CMD_BOOP = 2,
    CMD_CLOSE = 3,
    CMD_WIDE = 4,
    CMD_NORMAL = 5
};

#endif // EYE_STATE_H
