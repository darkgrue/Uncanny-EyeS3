#ifndef DISPLAY_GEOMETRY_H
#define DISPLAY_GEOMETRY_H

#include <stdint.h>

// Display geometry constants
// Max resolution supported: 480x480 (T-RGB)
#define MAX_DISPLAY_SIZE 480
#define MAP_MAX_RADIUS   240  // Maximum polar map radius (half of max display)

// Default eye radii for different displays
#define EYE_RADIUS_AMOLED 233  // ~466 / 2
#define EYE_RADIUS_TRGB   240  // ~480 / 2

// Eyelid shape data (255 = no data for that column)
struct EyelidData {
    uint8_t upperOpen[MAX_DISPLAY_SIZE];
    uint8_t upperClosed[MAX_DISPLAY_SIZE];
    uint8_t lowerOpen[MAX_DISPLAY_SIZE];
    uint8_t lowerClosed[MAX_DISPLAY_SIZE];
};

// Polar map data - allocated based on display size
struct PolarMapData {
    uint16_t* angle;      // [mapDiameter * mapDiameter]
    uint8_t*  dist;         // [mapDiameter * mapDiameter]
    uint8_t*  displaceX;    // [mapDiameter/2 * mapDiameter/2]
    uint8_t*  displaceY;    // [mapDiameter/2 * mapDiameter/2]
    uint16_t  radius;        // Map radius
    uint16_t  diameter;     // Map diameter (radius * 2)
};

// Screen coordinate conversion helpers
// Convert screen position to polar map coordinates
inline int16_t screenToMap(int16_t screenPos, int16_t mapRadius, int16_t displaySize) {
    return screenPos - (displaySize / 2) + mapRadius;
}

// Convert polar map coordinates to screen position
inline int16_t mapToScreen(int16_t mapPos, int16_t mapRadius, int16_t displaySize) {
    return mapPos + (displaySize / 2) - mapRadius;
}

// Calculate map radius based on display size and coverage factor
// coverage = 0.6 means the polar map covers 60% of the hemisphere
inline uint16_t calcMapRadius(uint16_t displaySize, float coverage = 0.6f) {
    // Eye radius is approximately displaySize/2
    // Map radius = eyeRadius * PI/2 * coverage
    return (uint16_t)((displaySize / 2.0f) * 3.14159f * coverage);
}

#endif // DISPLAY_GEOMETRY_H
