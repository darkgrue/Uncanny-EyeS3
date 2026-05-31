/**
 * @file GestureFaceInput.cpp
 * @brief DFRobot Gravity Offline Edge AI Gesture & Face Detection sensor input implementation.
 */
#include "GestureFaceInput.h"
#include <Arduino.h>

GestureFaceInput::GestureFaceInput(uint8_t addr, uint16_t frameW, uint16_t frameH, TwoWire *wire)
    : m_sensor(addr), m_wire(wire), m_addr(addr), m_frameW(frameW), m_frameH(frameH)
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
  // Probe the I2C address before calling m_sensor.begin() to avoid the DFRobot
  // library unconditionally re-calling Wire.begin() on an already-running bus,
  // which puts the ESP32 I2C driver into ESP_ERR_INVALID_STATE and generates
  // spurious error logs even when no sensor is connected.
  m_wire->beginTransmission(m_addr);
  if (m_wire->endTransmission() != 0)
  {
    Serial.println("[GestureFaceInput] Gesture & Face Detection Sensor not found (this is normal if not connected).");
    return false;
  }

  if (!m_sensor.begin(m_wire))
  {
    Serial.println("[GestureFaceInput] Gesture & Face Detection Sensor not found (this is normal if not connected).");
    return false;
  }

  m_connected = true;
  Serial.printf("[GestureFaceInput] GestureFace sensor initialized (VID: 0x%04X | PID: 0x%04X).\n", m_sensor.getVid(), m_sensor.getPid());
  return true;
}

/**
 * @brief Poll the sensor and update face position and tracking state.
 *
 * Rate-limited to POLL_INTERVAL_MS (100ms / 10 Hz) because each I2C read in
 * the DFRobot library blocks for ~5ms; calling at 120 FPS would block the
 * render task for 20ms per frame. Between polls the last cached result is
 * returned unchanged.
 *
 * 0xFFFF is the DFRobot library's error sentinel for a failed I2C read. A
 * failed getFaceNumber() read returns 0xFFFF which would pass the "> 0" check
 * and falsely claim a face is present, so all reads are validated.
 *
 * @return true if a valid face was detected this update.
 */
bool GestureFaceInput::update()
{
  if (!m_connected)
    return false;

  uint32_t now = millis();
  if (now - m_lastPollMs < POLL_INTERVAL_MS)
    return m_hasFace; // return cached result between polls

  m_lastPollMs = now;

  uint16_t count = m_sensor.getFaceNumber();
  if (count == 0xFFFF || count == 0)
  {
    m_hasFace = false;
    return false;
  }

  uint16_t score = m_sensor.getFaceScore();
  if (score == 0xFFFF || score < m_minScore)
  {
    m_hasFace = false;
    return false;
  }

  uint16_t fx = m_sensor.getFaceLocationX(); // Expect fx in range 0...640, camera center at 320
  uint16_t fy = m_sensor.getFaceLocationY(); // Expect fy in range 0...480, camera center at 240
  if (fx == 0xFFFF || fy == 0xFFFF)
  {
    m_hasFace = false;
    return false;
  }

#if defined(FDEBUG)
  static uint32_t lastDebug = 0;
  if (now - lastDebug > 500)
  {
    Serial.printf("[GestureFace] faces=%u score=%u x=%u y=%u -> targetX=%.2f targetY=%.2f\n",
                  count, score, fx, fy,
                  ((float)fx / (float)m_frameW) * 2.0f - 1.0f,
                  ((float)fy / (float)m_frameH) * 2.0f - 1.0f);
    lastDebug = now;
  }
#endif

  // Map camera coordinates to normalized eye space (-1..+1).
  // Camera origin is top-left; positive Y is down.
  m_targetX = constrain(((float)fx / (float)m_frameW) * 2.0f - 1.0f, -1.0f, 1.0f);
  m_targetY = constrain(((float)fy / (float)m_frameH) * 2.0f - 1.0f, -1.0f, 1.0f);
  m_hasFace = true;

  return m_hasFace;
}
