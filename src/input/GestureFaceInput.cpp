/**
 * @file GestureFaceInput.cpp
 * @brief DFRobot Gravity Offline Edge AI Gesture & Face Detection sensor input implementation.
 */
#include "GestureFaceInput.h"
#include <Arduino.h>

GestureFaceInput::GestureFaceInput(uint8_t addr, uint16_t frameW, uint16_t frameH)
    : m_sensor(addr), m_frameW(frameW), m_frameH(frameH)
{
}

/**
 * @brief Initialize the sensor on the Wire bus.
 *
 * Calls DFRobot_GestureFaceDetection_I2C::begin() and prints a startup
 * diagnostic to Serial. Sets m_connected on success.
 *
 * @return true if the sensor responded and initialized successfully.
 */
bool GestureFaceInput::begin()
{
  if (!m_sensor.begin(&Wire))
  {
    Serial.println("GestureFace sensor not found (this is normal if not connected).");
    return false;
  }

  m_connected = true;
  Serial.println("GestureFace sensor initialized.");
  return true;
}

/**
 * @brief Poll the sensor and update face position and tracking state.
 *
 * Reads getFaceNumber() and getFaceScore() from the hardware. When a face
 * is present and the score meets m_minScore, maps the pixel coordinates
 * returned by getFaceLocationX/Y into the [-1, +1] eye-space range and
 * sets m_hasFace = true. Clears m_hasFace when no face qualifies.
 *
 * @return true if a valid face was detected this update.
 */
bool GestureFaceInput::update()
{
  if (!m_connected)
    return false;

  uint16_t count = m_sensor.getFaceNumber();

  if (count > 0 && m_sensor.getFaceScore() >= m_minScore)
  {
    uint16_t fx = m_sensor.getFaceLocationX();
    uint16_t fy = m_sensor.getFaceLocationY();

    // Map camera pixel coordinates to normalized eye space (-1..+1).
    // Camera origin is top-left; positive Y is down. Eye space: -1 = left/up, +1 = right/down.
    m_targetX = constrain(((float)fx / (float)m_frameW) * 2.0f - 1.0f, -1.0f, 1.0f);
    m_targetY = constrain(((float)fy / (float)m_frameH) * 2.0f - 1.0f, -1.0f, 1.0f);
    m_hasFace = true;
  }
  else
  {
    m_hasFace = false;
  }

  return m_hasFace;
}
