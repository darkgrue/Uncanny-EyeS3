#pragma once

#include <Arduino.h>
#include "../eyes.h"

// Eye: default_eye for 240x240 display
// Map radius: 120
// Eye radius fraction: 0.5

const uint8_t default_eye_upper[240 * 2] PROGMEM = {0};

const uint8_t default_eye_lower[240 * 2] PROGMEM = {0};

namespace default_eye {
  const EyeDefinition eye PROGMEM = {
      "default_eye", 0.5, 0x7C0F, true, 0, disp_240_240,
      { 0x0000, 0, 0.35, 1.67 },
      { 0.5, { nullptr, 0, 0 }, 0xFF01, 0, 0, 0 },
      { { nullptr, 0, 0 }, 0xFFFF, 0, 0, 0 },
      { default_eye_upper, default_eye_lower, 0x0000 },
      { 120, polarAngle_120, polarDist_120 }
  };
}  // namespace default_eye
