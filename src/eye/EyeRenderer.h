/**
 * @file EyeRenderer.h
 * @brief Core eye rendering engine using precomputed polar maps.
 *
 * Renders iris, sclera, and eyelids to a double-buffered RGB565 frame in PSRAM.
 * Uses displacement/polar lookup tables from EyeDefinition for fast per-pixel
 * coordinate mapping. Supports dirty-region tracking for partial display updates.
 */
#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include "common/EyeState.h"
#include "common/DisplayGeometry.h"
#include "common/DisplayHAL.h"
#include "eyes.h"
#include "EyelidRenderer.h"
#include <cstring>

/**
 * @brief Renders a single eye using precomputed displacement and polar maps.
 *
 * Maintains two PSRAM-backed frame buffers (double-buffering) to allow drawing
 * the next frame while the current one is being transferred to the display.
 * Coordinates eyelid rendering and tracks dirty regions to minimize transfer size.
 */
class EyeRenderer
{
public:
  EyeRenderer();
  ~EyeRenderer();

  /**
   * @brief Initialize with display and eye definition.
   * @param display Pointer to the display HAL.
   * @param eyeDef Eye definition with precomputed maps.
   * @return true if initialization succeeded.
   */
  bool begin(DisplayHAL *display, const EyeDefinition &eyeDef);

  /**
   * @brief Render a single display column of the eye.
   *
   * Used internally by the column-buffer rendering path for optimal QSPI throughput.
   *
   * @param x Column index (0 to displaySize-1).
   * @param eyeX Normalized eye X (-1.0 to +1.0).
   * @param eyeY Normalized eye Y (-1.0 to +1.0).
   * @param pupilFactor Pupil constriction (0.0 dilated, 1.0 constricted).
   * @param upperLidFactor Upper eyelid position (0.0 = closed, 1.0 = open).
   * @param lowerLidFactor Lower eyelid position (0.0 = closed, 1.0 = open).
   * @param eyelidGap Eyelid open fraction (0.0 = fully closed, 1.0 = fully retracted).
   * @param irisAngle Iris rotation in degrees.
   * @param scleraAngle Sclera rotation in degrees.
   */
  void renderColumn(int x, float eyeX, float eyeY, float pupilFactor,
                    float upperLidFactor, float lowerLidFactor, float eyelidGap,
                    uint16_t irisAngle, uint16_t scleraAngle);

  /**
   * @brief Render the complete eye frame into the render buffer.
   *
   * Computes the dirty region for the current frame, clears the previous position,
   * draws the new eye, and flips the buffer swap flag.
   */
  void renderFrame(float eyeX, float eyeY, float pupilFactor,
                   float upperLidFactor, float lowerLidFactor, float eyelidGap,
                   uint16_t irisAngle, uint16_t scleraAngle);

  /**
   * @brief Column-buffer rendering path for QSPI displays.
   *
   * Renders one column at a time to improve DMA transfer efficiency.
   */
  void renderFrameUsingColumns(float eyeX, float eyeY, float pupilFactor,
                               float upperLidFactor, float lowerLidFactor, float eyelidGap,
                               uint16_t irisAngle, uint16_t scleraAngle);

  /** @brief Raw access to the current frame buffer for display transfer. */
  uint16_t *getFrameBuffer() { return m_displayBuf; }

  /** @brief Total pixel count in the frame buffer (displaySize^2). */
  int getFrameBufferSize() const { return m_displaySize * m_displaySize; }

  /** @brief Circular clipping bounds for the eye. */
  const CircularClip &getClip() const { return m_clip; }

  /** @brief Upper eyelid smooth factor (0.0-1.0). */
  float getUpperLidFactor() const { return m_eyelidRenderer.getUpperLidFactor(); }

  /** @brief Lower eyelid smooth factor (0.0-1.0). */
  float getLowerLidFactor() const { return m_eyelidRenderer.getLowerLidFactor(); }

private:
  DisplayHAL *m_display = nullptr; // Display abstraction
  int m_displaySize = 0;           // Frame buffer dimension (pixels)
  int m_mapRadius = 0;             // Radius of the eye polar map
  int m_mapDiameter = 0;           // Diameter of the eye polar map

  const EyeDefinition *m_eyeDef = nullptr; // Current eye definition

  // Double-buffered frame in PSRAM
  uint16_t *m_frameBuf1 = nullptr;  // Buffer 1 (render or display side)
  uint16_t *m_frameBuf2 = nullptr;  // Buffer 2 (render or display side)
  uint16_t *m_renderBuf = nullptr;  // Currently writing to this buffer
  uint16_t *m_displayBuf = nullptr; // Ready to send to display

  // Dirty region tracking
  int m_dirtyMinX, m_dirtyMinY; // Current frame dirty bounds
  int m_dirtyMaxX, m_dirtyMaxY;
  int m_prevDirtyMinX, m_prevDirtyMinY; // Previous frame dirty bounds
  int m_prevDirtyMaxX, m_prevDirtyMaxY;

  uint16_t m_backgroundColor = 0x0000; // Clear color (black)

  CircularClip m_clip; // Precomputed circular clip region

  /**
   * @brief Scratch buffer for display bus transfers.
   *
   * Pre-allocated to avoid per-frame heap allocation. Sized for a full
   * 466x466 frame buffer (maximum display size supported).
   */
  static constexpr int SCRATCH_BUF_SIZE = 466 * 466;
  uint16_t *m_scratchBuf = nullptr;

  EyelidRenderer m_eyelidRenderer; // Manages eyelid rendering

  bool m_hasCustomLids = false; // Cached: true when eyelid tables have non-trivial data
};

#endif // EYE_RENDERER_H