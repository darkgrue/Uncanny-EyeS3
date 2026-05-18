#ifndef EYE_CONFIG_H
#define EYE_CONFIG_H

#include "EyeState.h"
#include "DisplayGeometry.h"

// Main configuration container
struct EyeProjectConfig {
    // Display settings
    int displayWidth;
    int displayHeight;
    int displayOffsetX;
    int displayOffsetY;
    
    // Polar map settings
    int  mapRadius;
    int  mapDiameter;
    int  pupilMin;
    int  pupilMax;
    
    // Timing
    uint32_t blinkMinDuration;
    uint32_t blinkMaxDuration;
    uint32_t moveMinDuration;
    uint32_t moveMaxDuration;
    
    // Light sensor settings (-1 = disabled)
    int  lightSensorPin;
    uint16_t lightSensorMin;
    uint16_t lightSensorMax;
    float lightSensorCurve;
    float irisMin;
    float irisRange;
    
    // Input settings
    bool wiiChuckEnabled;
    int  wiiChuckAddress;
    
    // Network settings
    bool espNowEnabled;
    uint8_t espNowChannel;
    
    // User hook settings
    bool moveEyesRandomly;
    float eyeTargetX;
    float eyeTargetY;
};

#endif // EYE_CONFIG_H
