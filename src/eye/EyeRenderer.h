#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include "EyeState.h"
#include "DisplayGeometry.h"
#include "DisplayHAL.h"
#include <cstring>

// Texture data for iris/sclera
struct EyeTexture {
    uint16_t* data = nullptr;
    uint16_t  width = 0;
    uint16_t  height = 0;
    bool      allocated = false;
    
    void free() {
        if (allocated && data) {
            delete[] data;
        }
        data = nullptr;
        allocated = false;
    }
};

// Core eye rendering engine
// Handles polar coordinate projection, texture mapping, eyelid rendering
class EyeRenderer {
public:
    EyeRenderer();
    ~EyeRenderer();
    
    // Initialize with display and configuration
    bool begin(DisplayHAL* display, int displaySize, int mapRadius);
    
    // Load texture from 16-bit raw RGB565 image data
    bool loadTexture(EyeTexture& tex, const uint16_t* data, int width, int height);
    
    // Load texture from file (BMP or raw)
    bool loadTextureFromFile(EyeTexture& tex, const char* filename);
    
    // Set eyelid data
    void setEyelidData(const EyelidData& data);
    
    // Set colors
    void setScleraColor(uint16_t color) { m_scleraColor = color; }
    void setIrisColor(uint16_t color) { m_irisColor = color; }
    void setPupilColor(uint16_t color) { m_pupilColor = color; }
    void setBackColor(uint16_t color) { m_backColor = color; }
    void setEyelidColor(uint16_t color) { m_eyelidColor = color; }
    
    // Configure tracking
    void setTracking(bool enabled, float factor = 0.5f) {
        m_tracking = enabled;
        m_trackFactor = factor;
    }
    
    // Render a single column of the eye
    // x = column index (0 to displaySize-1)
    void renderColumn(int x, float eyeX, float eyeY, float pupilFactor,
                      float upperLidFactor, float lowerLidFactor, float blinkFactor,
                      uint16_t irisAngle, uint16_t scleraAngle);
    
    // Render full frame (call once per eye position update)
    void renderFrame(float eyeX, float eyeY, float pupilFactor,
                     float upperLidFactor, float lowerLidFactor, float blinkFactor,
                     uint16_t irisAngle, uint16_t scleraAngle);
    
    // Get render buffer for DMA
    uint16_t* getColumnBuffer() { return m_columnBuf; }
    int getColumnBufferSize() const { return m_displaySize; }

private:
    void computePolarMap();
    void computeDisplacementMap();
    
    DisplayHAL* m_display = nullptr;
    int m_displaySize = 0;
    int m_mapRadius = 0;
    int m_mapDiameter = 0;
    
    // Polar coordinate maps
    int16_t*  m_polarAngle = nullptr;     // [mapDiameter * mapDiameter]
    int8_t*   m_polarDist = nullptr;       // [mapDiameter * mapDiameter]
    uint8_t*  m_displaceX = nullptr;      // [mapDiameter/2 * mapDiameter/2]
    uint8_t*  m_displaceY = nullptr;      // [mapDiameter/2 * mapDiameter/2]
    
    // Eyelid data
    EyelidData m_eyelids;
    bool m_hasEyelids = false;
    
    // Textures
    EyeTexture m_iris;
    EyeTexture m_sclera;
    
    // Colors
    uint16_t m_scleraColor = 0xFFFF;
    uint16_t m_irisColor = 0xFF01;
    uint16_t m_pupilColor = 0x0000;
    uint16_t m_backColor = 0x7BEF;
    uint16_t m_eyelidColor = 0x0000;
    
    // Tracking
    bool m_tracking = true;
    float m_trackFactor = 0.5f;
    
    // Column render buffer
    uint16_t* m_columnBuf = nullptr;
};

// Helper: Generate default eyelid shapes (simple parabolic curves)
void generateDefaultEyelids(EyelidData& eyelids, int displaySize);

#endif // EYE_RENDERER_H
