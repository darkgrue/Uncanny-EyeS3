#include <Arduino.h>
#include "LightSensor.h"

LightSensor::LightSensor(int pin) : m_pin(pin) {}

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

  // A real photoresistor in a voltage divider shows a wide range of readings
  // when light changes (typically 200+ counts from dark to bright).
  // A floating/unconnected pin shows small noise (typically <150 counts range).
  // Require range > 150 to consider it a real sensor.

  if (range < 150)
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

void LightSensor::setMinMax(uint16_t minVal, uint16_t maxVal)
{
  m_minValue = minVal;
  m_maxValue = maxVal;
}