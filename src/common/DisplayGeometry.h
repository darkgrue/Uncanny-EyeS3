#ifndef DISPLAY_GEOMETRY_H
#define DISPLAY_GEOMETRY_H

#include <stdint.h>
#include <vector>
#include <cmath>

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

// Precomputed scanline bounds for circular clipping
// Computed once at init, reused every frame to avoid sqrt() per row
struct ScanlineBounds {
    int16_t xStart;
    int16_t xEnd;
};

class CircularClip {
public:
    void compute(int centerX, int centerY, int radius, int width, int height) {
        m_centerX = centerX;
        m_centerY = centerY;
        m_radius = radius;
        m_width = width;
        m_height = height;
        m_bounds.resize(height);

        int32_t radiusSq = (int32_t)radius * radius;
        int32_t centerXR = centerX;

        for (int y = 0; y < height; y++) {
            int32_t distY = y - centerY;
            int32_t distYSq = distY * distY;

            if (distYSq < radiusSq) {
                int32_t xExtentSq = radiusSq - distYSq;
                int16_t xExtent = (int16_t)sqrt((float)xExtentSq);
                m_bounds[y].xStart = centerXR - xExtent;
                m_bounds[y].xEnd = centerXR + xExtent;
            } else {
                m_bounds[y].xStart = -1;
                m_bounds[y].xEnd = -1;
            }
        }
    }

    bool isRowActive(int y) const {
        return y >= 0 && y < m_height && m_bounds[y].xStart >= 0;
    }

    int16_t getXStart(int y) const { return m_bounds[y].xStart; }
    int16_t getXEnd(int y) const { return m_bounds[y].xEnd; }

    int getCenterX() const { return m_centerX; }
    int getCenterY() const { return m_centerY; }
    int getRadius() const { return m_radius; }

private:
    int m_centerX = 0;
    int m_centerY = 0;
    int m_radius = 0;
    int m_width = 0;
    int m_height = 0;
    std::vector<ScanlineBounds> m_bounds;
};

#endif // DISPLAY_GEOMETRY_H
