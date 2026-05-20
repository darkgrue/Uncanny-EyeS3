#include "EyeRenderer.h"
#include <Arduino.h>

EyeRenderer::EyeRenderer() 
    : m_display(nullptr)
    , m_displaySize(0)
    , m_mapRadius(0)
    , m_mapDiameter(0)
    , m_eyeDef(nullptr)
    , m_columnBuf(nullptr) {
}

EyeRenderer::~EyeRenderer() {
    if (m_columnBuf) {
        delete[] m_columnBuf;
        m_columnBuf = nullptr;
    }
}

bool EyeRenderer::begin(DisplayHAL* display, const EyeDefinition& eyeDef) {
    m_display = display;
    m_eyeDef = &eyeDef;
    m_displaySize = display->getWidth();
    if (display->getHeight() < m_displaySize) {
        m_displaySize = display->getHeight();
    }
    m_mapRadius = eyeDef.polarMap.radius;
    m_mapDiameter = m_mapRadius * 2;

    // Allocate column buffer
    if (m_columnBuf) delete[] m_columnBuf;
    m_columnBuf = new uint16_t[m_displaySize];

    // Precompute circular clip bounds for the eye
    uint16_t eyeRadius = eyeRadiusPixels(eyeDef);
    int centerX = m_displaySize / 2;
    int centerY = m_displaySize / 2;
    m_clip.compute(centerX, centerY, eyeRadius, m_displaySize, m_displaySize);

    return true;
}

// Helper function to get texture color with coordinate wrapping
static inline uint16_t getTextureColor(const uint16_t* data, uint16_t w, uint16_t h,
                                        int32_t x, int32_t y) {
    if (x < 0) x += w * (-(x / w) + 1);
    if (y < 0) y += h * (-(y / h) + 1);
    x %= w;
    y %= h;
    return data[y * w + x];
}

// Fixed-point texture coordinate calculation helper
// Uses 16-bit fractional part (fixed-point Q16)
static inline int32_t fixedMul(int32_t a, int32_t b) {
    return (a * b) >> 16;
}

