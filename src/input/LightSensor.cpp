/**
 * @file LightSensor.cpp
 * @brief Implementation of ambient light sensor for pupil size control.
 *
 * Auto-detects sensor connection by sampling ADC readings over ~120ms and
 * requiring average reading above LIGHT_LOWER_ADC_THRESHOLD. When connected, performs
 * a 16-sample calibration to establish min/max bounds for normalization.
 */
#include <Arduino.h>
#include "LightSensor.h"

LightSensor::LightSensor(int pin) : m_pin(pin) {}

/**
 * @brief Initialize the light sensor and detect connection status.
 *
 * Takes 8 samples spaced 15ms apart (~120ms total). A floating pin reads near zero
 * on average; a connected photoresistor in any lighting reads above
 * LIGHT_LOWER_ADC_THRESHOLD due to the pull-up side of the voltage divider.
 */
bool LightSensor::begin()
{
  pinMode(m_pin, INPUT);

  uint16_t samples[8];
  for (int i = 0; i < 8; i++)
  {
    samples[i] = analogRead(m_pin);
    delay(15);
  }

  uint16_t minSample = 4095;
  uint16_t maxSample = 0;
  uint32_t sum = 0;

  for (int i = 0; i < 8; i++)
  {
    if (samples[i] < minSample)
      minSample = samples[i];
    if (samples[i] > maxSample)
      maxSample = samples[i];
    sum += samples[i];
  }

  uint16_t avg = sum / 8;
  uint16_t range = maxSample - minSample;

  if (avg < LIGHT_LOWER_ADC_THRESHOLD)
  {
    m_connected = false;
    Serial.printf("[LightSensor] Pin %d: No sensor connected (avg: %d, range: %d)\n", m_pin, avg, range);
  }
  else
  {
    m_connected = true;
    Serial.printf("[LightSensor] Pin %d: Sensor connected (avg: %d, range: %d)\n", m_pin, avg, range);
  }

  m_rawValue = avg;
  estimateRange();

  return m_connected;
}

/**
 * @brief Establish calibration range for the connected sensor.
 *
 * Collects 16 samples spaced 20ms apart (~320ms total) to find the ambient light
 * dynamic range. If minSample > 50 and range > 100, uses those as calibrated bounds.
 * Otherwise defaults to 0-1023 (full ADC range).
 */
void LightSensor::estimateRange()
{
  if (!m_connected)
  {
    m_minValue = 0;
    m_maxValue = 1023;
    return;
  }

  uint16_t samples[16];
  for (int i = 0; i < 16; i++)
  {
    samples[i] = analogRead(m_pin);
    delay(20);
  }

  uint16_t minSample = 4095;
  uint16_t maxSample = 0;
  uint32_t sum = 0;

  for (int i = 0; i < 16; i++)
  {
    if (samples[i] < minSample)
      minSample = samples[i];
    if (samples[i] > maxSample)
      maxSample = samples[i];
    sum += samples[i];
  }

  uint16_t avg = sum / 16;

  if (minSample > 50 && maxSample - minSample > 100)
  {
    m_minValue = minSample;
    m_maxValue = maxSample;
    Serial.printf("[LightSensor] Calibrated range: %d - %d (avg: %d)\n", m_minValue, m_maxValue, avg);
  }
  else
  {
    m_minValue = 0;
    m_maxValue = 1023;
    Serial.printf("[LightSensor] Using default range: 0 - 1023\n");
  }

  m_rawValue = avg;
}

/**
 * @brief Sample the sensor and compute the current pupil factor.
 *
 * Reads raw ADC, normalizes to 0.0-1.0 using calibrated min/max, applies optional
 * power curve, then computes pupilFactor = 1.0 - normalizedValue.
 *
 * @return true if sensor is connected and was updated, false otherwise.
 */
bool LightSensor::update()
{
  if (!m_connected)
  {
    return false;
  }

  m_rawValue = analogRead(m_pin);

  uint16_t range = m_maxValue - m_minValue;
  if (range > 0)
  {
    m_normalizedValue = constrain((float)(m_rawValue - m_minValue) / (float)range, 0.0f, 1.0f);
  }
  else
  {
    m_normalizedValue = 0.5f;
  }

  if (m_curve != 1.0f)
  {
    m_normalizedValue = powf(m_normalizedValue, m_curve);
  }

  m_pupilFactor = 1.0f - m_normalizedValue;

  return true;
}

/**
 * @brief Manually override the calibration min/max values.
 */
void LightSensor::setMinMax(uint16_t minVal, uint16_t maxVal)
{
  m_minValue = minVal;
  m_maxValue = maxVal;
}