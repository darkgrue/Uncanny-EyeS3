/**
 * @file user_hooks.cpp
 * @brief M4_Eyes-compatible user extension hook template.
 *
 * Copy this file to user.cpp and implement user_setup() and user_loop() to
 * add custom behavior without touching the core eye animation code.
 * user_setup() runs once after display init but before rendering.
 * user_loop() runs periodically during the animation loop (safe for I2C).
 */
#ifndef USER_HOOKS_CPP
#define USER_HOOKS_CPP

#include <Arduino.h>

/**
 * @brief Called once at startup after display init, before eye rendering.
 *
 * Add sensor initialization, peripheral setup, or custom data loading here.
 */
void user_setup()
{
  Serial.println("user_setup() called");
  // Add your initialization code here:
  // - Initialize sensors
  // - Set up peripherals
  // - Load custom data
}

/**
 * @brief Called periodically during the animation loop.
 *
 * This runs during SPI "quiet time" so I2C and other communication is safe.
 * NOTE: This function BLOCKS the render loop — keep it short and fast.
 *
 * Available expression calls:
 *   eyesBlink()  - trigger a single blink
 *   eyesBoop()   - trigger a boop (tongue out)
 *   eyesClose()  - hold eyelids closed
 *   eyesWide()   - open eyelids wide
 *   eyesNormal() - return to normal tracking
 */
void user_loop()
{
  // Example: Check a sensor and trigger eye reactions
  /*
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 100) {
      lastCheck = millis();

      // Read your sensor here

      // If something detected, you can call:
      // eyesBlink()  - trigger a blink
      // eyesBoop()   - trigger a boop (special reaction)
      // eyesClose()  - close eyelids
      // eyesWide()   - open eyelids wide
      // eyesNormal() - return to normal behavior
  }
  */
}

#endif // USER_HOOKS_CPP