void EyeRenderer::renderColumn(int x, float eyeX, float eyeY, float pupilFactor,
                               float upperLidFactor, float lowerLidFactor, float blinkFactor,
                               uint16_t irisAngle, uint16_t scleraAngle) {
    if (!m_eyeDef || !m_display) return;
    
    const uint16_t DISPLAY_W = m_displaySize;
    const uint16_t DISPLAY_H = m_displaySize;
    const int16_t MAP_RADIUS = m_mapRadius;
    const int16_t MAP_DIAMETER = m_mapDiameter;
    
    const EyeDefinition& eye = *m_eyeDef;
    
    // Calculate eye radius and iris radius from fraction
    uint16_t eyeRadius = eyeRadiusPixels(eye);
    uint16_t irisRadius = irisRadiusPixels(eye);
    
    // Calculate eye position in pixels (from normalized -1.0 to +1.0)
    int16_t eyePixelX = (int16_t)(eyeX * (float)(DISPLAY_W / 2));
    int16_t eyePixelY = (int16_t)(eyeY * (float)(DISPLAY_H / 2));
    
    // Position offset relative to display center
    int16_t xOffset = x - (DISPLAY_W / 2);
    
    // Calculate eyelid boundaries from lookup tables
    uint16_t lidIdx = x * 2;
    uint8_t upperStart = eye.eyelid.upper[lidIdx];
    uint8_t upperEnd = eye.eyelid.upper[lidIdx + 1];
    uint8_t lowerStart = eye.eyelid.lower[lidIdx];
    uint8_t lowerEnd = eye.eyelid.lower[lidIdx + 1];
    
    // If 255, no eyelid defined for this column
    if (upperStart == 255) upperStart = 0;
    if (upperEnd == 255) upperEnd = 255;
    if (lowerStart == 255) lowerStart = 0;
    if (lowerEnd == 255) lowerEnd = 255;
    
    // Apply lid factors and blink
    float upperPos = upperStart + upperLidFactor * (upperEnd - upperStart);
    float lowerPos = lowerStart + lowerLidFactor * (lowerEnd - lowerStart);
    upperPos = upperPos * (1.0f - blinkFactor);
    lowerPos = lowerPos * (1.0f - blinkFactor);
    
    int16_t y1 = (int16_t)lowerPos;  // Bottom of visible eye
    int16_t y2 = (int16_t)upperPos;  // Top of visible eye
    y1 = constrain(y1, 0, DISPLAY_H - 1);
    y2 = constrain(y2, 0, DISPLAY_H - 1);
    
    // Calculate scaling factors from actual iris radius
    int16_t irisRadiusMap = (MAP_RADIUS * irisRadius) / eyeRadius;
    int16_t pupilRadiusMap = (int16_t)((float)irisRadiusMap * pupilFactor);
    
    // Get displacement map for spherical correction
    int16_t dispX;
    int16_t dispIdx;
    int16_t halfW = DISPLAY_W / 2;
    int16_t halfH = DISPLAY_H / 2;
    
    if (x < halfW) {
        dispIdx = ((halfW - 1) - x) * halfH;
        dispX = -eye.dispMap[dispIdx];
    } else {
        dispIdx = (x - halfW) * halfH;
        dispX = eye.dispMap[dispIdx];
    }
    
uint16_t* colBuf = m_columnBuf;

    for (int16_t y = 0; y < DISPLAY_H; y++) {
        if (y < y1 || y > y2) {
            // Outside eye area - eyelid color
            *colBuf++ = eye.eyelid.color;
        } else {
            // Inside eye area - lookup polar coordinates
            int16_t mapX = xOffset + dispX + eyePixelX + MAP_RADIUS;
            int16_t mapY = y - (DISPLAY_H / 2) + dispX + eyePixelY + MAP_RADIUS;

            // Clamp to map bounds
            mapX = constrain(mapX, 0, MAP_DIAMETER - 1);
            mapY = constrain(mapY, 0, MAP_DIAMETER - 1);

            int mapIdx = mapY * MAP_DIAMETER + mapX;

            uint8_t angle = eye.polarMap.angleMap[mapIdx];
            uint8_t dist = eye.polarMap.distMap[mapIdx];

            if (dist < 128) {
                // Sclera (0-127 in polar dist = outer to iris boundary)
                if (eye.sclera.texture.data) {
                    uint16_t texAngle = (angle + scleraAngle) & 0xFF;
                    int16_t texX = (texAngle * eye.sclera.texture.width) >> 8;
                    int16_t texY = (dist * eye.sclera.texture.height) >> 7;
                    *colBuf++ = getTextureColor(eye.sclera.texture.data,
                                                  eye.sclera.texture.width,
                                                  eye.sclera.texture.height,
                                                  texX, texY);
                } else {
                    *colBuf++ = eye.sclera.color;
                }
            } else if (dist < 255) {
                // Iris/pupil (128-254 = iris/pupil region, 255 = off map)
                int8_t irisDist = dist - 128;  // 0-126
                int16_t texY = (irisDist * eye.iris.texture.height) / pupilRadiusMap;
                texY = constrain(texY, 0, eye.iris.texture.height - 1);

                if (irisDist > pupilRadiusMap) {
                    // Iris (outside pupil)
                    if (eye.iris.texture.data) {
                        uint16_t texAngle = (angle + irisAngle) & 0xFF;
                        int16_t texX = (texAngle * eye.iris.texture.width) >> 8;
                        *colBuf++ = getTextureColor(eye.iris.texture.data,
                                                      eye.iris.texture.width,
                                                      eye.iris.texture.height,
                                                      texX, texY);
                    } else {
                        *colBuf++ = eye.iris.color;
                    }
                } else {
                    // Pupil
                    *colBuf++ = eye.pupil.color;
                }
            } else {
                // Behind sphere or off map
                *colBuf++ = eye.backColor;
            }
        }
    }
}

void EyeRenderer::renderFrame(float eyeX, float eyeY, float pupilFactor,
                              float upperLidFactor, float lowerLidFactor, float blinkFactor,
                              uint16_t irisAngle, uint16_t scleraAngle) {
    if (!m_display) return;

    for (int x = 0; x < m_displaySize; x++) {
        renderColumn(x, eyeX, eyeY, pupilFactor, upperLidFactor, lowerLidFactor,
                     blinkFactor, irisAngle, scleraAngle);

        m_display->setAddrWindow(x, 0, 1, m_displaySize);
        m_display->pushPixels(m_columnBuf, m_displaySize);
    }
}
