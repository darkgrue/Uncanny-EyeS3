/**
 * @file EyelidRenderer.cpp
 * @brief Eyelid animation renderer with smoothstep tracking and squint.
 *
 * Renders upper and lower eyelids as overlays on the eye frame buffer.
 * Supports two modes: custom eyelid tables from the eye definition, and
 * a math-based curved boundary fallback. Eyelid position tracks the pupil
 * (upper moves down when looking up, lower moves up when looking down) and
 * squinting reduces the eyelid gap. Uses smoothstep easing for smooth
 * animation transitions between positions.
 */
#include "EyelidRenderer.h"
#include "animation/EyeMovement.h"
#include <cmath>

EyelidRenderer::EyelidRenderer()
    : m_displaySize(0), m_eyeRadius(0), m_config(nullptr),
      m_hasCustomEyelids(false), m_trackingEnabled(true), m_squint(false),
      m_smoothedUpperFactor(1.0f), m_smoothedLowerFactor(1.0f),
      m_prevUpperY(0.5f), m_prevLowerY(0.5f), m_eyelidColor(0)
{
}

/**
 * @brief Initialize renderer with display dimensions and eye geometry.
 * @param displaySize Display width/height in pixels.
 * @param eyeRadius Eye circle radius in pixels.
 * @param config Eyelid geometry and color configuration.
 */
void EyelidRenderer::begin(int displaySize, uint16_t eyeRadius, const EyelidConfig &config)
{
  m_displaySize = displaySize;
  m_eyeRadius = eyeRadius;
  m_config = &config;

  m_hasCustomEyelids = (config.upper != nullptr && config.lower != nullptr);
  m_eyelidColor = config.color;

  // Compute normal gap from normalClosure (gap = 1.0 - closure)
  float normalGap = 1.0f - config.normalClosure;

  m_smoothedUpperFactor = normalGap;
  m_smoothedLowerFactor = normalGap;
  m_prevUpperY = 0.5f - (normalGap * 0.5f);
  m_prevLowerY = 0.5f + (normalGap * 0.5f);
}

/**
 * @brief Compute the normalized Y position of the upper eyelid.
 *
 * Base position is centered around 0.5 with the gap applied symmetrically.
 * When tracking is enabled, looking upward shifts the upper lid downward
 * by a fraction of the eye Y position.
 */
float EyelidRenderer::calculateUpperLidY(float eyeY, float gap)
{
  float baseUpperY = 0.5f - (gap * 0.5f);
  if (m_trackingEnabled)
  {
    baseUpperY += -eyeY * EYELID_UPPER_TRACK_STRENGTH * 0.5f;
  }
  return baseUpperY;
}

/**
 * @brief Compute the normalized Y position of the lower eyelid.
 *
 * Base position is centered around 0.5 with the gap applied symmetrically.
 * When tracking is enabled, looking downward shifts the lower lid upward
 * by a fraction of the eye Y position.
 */
float EyelidRenderer::calculateLowerLidY(float eyeY, float gap)
{
  float baseLowerY = 0.5f + (gap * 0.5f);
  if (m_trackingEnabled)
  {
    baseLowerY += -eyeY * EYELID_LOWER_TRACK_STRENGTH * 0.5f;
  }
  return baseLowerY;
}

/**
 * @brief Render eyelids into the frame buffer.
 *
 * Applies squint modifier to the eyelid gap if enabled, computes target
 * Y positions with optional smoothstep easing, applies exponential
 * smoothing, then delegates to either custom eyelid rendering or the
 * default curved boundary renderer.
 */
void EyelidRenderer::render(float eyeX, float eyeY, float eyelidGap, uint16_t *frameBuffer)
{
  if (m_squint)
  {
    eyelidGap *= EYELID_SQUINT_FACTOR;
  }

  float targetUpperY = calculateUpperLidY(eyeY, eyelidGap);
  float targetLowerY = calculateLowerLidY(eyeY, eyelidGap);

#if BLINK_USE_SMOOTHSTEP
  constexpr float smoothK = 3.0f;
  auto smoothstep = [](float t)
  {
    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f
                                       : t;
    return t * t * (3.0f - 2.0f * t);
  };
  targetUpperY = smoothstep(targetUpperY);
  targetLowerY = smoothstep(targetLowerY);
  (void)smoothK;
#else
  (void)0;
#endif

  float upperDelta = targetUpperY - m_prevUpperY;
  float lowerDelta = targetLowerY - m_prevLowerY;

  float upperStep = upperDelta * EYELID_SMOOTHING;
  float lowerStep = lowerDelta * EYELID_SMOOTHING;

  m_smoothedUpperFactor = m_prevUpperY + upperStep;
  m_smoothedLowerFactor = m_prevLowerY + lowerStep;

  m_prevUpperY = m_smoothedUpperFactor;
  m_prevLowerY = m_smoothedLowerFactor;

  int centerX = m_displaySize / 2;
  int centerY = m_displaySize / 2;
  int offsetX = (int)(eyeX * (m_displaySize / 4));
  int offsetY = (int)(eyeY * (m_displaySize / 4));
  int eyeCenterX = centerX + offsetX;
  int eyeCenterY = centerY + offsetY;

  if (m_hasCustomEyelids && m_config != nullptr)
  {
    renderCustomEyelids(eyelidGap, eyeCenterX, eyeCenterY, frameBuffer, m_displaySize, m_eyelidColor);
  }
  else
  {
    float upperYNorm = m_smoothedUpperFactor;
    float lowerYNorm = m_smoothedLowerFactor;
    renderDefaultEyelids(eyeCenterX, eyeCenterY, upperYNorm, lowerYNorm, frameBuffer, m_displaySize, m_eyelidColor);
  }
}

