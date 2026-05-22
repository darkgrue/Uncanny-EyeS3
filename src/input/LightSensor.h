#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include "common/EyeState.h"

class LightSensor
{
public:
  LightSensor(int pin);

  bool begin();
  bool isConnected() const { return m_connected; }
  int getPin() const { return m_pin; }

  uint16_t getRawValue() const { return m_rawValue; }
  float getNormalizedValue() const { return m_normalizedValue; }
  uint16_t getMinValue() const { return m_minValue; }
  uint16_t getMaxValue() const { return m_maxValue; }
  float getCurve() const { return m_curve; }

  void setMinMax(uint16_t minVal, uint16_t maxVal);
  void setCurve(float curve) { m_curve = curve; }

  bool update();

  float getPupilFactor() const { return m_pupilFactor; }

  static constexpr int NOISE_FLOOR = 10;

private:
  int m_pin;
  bool m_connected = false;

  uint16_t m_rawValue = 0;
  float m_normalizedValue = 0.0f;

  uint16_t m_minValue = 0;
  uint16_t m_maxValue = 1023;
  float m_curve = 1.0f;

  float m_pupilFactor = 0.5f;

  void estimateRange();
};

#endif