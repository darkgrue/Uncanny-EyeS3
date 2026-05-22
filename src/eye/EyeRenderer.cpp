/**
 * @file EyeRenderer.cpp
 * @brief Core eye rendering engine using precomputed polar maps.
 *
 * Allocates two PSRAM-backed RGB565 frame buffers for double-buffered rendering.
 * Renders the eye column-by-column into the render buffer, then flips to the
 * display buffer for transfer. Handles pupil/iris/sclera coloring per pixel
 * based on squared distance from the eye center, and clips pixels within a
 * circular boundary. Supports both custom eyelid tables from the eye definition
 * and a math-based curved fallback. Enforces even coordinates for CO5300.
 */
#include "EyeRenderer.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

// Debug: force eyelids to a fixed gap (0=closed, 100=open, 50=half).
#define DEBUG_EYELID_GAP_OVERRIDE -1 // -1 = disabled, 0-100 = forced percentage (50=half)

EyeRenderer::EyeRenderer()
    : m_display(nullptr), m_displaySize(0), m_mapRadius(0), m_mapDiameter(0), m_eyeDef(nullptr),
      m_frameBuf1(nullptr), m_frameBuf2(nullptr), m_renderBuf(nullptr), m_displayBuf(nullptr),
      m_dirtyMinX(0), m_dirtyMinY(0), m_dirtyMaxX(0), m_dirtyMaxY(0),
      m_prevDirtyMinX(0), m_prevDirtyMinY(0), m_prevDirtyMaxX(0), m_prevDirtyMaxY(0),
      m_eyelidRenderer()
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

/**
 * @brief Initialize the renderer with display and eye definition.
 *
 * Allocates two PSRAM buffers (double-buffering) and a scratch buffer for
 * display transfers. Computes display size as min(width, height) and sets up
 * the circular clip bounds. Initializes the EyelidRenderer with tracking if
 * configured in the eye definition.
 */
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

  size_t bufSize = m_displaySize * m_displaySize * sizeof(uint16_t);
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

  m_renderBuf = m_frameBuf1;
  m_displayBuf = m_frameBuf2;

  m_dirtyMinX = 0;
  m_dirtyMinY = 0;
  m_dirtyMaxX = m_displaySize;
  m_dirtyMaxY = m_displaySize;
  m_prevDirtyMinX = 0;
  m_prevDirtyMinY = 0;
  m_prevDirtyMaxX = m_displaySize;
  m_prevDirtyMaxY = m_displaySize;

  Serial.printf("[EyeRenderer] Double buffer allocated: %d bytes each\n", bufSize);

  m_scratchBuf = (uint16_t *)heap_caps_malloc(SCRATCH_BUF_SIZE * sizeof(uint16_t),
                                              MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
  if (!m_scratchBuf)
  {
    Serial.println("[EyeRenderer] Warning: Failed to allocate scratch buffer!");
  }

  return true;
}

/**
 * @brief Render the complete eye frame into the render buffer.
 *
 * Clears the render buffer with the eye's background color, then iterates
 * each column within the eye's bounding box. For each pixel, determines
 * pupil/iris/sclera color by squared-distance comparison against precomputed
 * radii. Eyelid occlusion is applied either via custom tables or math-based
 * curved boundary. After rendering, swaps render/display buffers and
 * initiates a full-frame display transfer.
 *
 * @param eyeX Normalized eye X position (-1.0 to +1.0).
 * @param eyeY Normalized eye Y position (-1.0 to +1.0).
 * @param pupilFactor Pupil scale (1.0 = full size, smaller = contracted).
 * @param upperLidFactor Upper eyelid open fraction (0.0-1.0).
 * @param lowerLidFactor Lower eyelid open fraction (0.0-1.0).
 * @param blinkFactor Overall blink factor (0.0 = open, 1.0 = closed).
 * @param irisAngle Iris rotation angle (unused in current implementation).
 * @param scleraAngle Sclera rotation angle (unused in current implementation).
 */
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

  uint16_t eyeRadius = eyeRadiusPixels(eye);
  uint16_t irisRadius = irisRadiusPixels(eye);
  uint16_t pupilRadius = (uint16_t)(irisRadius * pupilFactor);

  int centerX = m_displaySize / 2;
  int centerY = m_displaySize / 2;

  int offsetX = (int)(eyeX * (m_displaySize / 4));
  int offsetY = (int)(eyeY * (m_displaySize / 4));

  int margin = eyeRadius;
  int minX = centerX + offsetX - margin;
  int maxX = centerX + offsetX + margin;
  int minY = centerY + offsetY - margin;
  int maxY = centerY + offsetY + margin;

  minX = (minX < 0) ? 0 : minX;
  maxX = (maxX > m_displaySize) ? m_displaySize : maxX;
  minY = (minY < 0) ? 0 : minY;
  maxY = (maxY > m_displaySize) ? m_displaySize : maxY;

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

  int eyeRadiusSq = (int)eyeRadius * (int)eyeRadius;
  int irisRadiusSq = (int)irisRadius * (int)irisRadius;
  int pupilRadiusSq = (int)pupilRadius * (int)pupilRadius;

  uint16_t bgColor = eye.backColor;
  memset(m_renderBuf, bgColor, m_displaySize * m_displaySize * sizeof(uint16_t));

  float eyelidGap = 1.0f - blinkFactor;

