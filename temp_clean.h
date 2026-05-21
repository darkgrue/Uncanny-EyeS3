#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include "common/EyeState.h"
#include "common/DisplayGeometry.h"
#include "common/DisplayHAL.h"
#include "eyes.h"
#include <cstring>

// Core eye rendering engine
// Uses precomputed polar/displacement maps from EyeDefinition
class EyeRenderer {
public:
    EyeRenderer();
    ~EyeRenderer();
    
    // Initialize with display and eye definition (uses precomputed tables)
    bool begin(DisplayHAL* display, const EyeDefinition& eyeDef);
    
    // Render a single column of the eye
    // x = column index (0 to displaySize-1)
    void renderColumn(int x, float eyeX, float eyeY, float pupilFactor,
                      uint16_t irisAngle, uint16_t scleraAngle);
    
    // Render full frame (call once per eye position update)
    void renderFrame(float eyeX, float eyeY, float pupilFactor,
    uint16_t irisAngle, uint16_t scleraAngle);

    /// Render using column buffer approach for optimal QSPI performance
                                uint16_t irisAngle, uint16_t scleraAngle);
                     uint16_t irisAngle, uint16_t scleraAngle);
    
// Get render buffer for DMA
    uint16_t* getColumnBuffer() { return m_columnBuf; }
    int getColumnBufferSize() const { return m_displaySize; }

    // Get circular clip for debug rendering
    const CircularClip& getClip() const { return m_clip; }

private:
    DisplayHAL* m_display = nullptr;
    int m_displaySize = 0;
    int m_mapRadius = 0;
    int m_mapDiameter = 0;

    // Reference to eye definition (must persist during rendering)
    const EyeDefinition* m_eyeDef = nullptr;

    // Column render buffer
    uint16_t* m_columnBuf = nullptr;

    // Precomputed circular clipping bounds
    CircularClip m_clip;
};

#endif // EYE_RENDERER_H
