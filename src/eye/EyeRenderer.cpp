#include "EyeRenderer.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

// Debug: force eyelids to a fixed gap (0=closed, 100=open, 50=half)
#define DEBUG_EYELID_GAP_OVERRIDE -1 // -1 = disabled, 0-100 = forced percentage (50=half)

EyeRenderer::EyeRenderer()
    : m_display(nullptr), m_displaySize(0), m_mapRadius(0), m_mapDiameter(0), m_eyeDef(nullptr), m_frameBuf1(nullptr), m_frameBuf2(nullptr), m_renderBuf(nullptr), m_displayBuf(nullptr), m_dirtyMinX(0), m_dirtyMinY(0), m_dirtyMaxX(0), m_dirtyMaxY(0), m_prevDirtyMinX(0), m_prevDirtyMinY(0), m_prevDirtyMaxX(0), m_prevDirtyMaxY(0), m_eyelidRenderer()
{
}

EyeRenderer::~EyeRenderer()
{
  if (m_frameBuf1)
  {
    heap_caps_free(m_frameBuf1);
    m_frameBuf1 = nullptr;
  }
  if (m_frameBuf2)
  {
    heap_caps_free(m_frameBuf2);
    m_frameBuf2 = nullptr;
  }
  if (m_scratchBuf)
  {
    heap_caps_free(m_scratchBuf);
    m_scratchBuf = nullptr;
  }
}

