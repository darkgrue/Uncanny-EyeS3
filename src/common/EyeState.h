/**
 * @file EyeState.h
 * @brief Shared state structures for eye animation and network sync.
 *
 * Contains enums (EyeBlinkState, EyeCommand), configuration structs
 * (EyeConfig, EyeRuntime), and the EyeSyncMessage layout for ESP-NOW peer
 * synchronization. These are shared across animation, rendering, and network
 * layers so they are placed in common/ rather than a specific domain.
 */
#ifndef EYE_STATE_H
#define EYE_STATE_H

#include <stdint.h>

/**
 * @brief Blink state machine states.
 */
enum EyeBlinkState : uint8_t
{
  NOBLINK       = 0, // Eyelids at resting gap, auto-blink armed
  ENBLINK       = 1, // Automatic blink: closing phase
  DEBLINK       = 2, // Automatic blink: opening phase
  FORCINGCLOSE  = 3, // Animated transition to fully closed (Z-button hold)
  FORCINGWIDE   = 4, // Animated transition to wide-open expression (C-button hold)
  FORCINGNORMAL = 5  // Animated transition back to resting gap (button release)
};

/**
 * @brief Static configuration for a single eye definition.
 */
struct EyeConfig
{
  int16_t eyeRadius;    // Eye circle radius in pixels (0 = auto)
  int16_t irisRadius;   // Iris radius in pixels (default 60)
  int16_t pupilMin;     // Minimum pupil radius (fully dilated)
  int16_t pupilMax;     // Maximum pupil radius (fully constricted)
  uint16_t scleraColor; // Sclera RGB565 color
  uint16_t irisColor;   // Default iris color if no texture
  uint16_t pupilColor;  // Pupil RGB565 color
  uint16_t backColor;   // Background/occluded eye color
  uint16_t eyelidColor; // Eyelid RGB565 color
  float coverage;       // Polar map coverage (0.0-1.0, default 0.6)
  bool tracking;        // Eyelids track pupil position
  float trackFactor;    // Tracking strength (0.0-1.0)
  float spin;           // Iris spin rate (RPM * 1024)
  int16_t iSpin;        // Fixed per-frame spin override
  uint16_t mirror;      // 0 = normal, 1023 = flip X
  uint8_t rotation;     // Display rotation (0-3)
};

/**
 * @brief Runtime state for a single eye instance.
 */
struct EyeRuntime
{
  float eyeX;               // Current eye X (0 = center)
  float eyeY;               // Current eye Y
  float targetX;            // Target eye X
  float targetY;            // Target eye Y
  float pupilFactor;        // Pupil scale (1.0 = full size)
  float upperLidFactor;     // Upper eyelid open fraction (0.0-1.0)
  float lowerLidFactor;     // Lower eyelid open fraction (0.0-1.0)
  float blinkFactor;        // Blink progress (0.0 = open, 1.0 = closed)
  EyeBlinkState blinkState; // Current blink state
  uint32_t blinkStartTime;  // Blink state start time (micros)
  uint32_t blinkDuration;   // Current blink duration
  uint16_t irisAngle;       // Iris rotation (0-1023)
  uint16_t scleraAngle;     // Sclera rotation (0-1023)
};

/**
 * @brief Message payload for ESP-NOW state synchronization.
 *
 * Broadcast from the controller eye to followers. Contains eye position,
 * pupil dilation, blink state, expression commands, and an application-layer
 * authentication token derived from the shared network key configured in
 * /eyes_config.json. Receivers reject messages whose token does not match
 * their own configured token (0 = no security configured on either end).
 */
struct EyeSyncMessage
{
  uint8_t  macAddress[6]; // Sender MAC address
  uint8_t  eyeIndex;      // Which eye this message controls (0, 1, ...)
  float    eyeX;          // Eye position X (-1.0 to +1.0)
  float    eyeY;          // Eye position Y (-1.0 to +1.0)
  float    pupilFactor;   // Pupil dilation (0.0 to 1.0)
  uint8_t  blinkState;    // EyeBlinkState enum value
  uint32_t timestamp;     // Sender timestamp (millis) for interpolation
  uint8_t  command;       // EyeCommand (blink, boop, close, wide, normal)
  uint32_t networkToken;  // App-layer auth token (0 = no security)
};

/**
 * @brief Expression and gesture commands sent over the network.
 */
enum EyeCommand : uint8_t
{
  CMD_NONE = 0,  // No command
  CMD_BLINK = 1, // Trigger a single blink
  CMD_BOOP = 2,  // Trigger a boop expression
  CMD_CLOSE = 3, // Hold eyelids closed
  CMD_WIDE = 4,  // Hold eyelids wide open
  CMD_NORMAL = 5 // Return to normal tracking
};

#endif // EYE_STATE_H