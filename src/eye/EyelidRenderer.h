/**
 * @file EyelidRenderer.h
 * @brief Eyelid animation renderer with position tracking.
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
 * natural "looking up/down" effect.
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
   * @param needsByteSwap True if the frame buffer requires big-endian pixels
   *   (see DisplayHAL::needsByteSwappedPixels()).
   */
  void begin(int displaySize, uint16_t eyeRadius, const EyelidConfig &config, bool needsByteSwap = false);

  /** @brief Enable/disable eyelid tracking of pupil position. */
  void setTrackingEnabled(bool enabled) { m_trackingEnabled = enabled; }

  /**
   * @brief Compute smoothed eyelid factors from eye position and blink gap.
   *
   * Call before the main render loop so getUpperRow()/getLowerRow() return
   * valid bounds for eyelid-zone row skipping. Stores the eye center and
   * computed gap for subsequent drawEyelids().
   */
  void prepareFactors(float eyeX, float eyeY, float eyelidGap);

  /** @brief Paint eyelid pixels into the frame buffer. Call after the main render loop. */
  void drawEyelids(uint16_t *frameBuffer);

  /** @brief Convenience wrapper: prepareFactors() then drawEyelids() in one call. */
  void render(float eyeX, float eyeY, float eyelidGap, uint16_t *frameBuffer);

  /** @brief Current upper eyelid factor after smoothing. */
  float getUpperLidFactor() const { return m_smoothedUpperFactor; }

  /** @brief Current lower eyelid factor after smoothing. */
  float getLowerLidFactor() const { return m_smoothedLowerFactor; }

  /** @brief Pixel row of the upper eyelid boundary (valid after prepareFactors()). */
  int getUpperRow(int displaySize) const { return (int)(m_smoothedUpperFactor * displaySize); }

  /** @brief Pixel row of the lower eyelid boundary (valid after prepareFactors()). */
  int getLowerRow(int displaySize) const { return (int)(m_smoothedLowerFactor * displaySize); }

private:
  /** @brief Draw default circular arc eyelids. */
  void renderDefaultEyelids(int centerX, int centerY, float upperY, float lowerY,
                            uint16_t *buffer, int size, uint16_t color);

  /** @brief Draw custom eyelid shapes from eye definition. */
  void renderCustomEyelids(float eyelidGap, int centerX, int centerY,
                           uint16_t *buffer, int size, uint16_t color);

  int m_displaySize = 0; // Display dimension in pixels
  int m_eyeRadius = 0;   // Eye circle radius

  const EyelidConfig *m_config = nullptr; // Eyelid geometry config

  bool m_hasCustomEyelids = false; // True if eye has custom eyelid data
  bool m_trackingEnabled = true;   // Enable pupil tracking
  bool m_needsByteSwap = false;    // True if frame buffer pixels must be big-endian

  float m_smoothedUpperFactor = 1.0f; // Smoothed upper eyelid factor
  float m_smoothedLowerFactor = 1.0f; // Smoothed lower eyelid factor

  float m_prevUpperY = 0.0f; // Smoothed tracking offset for upper eyelid
  float m_prevLowerY = 0.0f; // Smoothed tracking offset for lower eyelid

  uint16_t m_eyelidColor = 0; // Eyelid RGB565 color
  int m_eyeCenterX = 0;       // Eye center X stored by prepareFactors()
  int m_eyeCenterY = 0;       // Eye center Y stored by prepareFactors()
};

#endif // EYELID_RENDERER_H