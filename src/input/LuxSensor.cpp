/**
 * @file LuxSensor.cpp
 * @brief Implementation of Adafruit MAX44009 ambient light sensor for pupil size control.
 *
 * Uses the Adafruit_MAX44009 library via I2C to read ambient lux values.
 * Auto-detects sensor presence using the library's begin() method.
 * Performs auto-ranging on begin() to establish calibrated min/max bounds.
 * Applies time-based exponential smoothing to pupil factor changes for
 * realistic, gradual iris transitions.
 */
#include <Arduino.h>
#include "LuxSensor.h"

LuxSensor::LuxSensor(TwoWire &wire) : m_sensor(), m_wirePtr(&wire) {}

bool LuxSensor::begin()
{
  // Adafruit_I2CDevice::begin() calls Wire.begin() internally which resets ESP32
  // I2C state, causing the first endTransmission() to return 0 spuriously.
  // Pre-check while the bus is already in a clean initialized state.
  m_wirePtr->beginTransmission(MAX44009_DEFAULT_ADDRESS);
  if (m_wirePtr->endTransmission() != 0)
  {
    m_connected = false;
    Serial.println("[LuxSensor] MAX44009 not found at 0x4A");
    return false;
  }

  if (!m_sensor.begin(MAX44009_DEFAULT_ADDRESS, m_wirePtr))
  {
    m_connected = false;
    Serial.println("[LuxSensor] MAX44009 not found at 0x4A");
    return false;
  }

  m_connected = true;
  m_pupilFactor = 0.5f;
  m_pupilFactorPrev = 0.5f;
  m_lastUpdate = millis();
  Serial.println("[LuxSensor] MAX44009 initialized at 0x4A");

  float luxSum = 0.0f;
  uint8_t validSamples = 0;
  for (int i = 0; i < 16; i++)
  {
    float lux = m_sensor.readLux();
    if (lux >= 0.0f)
    {
      luxSum += lux;
      validSamples++;
    }
    delay(20);
  }

  if (validSamples == 0)
  {
    m_connected = false;
    Serial.println("[LuxSensor] MAX44009: no valid readings during calibration");
    return false;
  }

  float avgLux = luxSum / validSamples;
  m_minLux = avgLux * 0.5f;
  m_maxLux = avgLux * 2.0f;

  if (m_minLux < 0.01f)
    m_minLux = 0.01f;
  if (m_maxLux < m_minLux + 10.0f)
    m_maxLux = m_minLux + 1000.0f;

  Serial.printf("[LuxSensor] Auto-range calibrated: %.2f - %.2f lux\n", m_minLux, m_maxLux);

  return true;
}

bool LuxSensor::update()
{
  if (!m_connected)
  {
    return false;
  }

  float lux = m_sensor.readLux();
  if (lux < 0.0f)
  {
    return false;
  }

  m_rawLux = lux;

  float range = m_maxLux - m_minLux;
  if (range > 0.0f)
  {
    m_normalizedValue = constrain((lux - m_minLux) / range, 0.0f, 1.0f);
  }
  else
  {
    m_normalizedValue = 0.5f;
  }

  if (m_curve != 1.0f)
  {
    m_normalizedValue = powf(m_normalizedValue, m_curve);
  }

  // Bright light -> constricted pupil (high factor), dark -> dilated (low factor)
  float targetPupilFactor = m_normalizedValue;

  // Time-based exponential smoothing: alpha per 100ms
  uint32_t now = millis();
  uint32_t dt = now - m_lastUpdate;
  m_lastUpdate = now;

  // Calculate effective alpha scaled by elapsed time (base alpha per 100ms)
  float alpha = 1.0f - powf(1.0f - m_smoothAlpha, dt / 100.0f);
  alpha = constrain(alpha, 0.0f, 1.0f);

  m_pupilFactorPrev = m_pupilFactor;
  m_pupilFactor = m_pupilFactor + alpha * (targetPupilFactor - m_pupilFactor);

  return true;
}

void LuxSensor::setMinMax(float minVal, float maxVal)
{
  m_minLux = minVal;
  m_maxLux = maxVal;
}