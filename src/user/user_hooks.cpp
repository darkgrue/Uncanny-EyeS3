// user_hooks.cpp - User extension hooks (M4_Eyes compatible pattern)
//
// Copy this file to user.cpp and modify the functions below
// to add custom behavior without modifying the core eye code.

#ifndef USER_HOOKS_CPP
#define USER_HOOKS_CPP

#include <Arduino.h>

// Called once at startup, after display initialization but before eye rendering begins
void user_setup() {
    Serial.println("user_setup() called");
    // Add your initialization code here:
    // - Initialize sensors
    // - Set up peripherals
    // - Load custom data
}

// Called periodically during the animation loop
// This is SPI "quiet time" so I2C and other communication is safe here.
// NOTE: This function BLOCKS - keep it simple and fast!
void user_loop() {
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