bool EyeRenderer::begin(DisplayHAL *display, const EyeDefinition &eyeDef)
{
  m_display = display;
  m_eyeDef = &eyeDef;
  m_displaySize = display->getWidth();
  if (display->getHeight() < m_displaySize)
  {
    m_displaySize = display->getHeight();
  }
  m_mapRadius = eyeDef.polarMap.radius;
  m_mapDiameter = m_mapRadius * 2;

  m_eyelidRenderer.begin(m_displaySize, eyeRadiusPixels(eyeDef), eyeDef.eyelid);
  m_eyelidRenderer.setTrackingEnabled(eyeDef.tracking);

  // Allocate double buffers in PSRAM with 4-byte alignment (addr_align requirement)
  size_t bufSize = m_displaySize * m_displaySize * sizeof(uint16_t);
  // Use MALLOC_CAP_32BIT for proper alignment (addr_align=4 for CO5300)
  m_frameBuf1 = (uint16_t *)heap_caps_malloc(bufSize, MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
  m_frameBuf2 = (uint16_t *)heap_caps_malloc(bufSize, MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);

  if (!m_frameBuf1 || !m_frameBuf2)
  {
    Serial.println("[EyeRenderer] Failed to allocate double buffers!");
    if (m_frameBuf1)
      heap_caps_free(m_frameBuf1);
    if (m_frameBuf2)
      heap_caps_free(m_frameBuf2);
    m_frameBuf1 = m_frameBuf2 = nullptr;
    return false;
  }

  // Start with buf1 as render target, buf2 as display target
  m_renderBuf = m_frameBuf1;
  m_displayBuf = m_frameBuf2;

  // Initialize dirty region to full screen
  m_dirtyMinX = 0;
  m_dirtyMinY = 0;
  m_dirtyMaxX = m_displaySize;
  m_dirtyMaxY = m_displaySize;
  m_prevDirtyMinX = 0;
  m_prevDirtyMinY = 0;
  m_prevDirtyMaxX = m_displaySize;
  m_prevDirtyMaxY = m_displaySize;

  Serial.printf("[EyeRenderer] Double buffer allocated: %d bytes each\n", bufSize);

  // Allocate scratch buffer for display transfers (no heap per-frame)
  m_scratchBuf = (uint16_t *)heap_caps_malloc(SCRATCH_BUF_SIZE * sizeof(uint16_t),
                                              MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
  if (!m_scratchBuf)
  {
    Serial.println("[EyeRenderer] Warning: Failed to allocate scratch buffer!");
  }

  return true;
}

void EyeRenderer::renderFrame(float eyeX, float eyeY, float pupilFactor,
                              float upperLidFactor, float lowerLidFactor, float blinkFactor,
                              uint16_t irisAngle, uint16_t scleraAngle)
{
  (void)irisAngle;
  (void)scleraAngle;

  if (!m_display || !m_eyeDef || !m_renderBuf)
  {
    Serial.println("[EyeRenderer] ERROR: null pointer!");
    return;
  }

  const EyeDefinition &eye = *m_eyeDef;

  // Calculate radii from eye definition
  uint16_t eyeRadius = eyeRadiusPixels(eye);
  uint16_t irisRadius = irisRadiusPixels(eye);
  uint16_t pupilRadius = (uint16_t)(irisRadius * pupilFactor);

  int centerX = m_displaySize / 2;
  int centerY = m_displaySize / 2;

  // Apply eye position offset (eyeX, eyeY are normalized -1 to +1)
  int offsetX = (int)(eyeX * (m_displaySize / 4));
  int offsetY = (int)(eyeY * (m_displaySize / 4));

  // Compute bounding box to include full eye circle at any position
  // Eye center can be offset by eyeRadius in any direction before edges clip
  int margin = eyeRadius; // Full eye radius as margin
  int minX = centerX + offsetX - margin;
  int maxX = centerX + offsetX + margin;
  int minY = centerY + offsetY - margin;
  int maxY = centerY + offsetY + margin;

  // Clamp to display bounds
  minX = (minX < 0) ? 0 : minX;
  maxX = (maxX > m_displaySize) ? m_displaySize : maxX;
  minY = (minY < 0) ? 0 : minY;
  maxY = (maxY > m_displaySize) ? m_displaySize : maxY;

  // CO5300 requires EVEN coordinates and dimensions
  if (minX % 2 != 0)
    minX--;
  if (minY % 2 != 0)
    minY--;
  if (maxX % 2 != 0)
    maxX++;
  if (maxY % 2 != 0)
    maxY++;
  if (maxX > m_displaySize)
    maxX = m_displaySize;
  if (maxY > m_displaySize)
    maxY = m_displaySize;

  // Precompute squared radii
  int eyeRadiusSq = (int)eyeRadius * (int)eyeRadius;
  int irisRadiusSq = (int)irisRadius * (int)irisRadius;
  int pupilRadiusSq = (int)pupilRadius * (int)pupilRadius;

  // Fill entire buffer with background color (fast memset)
  uint16_t bgColor = eye.backColor;
  memset(m_renderBuf, bgColor, m_displaySize * m_displaySize * sizeof(uint16_t));

  // Render eyeball within circular bounds, but respect eyelid occlusion
  // The eyelid gap determines how much of the eye is visible:
  //   1.0 = fully open (normal visibility)
  //   0.0 = fully closed (no visibility)
  float eyelidGap = 1.0f - blinkFactor;

#if DEBUG_EYELID_GAP_OVERRIDE >= 0
  eyelidGap = DEBUG_EYELID_GAP_OVERRIDE / 100.0f;
#endif

  // Eye center relative to display center for eyelid calculations
  int eyeCenterX = centerX + offsetX;
  int eyeCenterY = centerY + offsetY;
  (void)eyeCenterX;

  // Only render pixels within bounding box
  // Per-column approach with curved eyelid approximation
  // Supports custom eyelid tables when available, falls back to math-based curve
  // Custom lids are detected by checking for non-marker values in the table
  // (marker values 0 or 255 indicate "use curved boundary" fallback)
  bool hasCustomLids = false;
  if (m_eyeDef->eyelid.upper != nullptr && m_eyeDef->eyelid.lower != nullptr)
  {
    // Check if any column has actual eyelid data (not just 0 or 255 markers)
    // Sample several columns to avoid false negatives from edge cases
    for (int checkCol = 0; checkCol < m_displaySize; checkCol += 47)
    {
      uint8_t upperEnd = m_eyeDef->eyelid.upper[checkCol * 2 + 1]; // endY value
      uint8_t lowerStart = m_eyeDef->eyelid.lower[checkCol * 2];   // startY value
      if ((upperEnd != 0 && upperEnd != 255) || (lowerStart != 0 && lowerStart != 255))
      {
        hasCustomLids = true;
        break;
      }
    }
  }

  for (int x = minX; x < maxX; x++)
  {
    int dx = x - eyeCenterX;

    // Calculate dyMax (half height of eye at column x) using integer sqrt
    int dyMaxSq = eyeRadiusSq - dx * dx;

    int dyMax;
    if (dyMaxSq <= 0)
    {
      dyMax = 0;
    }
    else
    {
      // Fast integer sqrt
      int guess = eyeRadius;
      for (int i = 0; i < 3; i++)
      {
        int prod = guess * guess;
        if (prod > dyMaxSq)
        {
          guess = (guess + dyMaxSq / guess) / 2;
        }
        else
        {
          guess = (guess + dyMaxSq / guess + 1) / 2;
        }
      }
      dyMax = guess;
    }

    int visibleTop;
    int visibleBottom;

    if (hasCustomLids)
    {
      // Use custom eyelid table for this column
      // Tables store (startY, endY) pairs as uint8 (absolute screen positions)
      // These are in 0-255 range representing 0-screenHeight pixels
      int tableIdx = x * 2;

      uint8_t upperStart = m_eyeDef->eyelid.upper[tableIdx];
      uint8_t upperEnd = m_eyeDef->eyelid.upper[tableIdx + 1];
      uint8_t lowerStart = m_eyeDef->eyelid.lower[tableIdx];
      uint8_t lowerEnd = m_eyeDef->eyelid.lower[tableIdx + 1];

      // Fallback to curved boundary only if BOTH eyelids have no custom data:
      // - upperEnd == 255 means upper lid has no custom data
      // - lowerEnd == 255 means lower lid has no custom data
      // If only ONE is 255, the other lid's custom data is still used.
      // This handles the case where geneye.py sets endY=255 for open columns
      // (eyelid extends to screen edge) vs. actually having no data.
      if (upperEnd == 255 && lowerEnd == 255)
      {
        goto useCurvedBoundary;
      }

      // Convert from 0-255 normalized to actual pixel positions
      float scale = (float)m_displaySize / 255.0f;
      // Upper lid: startY is the TOP edge (moves down when closing)
      // Lower lid: endY is the BOTTOM edge (moves up when closing)
      int upperStartY = (int)(upperStart * scale);
      int lowerEndY = (int)(lowerEnd * scale);

      // Calculate animation based on eyelidGap
      // gapClosed = 0 (open): upper at upperStartY, lower at lowerEndY
      // gapClosed = 1 (closed): upper moves down, lower moves up until they meet at gap center
      float gapClosed = 1.0f - eyelidGap;

      // The gap center is where upper lid's top meets lower lid's bottom
      // Upper moves DOWN from upperStartY toward gapCenter
      // Lower moves UP from lowerEndY toward gapCenter
      int gapCenter = (upperStartY + lowerEndY) / 2;
      int upperLidBottom = upperStartY + (int)(gapClosed * (gapCenter - upperStartY));
      int lowerLidTop = lowerEndY - (int)(gapClosed * (lowerEndY - gapCenter));

      // Upper lid clips from top down to upperLidBottom
      // Lower lid clips from bottom up to lowerLidTop
      // Note: upperLidBottom and lowerLidTop are absolute screen coordinates
      visibleTop = upperLidBottom - eyeCenterY; // Convert to relative for consistency
      visibleBottom = lowerLidTop - eyeCenterY; // Convert to relative for consistency
    }
    else
    {
    useCurvedBoundary:
      // Calculate visible range for this column using math-based curved boundary
      // The eyelid clips symmetrically from top and bottom as gapClosed increases
      // gapClosed=0: visible range is [-dyMax, +dyMax] (full eye visible)
      // gapClosed=1: visible range is [0, 0] (eye fully occluded)
      float gapClosed = 1.0f - eyelidGap;
      int clip = (int)(gapClosed * dyMax);

      visibleTop = -dyMax + clip;   // upper eyelid clips here
      visibleBottom = dyMax - clip; // lower eyelid clips here
    }

    // Convert to display coordinates
    int yStart = visibleTop + eyeCenterY;
    int yEnd = visibleBottom + eyeCenterY;

    // Clamp to display/frame buffer bounds
    if (yStart < 0)
      yStart = 0;
    if (yEnd > m_displaySize)
      yEnd = m_displaySize;

    if (yStart >= yEnd)
      continue;

    // Render the visible portion of this column
    for (int y = yStart; y < yEnd; y++)
    {
      int dy = y - eyeCenterY;
      int distSq = dx * dx + dy * dy;

      uint16_t color;
      if (distSq <= pupilRadiusSq)
      {
        color = eye.pupil.color;
      }
      else if (distSq <= irisRadiusSq)
      {
        color = eye.iris.color;
      }
      else
      {
        color = eye.sclera.color;
      }
      m_renderBuf[y * m_displaySize + x] = color;
    }
  }

  // Render eyelid overlay
  // Note: Simplified occlusion is already done in the main loop above.
  // This call currently disabled to avoid double-pass. Can re-enable
  // later with optimized renderDefaultEyelids if corner effects are needed.
  // m_eyelidRenderer.render(eyeX, eyeY, eyelidGap, m_renderBuf);

  // Store current dirty region (kept for future use if needed)
  m_dirtyMinX = minX;
  m_dirtyMinY = minY;
  m_dirtyMaxX = maxX;
  m_dirtyMaxY = maxY;

  // Swap buffers BEFORE transfer - displayBuf holds the frame to transfer
  uint16_t *temp = m_renderBuf;
  m_renderBuf = m_displayBuf;
  m_displayBuf = temp;

  // Full frame transfer - no memcpy needed, contiguous buffer
  m_display->beginDisplayTransfer();
  m_display->drawRGBBitmap(0, 0, m_displayBuf, m_displaySize, m_displaySize);
  m_display->endDisplayTransfer();
}