#if DEBUG_EYELID_GAP_OVERRIDE >= 0
  eyelidGap = DEBUG_EYELID_GAP_OVERRIDE / 100.0f;
#endif

  int eyeCenterX = centerX + offsetX;
  int eyeCenterY = centerY + offsetY;
  (void)eyeCenterX;

  bool hasCustomLids = false;
  if (m_eyeDef->eyelid.upper != nullptr && m_eyeDef->eyelid.lower != nullptr)
  {
    for (int checkCol = 0; checkCol < m_displaySize; checkCol += 47)
    {
      uint8_t upperEnd = m_eyeDef->eyelid.upper[checkCol * 2 + 1];
      uint8_t lowerStart = m_eyeDef->eyelid.lower[checkCol * 2];
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

    int dyMaxSq = eyeRadiusSq - dx * dx;
    int dyMax;
    if (dyMaxSq <= 0)
    {
      dyMax = 0;
    }
    else
    {
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
      int tableIdx = x * 2;
      uint8_t upperStart = m_eyeDef->eyelid.upper[tableIdx];
      uint8_t upperEnd = m_eyeDef->eyelid.upper[tableIdx + 1];

      if (upperEnd == 255 && m_eyeDef->eyelid.lower[tableIdx + 1] == 255)
      {
        goto useCurvedBoundary;
      }

      float scale = (float)m_displaySize / 255.0f;
      int upperStartY = (int)(upperStart * scale);
      int lowerEndY = (int)(m_eyeDef->eyelid.lower[tableIdx + 1] * scale);

      float gapClosed = 1.0f - eyelidGap;
      int gapCenter = (upperStartY + lowerEndY) / 2;
      int upperLidBottom = upperStartY + (int)(gapClosed * (gapCenter - upperStartY));
      int lowerLidTop = lowerEndY - (int)(gapClosed * (lowerEndY - gapCenter));

      visibleTop = upperLidBottom - eyeCenterY;
      visibleBottom = lowerLidTop - eyeCenterY;
    }
    else
    {
    useCurvedBoundary:
      float gapClosed = 1.0f - eyelidGap;
      int clip = (int)(gapClosed * dyMax);
      visibleTop = -dyMax + clip;
      visibleBottom = dyMax - clip;
    }

    int yStart = visibleTop + eyeCenterY;
    int yEnd = visibleBottom + eyeCenterY;

    if (yStart < 0)
      yStart = 0;
    if (yEnd > m_displaySize)
      yEnd = m_displaySize;
    if (yStart >= yEnd)
      continue;

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

  m_dirtyMinX = minX;
  m_dirtyMinY = minY;
  m_dirtyMaxX = maxX;
  m_dirtyMaxY = maxY;

  uint16_t *temp = m_renderBuf;
  m_renderBuf = m_displayBuf;
  m_displayBuf = temp;

  m_display->beginDisplayTransfer();
  m_display->drawRGBBitmap(0, 0, m_displayBuf, m_displaySize, m_displaySize);
  m_display->endDisplayTransfer();
}