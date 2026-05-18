#include "EyeRenderer.h"
#include <Arduino.h>
#include <math.h>

#define PI 3.14159265358979f

EyeRenderer::EyeRenderer() {
}

EyeRenderer::~EyeRenderer() {
    if (m_polarAngle) delete[] m_polarAngle;
    if (m_polarDist) delete[] m_polarDist;
    if (m_displaceX) delete[] m_displaceX;
    if (m_displaceY) delete[] m_displaceY;
    if (m_columnBuf) delete[] m_columnBuf;
    m_iris.free();
    m_sclera.free();
}

bool EyeRenderer::begin(DisplayHAL* display, int displaySize, int mapRadius) {
    m_display = display;
    m_displaySize = displaySize;
    m_mapRadius = mapRadius;
    m_mapDiameter = mapRadius * 2;
    
    // Allocate buffers
    size_t polarSize = m_mapDiameter * m_mapDiameter;
    m_polarAngle = new int16_t[polarSize];
    m_polarDist = new int8_t[polarSize];
    
    size_t dispSize = (m_mapDiameter / 2) * (m_mapDiameter / 2);
    m_displaceX = new uint8_t[dispSize];
    m_displaceY = new uint8_t[dispSize];
    
    m_columnBuf = new uint16_t[displaySize];
    
    computePolarMap();
    computeDisplacementMap();
    
    return true;
}

void EyeRenderer::computePolarMap() {
    // Compute polar angle/distance for each point in the map
    // Origin is at center of map (mapRadius, mapRadius)
    // Angle 0 = right, increases counter-clockwise (1024 = 360 degrees)
    
    for (int y = 0; y < m_mapDiameter; y++) {
        for (int x = 0; x < m_mapDiameter; x++) {
            int dx = x - m_mapRadius;
            int dy = y - m_mapRadius;
            int idx = y * m_mapDiameter + x;
            
            float dist = sqrt(dx * dx + dy * dy);
            
            // Distance: normalized to 128 at edge of sphere
            // Negative values indicate off-sphere (behind)
            if (dist <= m_mapRadius) {
                // Inside the hemisphere
                float normDist = dist / (float)m_mapRadius;
                // Map to sphere surface: sqrt(1 - z^2) for sphere
                float z = cos(normDist * PI / 2.0f);
                float sphereDist = sqrt(1.0f - z * z);
                m_polarDist[idx] = (int8_t)(sphereDist * 127);
            } else {
                m_polarDist[idx] = -1;  // Off sphere
            }
            
            // Angle: 0-1023 representing 0-360 degrees
            float angle = atan2((float)dy, (float)dx);
            int angle1024 = (int)((angle + PI) / (2.0f * PI) * 1024.0f) & 0x3FF;
            m_polarAngle[idx] = angle1024;
        }
    }
}

void EyeRenderer::computeDisplacementMap() {
    // Precompute spherical displacement for faster rendering
    // This mimics the M4_Eyes "displacement map" optimization
    
    int halfMap = m_mapDiameter / 2;
    
    for (int y = 0; y < halfMap; y++) {
        for (int x = 0; x < halfMap; x++) {
            // Distance from center in spherical coordinates
            float dist = sqrt(x * x + y * y);
            
            if (dist < halfMap) {
                float angle = atan2((float)y, (float)x);
                float sphereDist = sin(dist * PI / (2.0f * halfMap));
                
                // Displacement is the offset needed to render spherical look
                float dx = cos(angle) * sphereDist * halfMap;
                float dy = sin(angle) * sphereDist * halfMap;
                
                m_displaceX[y * halfMap + x] = (uint8_t)(dx + 0.5f);
                m_displaceY[y * halfMap + x] = (uint8_t)(dy + 0.5f);
            } else {
                m_displaceX[y * halfMap + x] = 255;  // Indicates off-map
                m_displaceY[y * halfMap + x] = 0;
            }
        }
    }
}

bool EyeRenderer::loadTexture(EyeTexture& tex, const uint16_t* data, int width, int height) {
    tex.free();
    tex.data = new uint16_t[width * height];
    memcpy(tex.data, data, width * height * 2);
    tex.width = width;
    tex.height = height;
    tex.allocated = true;
    return true;
}

void EyeRenderer::setEyelidData(const EyelidData& data) {
    m_eyelids = data;
    m_hasEyelids = true;
}


