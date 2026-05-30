/**
 * @file LuxSensor.h
 * @brief Adafruit MAX44009 ambient light sensor for pupil size control.
 *
 * Reads ambient light via I2C using the Adafruit MAX44009 library.
 * Pupil size inversely tracks light level (bright = constricted, dark = dilated).
 * Auto-detects sensor presence and falls back to autonomous iris animation when
 * no sensor is connected.
 */
#ifndef LUX_SENSOR_H
#define LUX_SENSOR_H

#include <Wire.h>
#include <Adafruit_MAX44009.h>
#include "common/EyeState.h"
#include "animation/EyeMovement.h"

class LuxSensor
{
public:
  /**
   * @brief Construct a LuxSensor on the specified TwoWire bus.
   * @param wire Reference to the TwoWire instance (e.g., Wire or Wire1).
   */
  explicit LuxSensor(TwoWire &wire);

  /**
   * @brief Initialize sensor and detect connection status.
   * @return true if a sensor is detected, false otherwise.
   */
  bool begin();

  /** @brief Returns true if a MAX44009 sensor is currently detected. */
  bool isConnected() const { return m_connected; }

  /** @brief Returns the most recent raw lux value. */
  float getRawLux() const { return m_rawLux; }

  /** @brief Returns normalized light value (0.0 = dark, 1.0 = bright). */
  float getNormalizedValue() const { return m_normalizedValue; }

  /** @brief Returns calibrated minimum lux value. */
  float getMinLux() const { return m_minLux; }

  /** @brief Returns calibrated maximum lux value. */
  float getMaxLux() const { return m_maxLux; }

  /**
   * @brief Set the calibration min/max values manually.
   * @param minVal Calibrated dark value (low lux = dark)
   * @param maxVal Calibrated bright value (high lux = bright)
   */
  void setMinMax(float minVal, float maxVal);

  /**
   * @brief Set the power curve exponent for response shaping.
   * @param curve Exponent > 1.0 makes pupil react more to dark, < 1.0 more to bright.
   */
  void setCurve(float curve) { m_curve = curve; }

  /** @brief Returns the power curve exponent. */
  float getCurve() const { return m_curve; }

  /**
   * @brief Sample the sensor and update normalized values.
   * @return true if sensor is connected and was updated, false otherwise.
   */
  bool update();

  /**
   * @brief Returns pupil constriction factor (0.0 = fully dilated, 1.0 = fully constricted).
   *
   * This is the value to pass to the eye renderer for pupil size control.
   * Inversely tracks light: more light -> smaller pupil.
   */
  float getPupilFactor() const { return m_pupilFactor; }

private:
  Adafruit_MAX44009 m_sensor;
  TwoWire *m_wirePtr = nullptr;
  bool m_connected = false;

  float m_rawLux = 0.0f;
  float m_normalizedValue = 0.0f;

  float m_minLux = 0.0f;
  float m_maxLux = 1000.0f;
  float m_curve = 1.0f;

  float m_pupilFactor = 0.5f;               // Current smoothly-filtered pupil factor
  float m_pupilFactorPrev = 0.5f;           // Previous filtered value for smoothing
  float m_smoothAlpha = PUPIL_SMOOTH_ALPHA; // Exponential smoothing coefficient (lower = smoother/slower)
  uint32_t m_lastUpdate = 0;                // Last update timestamp for time-based smoothing
};

#endif