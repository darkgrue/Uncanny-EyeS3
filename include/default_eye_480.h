#pragma once

#include <Arduino.h>
#include "eyes.h"
#include "display_480.h"

// Eye: default_eye for 480x480 display
// Map radius: 240
// Eye radius fraction: 0.5

const uint8_t default_eye_upper[480 * 2] PROGMEM = {0};

const uint8_t default_eye_lower[480 * 2] PROGMEM = {0};

namespace default_eye {
  const EyeDefinition eye PROGMEM = {
      "default_eye", 0.5, 0x7C0F, true, 0, disp_480_480,
      { 0x0000, 0, 0.35, 1.67 },
      { 0.5, { nullptr, 0, 0 }, 0xFF01, 0, 0, 0, 0 },
      { { nullptr, 0, 0 }, 0xFFFF, 0, 0, 0, 0 },
      { default_eye_upper, default_eye_lower, 0x0000 },
      { 240, polarAngle_240, polarDist_240 }
  };
}  // namespace default_eye
