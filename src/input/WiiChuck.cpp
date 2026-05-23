/**
 * @file WiiChuck.cpp
 * @brief Implementation of Wii Nunchuck controller input.
 *
 * Communication via I2C. The Nunchuck requires initialization
 * sequence: 0xF0/0x55 to enter mode, then 0xFB/0x00 to request raw data.
 * Joystick is decoded from bytes 0-1 (centered at 0x80) and buttons from byte 5.
 */
#include "WiiChuck.h"
#include "BoardPins.h"
#include <Arduino.h>

WiiChuckInput::WiiChuckInput(uint8_t address)
    : m_address(address)
{
}

/**
 * @brief Initialize I2C communication with the Nunchuck.
 *
 * The I2C bus (Wire) is expected to already be running from a prior
 * component (SY6970, display, etc.). The Nunchuck supports 100kHz so
 * no clock change is needed when the bus is already at that speed.
 */
bool WiiChuckInput::begin()
{
  // Initialize controller - step 1: enter extended mode
  Wire.beginTransmission(m_address);
  Wire.write(0xF0);
  Wire.write(0x55);
  uint8_t ret1 = Wire.endTransmission();
  if (ret1 != 0)
  {
    return false;
  }

  delay(1);

  // Initialize controller - step 2: request raw data mode
  Wire.beginTransmission(m_address);
  Wire.write(0xFB);
  Wire.write(0x00);
  uint8_t ret2 = Wire.endTransmission();
  if (ret2 != 0)
  {
    return false;
  }

  delay(1);

  readData();
  return true;
}

/**
 * @brief Read and decode joystick and button state from Nunchuck.
 *
 * Requests 6 bytes from the device. Byte 0 = joystick X (0x80 = center),
 * byte 1 = joystick Y (0x80 = center), byte 5 = button bits (bit 0 = Z,
 * bit 1 = C, active-low). Joystick values outside the 10-count deadzone
 * set m_hasStick = true to claim exclusive control.
 */
void WiiChuckInput::readData()
{
  int joyX = 0, joyY = 0;

  Wire.requestFrom((uint8_t)m_address, (uint8_t)6);
  if (Wire.available() >= 6)
  {
    m_status[0] = Wire.read();
    m_status[1] = Wire.read();
    m_status[2] = Wire.read();
    m_status[3] = Wire.read();
    m_status[4] = Wire.read();
    m_status[5] = Wire.read();

    joyX = (int)m_status[0] - 0x80;
    joyY = (int)m_status[1] - 0x80;

    m_zPressed = !(m_status[5] & 0x01);
    m_cPressed = !((m_status[5] >> 1) & 0x01);
  }

  if (abs(joyX) > 10 || abs(joyY) > 10)
  {
    m_hasStick = true;
    m_targetX = constrain((float)joyX / 64.0f, -1.0f, 1.0f);
    m_targetY = constrain((float)joyY / 64.0f, -1.0f, 1.0f);
  }
  else
  {
    m_hasStick = false;
  }

  if (m_zPressed && !m_lastZPressed)
  {
    m_wantsBlink = true;
  }
  m_lastZPressed = m_zPressed;

  if (m_cPressed && !m_lastCPressed)
  {
    m_wantsBoop = true;
  }
  m_lastCPressed = m_cPressed;
}

/**
 * @brief Poll the Nunchuck at ~60Hz.
 *
 * Reads are throttled to prevent bus flooding. Returns true on each
 * actual read so callers know when fresh data is available.
 */
bool WiiChuckInput::update()
{
  uint32_t now = micros();
  static uint32_t lastRead = 0;

  if (now - lastRead > 16667)
  {
    readData();
    lastRead = now;
    return true;
  }
  return false;
}