#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include "common/EyeState.h"
#include "common/DisplayGeometry.h"
#include "common/DisplayHAL.h"
#include "eyes.h"
#include <cstring>

// Core eye rendering engine
// Uses precomputed polar/displacement maps from EyeDefinition
class EyeRenderer
{
public:
  EyeRenderer();
  ~EyeRenderer();

  // Initialize with display and eye definition (uses precomputed tables)
  bool begin(DisplayHAL *display, const EyeDefinition &eyeDef);

  // Render a single column of the eye
  // x = column index (0 to displaySize-1)
  void renderColumn(int x, float eyeX, float eyeY, float pupilFactor,
                    float upperLidFactor, float lowerLidFactor, float blinkFactor,
                    uint16_t irisAngle, uint16_t scleraAngle);

  // Render full frame (call once per eye position update)
  void renderFrame(float eyeX, float eyeY, float pupilFactor,
                   float upperLidFactor, float lowerLidFactor, float blinkFactor,
                   uint16_t irisAngle, uint16_t scleraAngle);

  /// Render using column buffer approach for optimal QSPI performance
  void renderFrameUsingColumns(float eyeX, float eyeY, float pupilFactor,
                               float upperLidFactor, float lowerLidFactor, float blinkFactor,
                               uint16_t irisAngle, uint16_t scleraAngle);

  // Get render buffer for external access
  uint16_t *getFrameBuffer() { return m_frameBuf1; }
  int getFrameBufferSize() const { return m_displaySize * m_displaySize; }

  // Get circular clip for debug rendering
  const CircularClip &getClip() const { return m_clip; }

private:
  DisplayHAL *m_display = nullptr;
  int m_displaySize = 0;
  int m_mapRadius = 0;
  int m_mapDiameter = 0;

  // Reference to eye definition (must persist during rendering)
  const EyeDefinition *m_eyeDef = nullptr;

  // Double-buffered frame rendering
  // m_renderBuf: currently being drawn into
  // m_displayBuf: ready to send to display
  uint16_t *m_frameBuf1 = nullptr;  // PSRAM buffer 1
  uint16_t *m_frameBuf2 = nullptr;  // PSRAM buffer 2
  uint16_t *m_renderBuf = nullptr;  // Current render target
  uint16_t *m_displayBuf = nullptr; // Current display target

  // Dirty region tracking for partial updates
  int m_dirtyMinX, m_dirtyMinY;
  int m_dirtyMaxX, m_dirtyMaxY;

  // Previous dirty region (to clear old position)
  int m_prevDirtyMinX, m_prevDirtyMinY;
  int m_prevDirtyMaxX, m_prevDirtyMaxY;

  // Background color for clearing
  uint16_t m_backgroundColor = 0x0000;

  // Precomputed circular clipping bounds
  CircularClip m_clip;

  // Pre-allocated scratch buffer for display transfers (avoids heap per-frame)
  // Sized for maximum single transfer: full display width * height
  static constexpr int SCRATCH_BUF_SIZE = 466 * 466;
  uint16_t* m_scratchBuf = nullptr;
};

#endif // EYE_RENDERER_H
