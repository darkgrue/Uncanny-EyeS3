/**
 * @file LightSensor.h
 * @brief Photoresistor-based ambient light sensor for pupil size control.
 *
 * Reads an LDR (Light Dependent Resistor) voltage divider on an ADC pin to determine
 * ambient brightness. Pupil size inversely tracks light level (bright = constricted,
 * dark = dilated). Auto-detects sensor presence and falls back to autonomous iris
 * animation when no sensor is connected.
 */
#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include "common/EyeState.h"
#include "animation/EyeMovement.h"

#define LIGHT_LOWER_ADC_THRESHOLD 300

/**
 * @brief Ambient light sensor for controlling iris/pupil size.
 *
 * Uses an LDR voltage divider circuit on a GPIO pin. Detects presence automatically
 * by sampling readings and checking for minimum variance (range >= 150 counts).
 * When connected, provides normalized light values for pupil control. When disconnected,
 * the system falls back to autonomous iris animation.
 */
class LightSensor
{
public:
  /**
   * @brief Construct a LightSensor on the specified ADC pin.
   * @param pin ADC pin number connected to the LDR voltage divider.
   */
  explicit LightSensor(int pin);

  /**
   * @brief Initialize sensor and detect connection status.
   * @return true if a sensor is detected, false otherwise.
   */
  bool begin();

  /** @brief Returns true if a photoresistor is currently detected. */
  bool isConnected() const { return m_connected; }

  /** @brief Returns the raw ADC pin number. */
  int getPin() const { return m_pin; }

  /** @brief Returns the most recent raw ADC reading (0-4095 on ESP32). */
  uint16_t getRawValue() const { return m_rawValue; }

  /**
   * @brief Returns normalized light value (0.0 = dark, 1.0 = bright).
   *
   * Applies min/max calibration and power curve after normalization.
   */
  float getNormalizedValue() const { return m_normalizedValue; }

  /** @brief Returns calibrated minimum ADC value from auto-range estimation. */
  uint16_t getMinValue() const { return m_minValue; }

  /** @brief Returns calibrated maximum ADC value from auto-range estimation. */
  uint16_t getMaxValue() const { return m_maxValue; }

  /** @brief Returns the power curve exponent (1.0 = linear). */
  float getCurve() const { return m_curve; }

  /**
   * @brief Set the calibration min/max values manually.
   * @param minVal Calibrated dark value (low ADC = bright)
   * @param maxVal Calibrated bright value (high ADC = dark)
   */
  void setMinMax(uint16_t minVal, uint16_t maxVal);

  /**
   * @brief Set the power curve exponent for response shaping.
   * @param curve Exponent > 1.0 makes pupil react more to dark, < 1.0 more to bright.
   */
  void setCurve(float curve) { m_curve = curve; }

  /**
   * @brief Sample the sensor and update normalized values.
   * @return true if sensor is connected and was updated, false otherwise.
   */
  bool update();

  /**
   * @brief Returns pupil constriction factor (0.0 = fully dilated, 1.0 = fully constricted).
   *
   * This is the value to pass to the eye renderer for pupil size control.
   * Inversely tracks light: more light → smaller pupil.
   */
  float getPupilFactor() const { return m_pupilFactor; }

  /** @brief Noise floor threshold used in detection algorithm. */
  static constexpr int NOISE_FLOOR = 10;

private:
  /** @brief Sample ambient light over ~250ms to establish min/max calibration range. */
  void estimateRange();

  int m_pin;                /**< ADC pin number. */
  bool m_connected = false; /**< True if sensor presence is confirmed. */

  uint16_t m_rawValue = 0;        /**< Latest ADC reading. */
  float m_normalizedValue = 0.0f; /**< Latest normalized value (0.0-1.0). */

  uint16_t m_minValue = 0;    /**< Calibrated dark ADC value. */
  uint16_t m_maxValue = 1023; /**< Calibrated bright ADC value. */
  float m_curve = 1.0f;       /**< Power curve exponent. */

  float m_pupilFactor = 0.5f; /**< Current pupil constriction factor. */
  float m_smoothAlpha = PUPIL_SMOOTH_ALPHA;
  float m_pupilFactorPrev = 0.5f;
  uint32_t m_lastUpdate = 0;
};

#endif