/**
 * @brief Render default curved arc eyelids using math-based boundary.
 *
 * Iterates pixels within the eye's circular bounding box. For each pixel
 * above the upper boundary or below the lower boundary, tests whether it
 * falls within the curved eyelid shape (using normalized distance from center)
 * and fills with the eyelid color if so.
 */
void EyelidRenderer::renderDefaultEyelids(int centerX, int centerY, float upperY, float lowerY,
                                          uint16_t *buffer, int size, uint16_t color)
{
  int upperYPos = (int)(upperY * size);
  int lowerYPos = (int)(lowerY * size);
  int eyeRadius = m_eyeRadius;

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

  int eyeRadiusSq = eyeRadius * eyeRadius;

  float cornerPinch = 0.35f;
  float cornerY = upperY + (lowerY - upperY) * cornerPinch;
  (void)cornerPinch;
  int cornerYPos = (int)(cornerY * size);
  (void)cornerYPos;
  float invRadius = 1.0f / (float)eyeRadius;
  (void)cornerY;

  for (int y = minY; y <= maxY; y++)
  {
    int dy = y - centerY;
    int dySq = dy * dy;
    bool aboveUpper = (y < upperYPos);
    bool belowLower = (y > lowerYPos);
    if (!aboveUpper && !belowLower)
      continue;

    for (int x = minX; x <= maxX; x++)
    {
      int dx = x - centerX;
      int distSq = dx * dx + dySq;
      if (distSq > eyeRadiusSq)
        continue;

      bool occluded = false;
      if (aboveUpper)
      {
        float centerToY = (float)dy * invRadius;
        float lidBoundary = (upperY - 0.5f) * 2.0f;
        occluded = centerToY < lidBoundary;
      }
      else if (belowLower)
      {
        float centerToY = (float)dy * invRadius;
        float lidBoundary = (lowerY - 0.5f) * 2.0f;
        occluded = centerToY > lidBoundary;
      }

      if (occluded)
      {
        buffer[y * size + x] = color;
      }
    }
  }
}

/**
 * @brief Render custom eyelid shapes from the eye definition table.
 *
 * Uses per-column (startY, endY) pairs stored in the EyelidConfig.
 * Values of 255 indicate "no custom data" — falls back to a procedural
 * boundary based on blinkFactor. Otherwise, eyelid edges are computed
 * by scaling the table values by the current eye size.
 */
void EyelidRenderer::renderCustomEyelids(float blinkFactor, int centerX, int centerY,
                                         uint16_t *buffer, int size, uint16_t color)
{
  if (m_config == nullptr)
    return;

  int effectiveUpper = (int)(blinkFactor * size * 0.5f);
  int effectiveLower = size - effectiveUpper;
  int mapWidth = size;

  for (int x = 0; x < mapWidth; x++)
  {
    int tableIdx = x * 2;
    uint8_t upperStart = 0, upperEnd = 0, lowerStart = 0, lowerEnd = 0;

    if (m_config->upper != nullptr)
    {
      upperStart = m_config->upper[tableIdx];
      upperEnd = m_config->upper[tableIdx + 1];
    }
    if (m_config->lower != nullptr)
    {
      lowerStart = m_config->lower[tableIdx];
      lowerEnd = m_config->lower[tableIdx + 1];
    }

    if (upperStart == 255 || upperEnd == 255)
    {
      upperStart = 0;
      upperEnd = (uint8_t)(blinkFactor * size * 0.5f);
    }
    if (lowerStart == 255 || lowerEnd == 255)
    {
      lowerStart = (uint8_t)(size - blinkFactor * size * 0.5f);
      lowerEnd = size;
    }

    int upperLidY = (int)centerY + (int)upperStart - (int)(effectiveUpper * (1.0f - (float)upperEnd / (float)size));
    int lowerLidY = (int)centerY + (int)lowerStart - (int)((size - effectiveLower) * (1.0f - (float)lowerEnd / (float)size));

    upperLidY = (upperLidY < 0) ? 0 : (upperLidY >= size) ? size - 1
                                                          : upperLidY;
    lowerLidY = (lowerLidY < 0) ? 0 : (lowerLidY >= size) ? size - 1
                                                          : lowerLidY;

    for (int y = upperLidY; y <= lowerLidY; y++)
    {
      if (y >= 0 && y < size)
      {
        int idx = y * size + x;
        buffer[idx] = color;
      }
    }
  }
}
