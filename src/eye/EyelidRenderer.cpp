/**
 * @file EyelidRenderer.cpp
 * @brief Eyelid animation renderer with smoothstep tracking.
 *
 * Renders upper and lower eyelids as overlays on the eye frame buffer.
 * Supports two modes: custom eyelid tables from the eye definition, and
 * a math-based curved boundary fallback. Eyelid position tracks the pupil
 * (upper moves down when looking up, lower moves up when looking down).
 * Uses smoothstep easing for smooth animation transitions between positions.
 */
#include "EyelidRenderer.h"
#include "animation/EyeMovement.h"
#include <cmath>

EyelidRenderer::EyelidRenderer()
    : m_displaySize(0), m_eyeRadius(0), m_config(nullptr),
      m_hasCustomEyelids(false), m_trackingEnabled(true),
      m_smoothedUpperFactor(1.0f), m_smoothedLowerFactor(1.0f),
      m_prevUpperY(0.5f), m_prevLowerY(0.5f), m_eyelidColor(0)
{
}

/**
 * @brief Initialize renderer with display dimensions and eye geometry.
 * @param displaySize Display width/height in pixels.
 * @param eyeRadius Eye circle radius in pixels.
 * @param config Eyelid geometry and color configuration.
 * @param needsByteSwap True if the frame buffer requires big-endian pixels.
 */
void EyelidRenderer::begin(int displaySize, uint16_t eyeRadius, const EyelidConfig &config, bool needsByteSwap)
{
  m_displaySize = displaySize;
  m_eyeRadius = eyeRadius;
  m_config = &config;
  m_needsByteSwap = needsByteSwap;

  m_eyelidColor = config.color;

  // Detect real custom eyelid data: any column with a non-sentinel inner edge.
  // generate_no_eyelids() fills (0,0) sentinels; real tables have values in 1-254.
  m_hasCustomEyelids = false;
  if (config.upper != nullptr && config.lower != nullptr)
  {
    for (int col = 0; col < m_displaySize; col++)
    {
      uint8_t upperEnd = config.upper[col * 2 + 1];
      uint8_t lowerStart = config.lower[col * 2];
      if ((upperEnd != 0 && upperEnd != 255) || (lowerStart != 0 && lowerStart != 255))
      {
        m_hasCustomEyelids = true;
        break;
      }
    }
  }

  float normalGap = 1.0f - config.normalClosure;

  // Tracking offsets start at zero; blink position is applied directly each frame.
  m_prevUpperY = 0.0f;
  m_prevLowerY = 0.0f;
  m_smoothedUpperFactor = 0.5f - normalGap * 0.5f;
  m_smoothedLowerFactor = 0.5f + normalGap * 0.5f;
}

/**
 * @brief Compute smoothed eyelid factors from eye position and blink gap.
 *
 * Advances exponential tracking smoothing and computes
 * m_smoothedUpperFactor / m_smoothedLowerFactor. Stores the eye center so
 * drawEyelids() can paint without re-deriving it. Call before the main
 * render loop so getUpperRow()/getLowerRow() are valid for row skipping.
 */
void EyelidRenderer::prepareFactors(float eyeX, float eyeY, float eyelidGap)
{
  // Smooth only the tracking offsets — BlinkFSM already eases the blink factor,
  // so re-smoothing it here would prevent the eyelids from ever fully closing.
  float targetUpperTracking = m_trackingEnabled ? -eyeY * EYELID_UPPER_TRACK_STRENGTH * 0.5f : 0.0f;
  float targetLowerTracking = m_trackingEnabled ? -eyeY * EYELID_LOWER_TRACK_STRENGTH * 0.5f : 0.0f;

  m_prevUpperY += (targetUpperTracking - m_prevUpperY) * EYELID_SMOOTHING;
  m_prevLowerY += (targetLowerTracking - m_prevLowerY) * EYELID_SMOOTHING;

  // Center the eyelid base at the eye's vertical screen position when tracking.
  // Default (procedural) eyelids use m_smoothedUpperFactor/m_smoothedLowerFactor
  // as absolute screen-row fractions, so they must follow the eye center.
  // Custom eyelids render relative to the circle boundary; eyeYBase cancels
  // in smoothGap (lower - upper), so their output is unaffected.
  float eyeYBase = 0.5f + (m_trackingEnabled ? eyeY * 0.25f : 0.0f);

  // Scale tracking by eyelidGap so tracking vanishes at full closure.
  // This prevents the asymmetric upper/lower tracking strengths from leaving
  // a gap between the eyelids when the eye is looking up or down and blinking.
  m_smoothedUpperFactor = eyeYBase - eyelidGap * 0.5f + m_prevUpperY * eyelidGap;
  m_smoothedLowerFactor = eyeYBase + eyelidGap * 0.5f + m_prevLowerY * eyelidGap;

  m_eyeCenterX = m_displaySize / 2 + (int)(eyeX * (m_displaySize / 4));
  m_eyeCenterY = m_displaySize / 2 + (int)(eyeY * (m_displaySize / 4));
}

