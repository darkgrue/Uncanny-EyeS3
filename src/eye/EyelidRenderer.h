/**
 * @file EyelidRenderer.h
 * @brief Eyelid animation renderer with tracking and squint support.
 *
 * Draws upper and lower eyelids with smooth tracking of pupil position, and
 * supports a squint modifier for expressive animations. Can use default
 * procedural eyelids or custom eyelid shapes from the eye definition.
 */
#ifndef EYELID_RENDERER_H
#define EYELID_RENDERER_H

#include <stdint.h>
#include <Arduino.h>
#include "common/EyeState.h"
#include "common/DisplayGeometry.h"
#include "eyes.h"

/**
 * @brief Renders upper and lower eyelids with position tracking.
 *
 * Supports two modes: default procedural eyelids (circular arc) and custom
 * eyelids from the EyeDefinition. Eyelid position tracks pupil to create a
 * natural "looking up/down" effect. Squinting reduces the eyelid gap.
 */
class EyelidRenderer
{
public:
  EyelidRenderer();

  /**
   * @brief Initialize with display dimensions and eye geometry.
   * @param displaySize Display width/height in pixels.
   * @param eyeRadius Radius of the eye circle in pixels.
   * @param config Eyelid geometry and color configuration.
   */
  void begin(int displaySize, uint16_t eyeRadius, const EyelidConfig &config);

  /** @brief Enable/disable eyelid tracking of pupil position. */
  void setTrackingEnabled(bool enabled) { m_trackingEnabled = enabled; }

  /** @brief Enable/disable squint expression. */
  void setSquint(bool squint) { m_squint = squint; }

  /**
   * @brief Render eyelids into the frame buffer.
   * @param eyeX Current eye X position.
   * @param eyeY Current eye Y position.
   * @param eyelidGap Open fraction (0.0 = fully closed, 1.0 = fully open).
   * @param frameBuffer Pointer to the RGB565 frame buffer.
   */
  void render(float eyeX, float eyeY, float eyelidGap, uint16_t *frameBuffer);

  /** @brief Current upper eyelid factor after smoothing. */
  float getUpperLidFactor() const { return m_smoothedUpperFactor; }

  /** @brief Current lower eyelid factor after smoothing. */
  float getLowerLidFactor() const { return m_smoothedLowerFactor; }

private:
  /** @brief Draw default circular arc eyelids. */
  void renderDefaultEyelids(int centerX, int centerY, float upperY, float lowerY,
                            uint16_t *buffer, int size, uint16_t color);

  /** @brief Draw custom eyelid shapes from eye definition. */
  void renderCustomEyelids(float blinkFactor, int centerX, int centerY,
                           uint16_t *buffer, int size, uint16_t color);

  /** @brief Compute Y position of upper eyelid given eye position and gap. */
  float calculateUpperLidY(float eyeY, float gap);

  /** @brief Compute Y position of lower eyelid given eye position and gap. */
  float calculateLowerLidY(float eyeY, float gap);

  int m_displaySize = 0; // Display dimension in pixels
  int m_eyeRadius = 0;   // Eye circle radius

  const EyelidConfig *m_config = nullptr; // Eyelid geometry config

  bool m_hasCustomEyelids = false; // True if eye has custom eyelid data
  bool m_trackingEnabled = true;   // Enable pupil tracking
  bool m_squint = false;           // Apply squint modifier

  float m_smoothedUpperFactor = 1.0f; // Smoothed upper eyelid factor
  float m_smoothedLowerFactor = 1.0f; // Smoothed lower eyelid factor

  float m_prevUpperY = 0.5f; // Previous upper Y for smoothing
  float m_prevLowerY = 0.5f; // Previous lower Y for smoothing

  uint16_t m_eyelidColor = 0; // Eyelid RGB565 color
};

#endif // EYELID_RENDERER_H