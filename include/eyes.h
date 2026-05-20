#pragma once

#include <Arduino.h>
#include "common/DisplayHAL.h"

// Get screen dimensions based on display type
#ifdef ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED
    #define SCREEN_WIDTH  466
    #define SCREEN_HEIGHT 466
#elif defined(ARDUINO_LILYGO_T_RGB)
    #define SCREEN_WIDTH  480
    #define SCREEN_HEIGHT 480
#else
    #define SCREEN_WIDTH  240
    #define SCREEN_HEIGHT 240
#endif

// Note: Lookup tables (polarAngle_*, polarDist_*, disp_*) are defined
// in display_466.h or display_480.h depending on display type

// Pupil/iris configuration
struct PupilConfig {
    uint16_t color;
    uint8_t  slitRadius;    // 0 = round pupil
    float    minFraction;   // min pupil size as fraction of iris radius
    float    maxFraction;    // max pupil size as fraction of iris radius
};

// Iris texture configuration
struct IrisConfig {
    float    radiusFraction; // iris radius as fraction of eye radius
    struct {
        const uint16_t* data;   // Pointer to texture data
        uint16_t width;
        uint16_t height;
    } texture;
    uint16_t color;        // Default color if no texture
    uint16_t angle;       // Initial rotation (0-1023)
    float    spin;        // Spin rate (RPM * 1024)
    int16_t  iSpin;       // Fixed per-frame spin override
    uint16_t mirror;      // 0 = normal, 1023 = flip X
};

// Sclera configuration  
struct ScleraConfig {
    struct {
        const uint16_t* data;
        uint16_t width;
        uint16_t height;
    } texture;
    uint16_t color;
    uint16_t angle;
    float    spin;
    int16_t  iSpin;
    uint16_t mirror;
};

// Eyelid configuration
struct EyelidConfig {
    const uint8_t* upper;     // Upper eyelid lookup table (pairs of startY, endY per column)
    const uint8_t* lower;     // Lower eyelid lookup table
    uint16_t color;
};

// Polar map info
struct PolarMapInfo {
    uint16_t radius;
    const uint8_t* angleMap;
    const uint8_t* distMap;
};

// Main eye definition structure
struct EyeDefinition {
    const char* name;
    float       radiusFraction; // eye radius as fraction of smaller screen dimension
    uint16_t    backColor;
    bool        tracking;
    uint8_t     squint;
    const uint8_t* dispMap;     // Spherical displacement map

    PupilConfig pupil;
    IrisConfig  iris;
    ScleraConfig sclera;
    EyelidConfig eyelid;
    PolarMapInfo polarMap;
};

// Compute actual pixel radius from fraction
inline uint16_t eyeRadiusPixels(const EyeDefinition& eye) {
    uint16_t minDim = (SCREEN_WIDTH < SCREEN_HEIGHT) ? SCREEN_WIDTH : SCREEN_HEIGHT;
    return (uint16_t)(eye.radiusFraction * minDim);
}

// Compute iris radius in pixels
inline uint16_t irisRadiusPixels(const EyeDefinition& eye) {
    return (uint16_t)(eyeRadiusPixels(eye) * eye.iris.radiusFraction);
}

// Macro to declare an eye namespace
#define DECLARE_EYE(EyeName) \
    namespace EyeName { \
        extern const EyeDefinition eye; \
    }