/**
 * @brief Paint eyelid pixels into the frame buffer.
 *
 * Dispatches to custom or default eyelid renderer using factors and eye
 * center computed by the preceding prepareFactors() call.
 */
void EyelidRenderer::drawEyelids(uint16_t *frameBuffer)
{
  if (m_hasCustomEyelids && m_config != nullptr)
  {
    float smoothGap = m_smoothedLowerFactor - m_smoothedUpperFactor;
    renderCustomEyelids(smoothGap, m_eyeCenterX, m_eyeCenterY, frameBuffer, m_displaySize, m_eyelidColor);
  }
  else
  {
    renderDefaultEyelids(m_eyeCenterX, m_eyeCenterY, m_smoothedUpperFactor, m_smoothedLowerFactor,
                         frameBuffer, m_displaySize, m_eyelidColor);
  }
}

/** @brief Convenience wrapper: prepareFactors() then drawEyelids(). */
void EyelidRenderer::render(float eyeX, float eyeY, float eyelidGap, uint16_t *frameBuffer)
{
  prepareFactors(eyeX, eyeY, eyelidGap);
  drawEyelids(frameBuffer);
}

/**
 * @brief Render default eyelids as circular segments.
 *
 * Paints all pixels within the eye circle that are at or above upperY or
 * at or below lowerY (both expressed as normalized display positions 0–1).
 * Inclusive row bounds ensure the boundary row is always covered, so
 * upperY==lowerY (full closure) leaves no gap.
 */
void EyelidRenderer::renderDefaultEyelids(int centerX, int centerY, float upperY, float lowerY,
                                          uint16_t *buffer, int size, uint16_t color)
{
  const uint16_t colorOut = m_needsByteSwap ? __builtin_bswap16(color) : color;
  // upperY/lowerY are normalized display positions (0.0=top, 1.0=bottom).
  // Convert to absolute pixel rows; use inclusive bounds so the boundary row
  // is always painted and upperYPos==lowerYPos (full closure) leaves no gap.
  int upperYPos = (int)(upperY * size);
  int lowerYPos = (int)(lowerY * size);
  int eyeRadius = m_eyeRadius;
  int eyeRadiusSq = eyeRadius * eyeRadius;

  int minY = centerY - eyeRadius;
  int maxY = centerY + eyeRadius;
  int minX = centerX - eyeRadius;
  int maxX = centerX + eyeRadius;

  if (minY < 0)
    minY = 0;
  if (maxY >= size)
    maxY = size - 1;
  if (minX < 0)
    minX = 0;
  if (maxX >= size)
    maxX = size - 1;

  for (int y = minY; y <= maxY; y++)
  {
    // Inclusive bounds: boundary row belongs to the nearer eyelid, ensuring
    // full closure when upperYPos == lowerYPos.
    bool aboveUpper = (y <= upperYPos);
    bool belowLower = (y >= lowerYPos);
    if (!aboveUpper && !belowLower)
      continue;

    int dy = y - centerY;
    int dySq = dy * dy;

    for (int x = minX; x <= maxX; x++)
    {
      int dx = x - centerX;
      if (dx * dx + dySq > eyeRadiusSq)
        continue;
      buffer[y * size + x] = colorOut;
    }
  }
}

/**
 * @brief Render custom eyelid shapes from the eye definition table.
 *
 * Table format per column: upper=(outer_top, inner_bottom), lower=(outer_bottom, inner_top),
 * all scaled 0-255. (0,0) sentinel means no eyelid data for that column.
 *
 * For each column within the eye circle, paints eyelid color between the circle
 * boundary and the inner eyelid edge, interpolated by eyelidGap:
 *   eyelidGap=1.0 (open)   → eyelid edge at circle boundary → nothing painted
 *   eyelidGap=0.0 (closed) → eyelid edge at table inner edge → full eyelid painted
 */
