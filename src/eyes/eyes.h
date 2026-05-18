#pragma once

#include <Arduino.h>
#include "DisplayHAL.h"

// Forward declarations for lookup tables (defined in generated headers)
extern const uint8_t polarAngle_240[];
extern const uint8_t polarDist_240_125_125_110[];
extern const uint8_t disp_240_240[];

// Pupil/iris configuration
struct PupilConfig {
    uint16_t color;
    uint8_t  slitRadius;    // 0 = round pupil
    uint8_t  min;           // min pupil size (0-255 represents min radius)
    uint8_t  max;           // max pupil size
};

// Iris texture configuration
struct IrisConfig {
    uint8_t  radius;
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
    uint16_t    radius;        // Eye radius in pixels
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

// Get screen dimensions based on display type
#ifdef ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED
    #define SCREEN_WIDTH  466
    #define SCREEN_HEIGHT 466
#elif defined(ARDUINO_LILYGO_T_RGB)
    #define SCREEN_WIDTH  480
    #define SCREEN_HEIGHT 320  // T-RGB is 320x480 but rotated
#else
    #define SCREEN_WIDTH  240
    #define SCREEN_HEIGHT 240
#endif

// Macro to declare an eye namespace
#define DECLARE_EYE(EyeName) \
    namespace EyeName { \
        extern const EyeDefinition eye; \
    }

// Function to render an eye column
void renderEyeColumn(const EyeDefinition& eye, 
                     int16_t x, 
                     float eyeX, float eyeY,
                     float pupilFactor,
                     float upperLidFactor, float lowerLidFactor,
                     float blinkFactor,
                     uint16_t irisAngle, uint16_t scleraAngle,
                     uint16_t* columnBuf);

// Function to render full eye frame
void renderEyeFrame(const EyeDefinition& eye,
                   float eyeX, float eyeY,
                   float pupilFactor,
                   float upperLidFactor, float lowerLidFactor,
                   float blinkFactor,
                   uint16_t irisAngle, uint16_t scleraAngle,
                   DisplayHAL& display);