// Render a single column of the eye
// This is the core rendering loop - called once per column per frame
void EyeRenderer::renderColumn(int x, float eyeX, float eyeY, float pupilFactor,
                               float upperLidFactor, float lowerLidFactor, float blinkFactor,
                               uint16_t irisAngle, uint16_t scleraAngle) {
    // Calculate eye position in screen coordinates
    // eyeX/eyeY are normalized -1 to +1, mapped to pixel offset
    int eyePixelX = (int)(eyeX * (float)(m_displaySize / 2));
    int eyePixelY = (int)(eyeY * (float)(m_displaySize / 2));
    
    // Position offset relative to display center
    int xOffset = x - (m_displaySize / 2);
    
    // Compute eyelid boundaries
    int y1 = 0, y2 = m_displaySize - 1;
    
    if (m_hasEyelids) {
        int lidIdx = x;  // Column index into eyelid arrays
        
        // Apply blink factor to interpolate between open and closed positions
        float upperOpen = m_eyelids.upperOpen[lidIdx] / 255.0f;
        float upperClosed = m_eyelids.upperClosed[lidIdx] / 255.0f;
        float lowerOpen = m_eyelids.lowerOpen[lidIdx] / 255.0f;
        float lowerClosed = m_eyelids.lowerClosed[lidIdx] / 255.0f;
        
        // Interpolate eyelid positions
        float upperPos = upperClosed + upperLidFactor * (upperOpen - upperClosed);
        float lowerPos = lowerClosed + lowerLidFactor * (lowerOpen - lowerClosed);
        
        // Apply blink animation
        upperPos = upperPos * (1.0f - blinkFactor);
        lowerPos = lowerPos * (1.0f - blinkFactor);
        
        y1 = (int)(lowerPos * m_displaySize);
        y2 = (int)((1.0f - upperPos) * m_displaySize);
        
        y1 = constrain(y1, 0, m_displaySize - 1);
        y2 = constrain(y2, 0, m_displaySize - 1);
    }
    
    // Track if we're rendering eyelid or eye
    uint16_t* ptr = m_columnBuf;
    
    for (int y = 0; y < m_displaySize; y++) {
        if (y < y1 || y > y2) {
            // Outside eye area - eyelid color
            *ptr++ = m_eyelidColor;
        } else {
            // Inside eye area - compute polar coordinates
            int mapX = xOffset + eyePixelX + m_mapRadius;
            int mapY = (y - m_displaySize / 2) + eyePixelY + m_mapRadius;
            
            // Check bounds
            if (mapX >= 0 && mapX < m_mapDiameter && mapY >= 0 && mapY < m_mapDiameter) {
                int mapIdx = mapY * m_mapDiameter + mapX;
                int angle = m_polarAngle[mapIdx];
                int dist = m_polarDist[mapIdx];
                
                if (dist >= 0) {
                    // On the spherical surface
                    uint16_t color;
                    
                    // Determine if sclera, iris, or pupil based on distance
                    int irisBoundary = (m_mapRadius * 3) / 4;  // Iris is 75% of eye radius
                    int pupilBoundary = (int)(irisBoundary * pupilFactor);
                    
                    if (dist > irisBoundary) {
                        // Sclera - use texture or color
                        if (m_sclera.data) {
                            int texX = ((angle + scleraAngle) & 0x3FF) * m_sclera.width / 1024;
                            int texY = dist * m_sclera.height / 128;
                            texX %= m_sclera.width;
                            texY %= m_sclera.height;
                            color = m_sclera.data[texY * m_sclera.width + texX];
                        } else {
                            color = m_scleraColor;
                        }
                    } else if (dist > pupilBoundary) {
                        // Iris - use texture or color
                        if (m_iris.data) {
                            int texX = ((angle + irisAngle) & 0x3FF) * m_iris.width / 1024;
                            int texY = dist * m_iris.height / irisBoundary;
                            texX %= m_iris.width;
                            texY %= m_iris.height;
                            color = m_iris.data[texY * m_iris.width + texX];
                        } else {
                            color = m_irisColor;
                        }
                    } else {
                        // Pupil
                        color = m_pupilColor;
                    }
                    *ptr++ = color;
                } else {
                    // Behind the sphere
                    *ptr++ = m_backColor;
                }
            } else {
                // Off the map
                *ptr++ = m_backColor;
            }
        }
    }
}

void EyeRenderer::renderFrame(float eyeX, float eyeY, float pupilFactor,
                              float upperLidFactor, float lowerLidFactor, float blinkFactor,
                              uint16_t irisAngle, uint16_t scleraAngle) {
    // Render each column sequentially
    for (int x = 0; x < m_displaySize; x++) {
        renderColumn(x, eyeX, eyeY, pupilFactor, upperLidFactor, lowerLidFactor, 
                     blinkFactor, irisAngle, scleraAngle);
        
        // Send column to display
        m_display->setAddrWindow(x, 0, 1, m_displaySize);
        m_display->pushPixels(m_columnBuf, m_displaySize);
    }
}

void generateDefaultEyelids(EyelidData& eyelids, int displaySize) {
    // Generate default parabolic eyelid shapes
    // Upper eyelid: curves down from corners to center
    // Lower eyelid: curves up from corners to center
    
    for (int i = 0; i < displaySize; i++) {
        float normalized = (float)i / (float)(displaySize - 1);  // 0 to 1
        
        // Parabolic curve (peaks at edges, dips in middle)
        float upperCurve = 0.25f * (1.0f - cos(normalized * 2.0f * PI));
        float lowerCurve = 0.25f * (1.0f - cos(normalized * 2.0f * PI));
        
        // Upper eyelid: 0 = fully open at top, 255 = fully closed at bottom
        // We're generating the "open" position (how much is covered when open)
        eyelids.upperOpen[i] = (uint8_t)(upperCurve * 255);
        eyelids.upperClosed[i] = (uint8_t)(0.9f * 255);  // Almost fully closed
        
        // Lower eyelid: 0 = fully open at bottom, 255 = fully closed at top
        eyelids.lowerOpen[i] = (uint8_t)(lowerCurve * 255);
        eyelids.lowerClosed[i] = (uint8_t)(0.9f * 255);
    }
    
    // Ensure edge columns are fully open
    eyelids.upperOpen[0] = eyelids.upperOpen[displaySize - 1] = 0;
    eyelids.lowerOpen[0] = eyelids.lowerOpen[displaySize - 1] = 0;
}