void EyelidRenderer::renderCustomEyelids(float eyelidGap, int centerX, int centerY,
                                         uint16_t *buffer, int size, uint16_t color)
{
  if (m_config == nullptr)
    return;

  int eyeRadius = m_eyeRadius;
  int eyeRadiusSq = eyeRadius * eyeRadius;
  float scale = (float)size / 255.0f;
  float gapClosed = 1.0f - eyelidGap;

#if defined(FDEBUG)
  static uint32_t s_lastDbg = 0;
  bool doDbg = (millis() - s_lastDbg) >= 2000;
  if (doDbg)
    s_lastDbg = millis();
#endif

  for (int x = 0; x < size; x++)
  {
    int dx = x - centerX;
    int dxSq = dx * dx;
    if (dxSq > eyeRadiusSq)
      continue;

    int dyMaxSq = eyeRadiusSq - dxSq;
    int dyMax;
    if (dyMaxSq <= 0)
    {
      dyMax = 0;
    }
    else
    {
      dyMax = eyeRadius;
      dyMax = (dyMax + dyMaxSq / dyMax) / 2;
      dyMax = (dyMax + dyMaxSq / dyMax) / 2;
      dyMax = (dyMax + dyMaxSq / dyMax) / 2;
    }

    int circleTop = centerY - dyMax;
    int circleBottom = centerY + dyMax;
    // Convert absolute screen column to eye-relative column for the table lookup.
    // The table was generated for a centered eye; when the eye is off-center,
    // screen column x maps to centered-eye column (x - eyeOffset).
    int tableCol = x - centerX + m_displaySize / 2;
    if (tableCol < 0 || tableCol >= m_displaySize)
      continue;
    int tableIdx = tableCol * 2;

    // Upper eyelid: upper[col*2+1] = inner bottom edge (0 = no data for this column).
    if (m_config->upper != nullptr)
    {
      uint8_t upperEnd = m_config->upper[tableIdx + 1];
      if (upperEnd != 0)
      {
        int upperInnerY = (int)(upperEnd * scale);
        int upperEdge = circleTop + (int)(gapClosed * (float)(upperInnerY - circleTop));
        if (upperEdge > circleTop)
        {
          int yTop = (circleTop < 0) ? 0 : circleTop;
          int yBot = (upperEdge > size) ? size : upperEdge;
          uint16_t colorBE = m_needsByteSwap ? __builtin_bswap16(color) : color;
          for (int y = yTop; y < yBot; y++)
            buffer[y * size + x] = colorBE;
#if defined(FDEBUG)
          if (doDbg && x == centerX)
            Serial.printf("[Eyelid] center col: gapClosed=%.2f circleTop=%d upperInnerY=%d upperEdge=%d (paints %d-%d)\n",
                          gapClosed, circleTop, upperInnerY, upperEdge, yTop, yBot);
#endif
        }
      }
    }

    // Lower eyelid: lower[col*2]=outer_bottom (0=no data), lower[col*2+1]=inner top edge.
    if (m_config->lower != nullptr)
    {
      uint8_t lowerStart = m_config->lower[tableIdx];
      if (lowerStart != 0)
      {
        int lowerInnerY = (int)(m_config->lower[tableIdx + 1] * scale);
        int lowerEdge = circleBottom - (int)(gapClosed * (float)(circleBottom - lowerInnerY));
        if (lowerEdge < circleBottom)
        {
          int yTop = (lowerEdge < 0) ? 0 : lowerEdge;
          int yBot = (circleBottom > size) ? size : circleBottom;
          uint16_t colorBE = m_needsByteSwap ? __builtin_bswap16(color) : color;
          for (int y = yTop; y < yBot; y++)
            buffer[y * size + x] = colorBE;
#if defined(FDEBUG)
          if (doDbg && x == centerX)
            Serial.printf("[Eyelid] center col: lowerInnerY=%d lowerEdge=%d circleBottom=%d (paints %d-%d)\n",
                          lowerInnerY, lowerEdge, circleBottom, yTop, yBot);
#endif
        }
      }
    }
  }
}
