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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

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
   * @brief Render the complete eye frame into the render buffer.
   *
   * Computes the dirty region for the current frame, clears the previous position,
   * draws the new eye, and flips the buffer swap flag.
   */
  void renderFrame(float eyeX, float eyeY, float pupilFactor,
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
  // Per-buffer previous dirty rect: [0]=frameBuf1, [1]=frameBuf2.
  // Each buffer alternates every frame, so its stale content is from 2 frames ago,
  // not 1. Tracking separately prevents clearing the wrong region.
  int m_prevDirtyMinX[2] = {0, 0};
  int m_prevDirtyMinY[2] = {0, 0};
  int m_prevDirtyMaxX[2] = {0, 0};
  int m_prevDirtyMaxY[2] = {0, 0};

  uint16_t m_backgroundColor = 0x0000; // Clear color (black)

  CircularClip m_clip; // Precomputed circular clip region

  EyelidRenderer m_eyelidRenderer; // Manages eyelid rendering

  bool m_hasCustomLids = false; // Cached: true when eyelid tables have non-trivial data
  bool m_needsByteSwap = false; // Cached: true when m_display->directTransfer() needs big-endian pixels

  // DRAM caches for lookup tables — eliminates flash and PSRAM latency in the hot render loop.
  uint16_t *m_irisTexCache = nullptr;   // DRAM copy of iris texture
  uint16_t *m_scleraTexCache = nullptr; // DRAM copy of sclera texture
  uint8_t *m_angleMapCache = nullptr;   // DRAM copy of polar angle map
  uint8_t *m_radiusMapCache = nullptr;  // DRAM radius lookup: radiusMap[qy*r + qx] = sqrt(qx²+qy²)

  // Angle→texture row pointer tables: irisAnglePtrs[angle] = &irisTexData[texU(angle) * texH].
  // Replaces the per-pixel multiply (fullAngle * texH) with a single indexed load.
  const uint16_t *m_irisAnglePtrs[256] = {};
  const uint16_t *m_scleraAnglePtrs[256] = {};

  // Per-row precomputed circle and iris x-extent tables indexed by qy = |dy| (0..mapRadius-1).
  // Built once in begin() from eyeRadius / irisRadius (constant per eye definition).
  // Eliminates two sqrtf calls per row in renderFrame().
  int16_t *m_xCircHalfW  = nullptr; // [qy] max |dx| still inside the eye circle
  int16_t *m_xIrisLimTab = nullptr; // [qy] max |dx| still inside the iris boundary (-1 = none)

  // Async transfer task pinned to Core 0 — overlaps the next render with the current transfer.
  TaskHandle_t m_xferTask = nullptr;
  SemaphoreHandle_t m_xferReady = nullptr; // render→task: new frame ready to send
  SemaphoreHandle_t m_xferDone = nullptr;  // task→render: transfer complete
  volatile uint32_t m_xferUs = 0;          // last measured transfer time (updated by task)
  static void xferTaskFunc(void *pv);
};

#endif // EYE_RENDERER_H