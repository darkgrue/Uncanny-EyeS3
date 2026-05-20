#pragma once

#include <Arduino.h>
#include "eyes.h"

// Eye: eagle for 480x480 display
// Map radius: 240
// Eye radius fraction: 0.5

const uint8_t eagle_upper[480 * 2] PROGMEM = {0};

const uint8_t eagle_lower[480 * 2] PROGMEM = {0};

namespace eagle {
  const EyeDefinition eye PROGMEM = {
      "eagle", 0.5, 0x7C0F, true, 0, disp_480_480,
      { 0x0000, 0, 0.35, 1.67 },
      { 0.5, { nullptr, 0, 0 }, 0xFF01, 0, 0, 0, 0 },
      { { nullptr, 0, 0 }, 0xFFFF, 0, 0, 0, 0 },
      { eagle_upper, eagle_lower, 0x0000 },
      { 240, polarAngle_240, polarDist_240 }
  };
}  // namespace eagle
