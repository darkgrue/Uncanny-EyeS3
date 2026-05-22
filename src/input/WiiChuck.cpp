#include "WiiChuck.h"
#include "config/BoardPins.h"
#include <Arduino.h>

WiiChuckInput::WiiChuckInput(uint8_t address)
    : m_address(address)
{
}

bool WiiChuckInput::begin()
{
  // Only call Wire.begin() if it hasn't been started yet
  // This prevents "Bus already started" warnings when SY6970 or other
  // peripherals have already initialized the I2C bus
  // Note: Wire doesn't have a native "isStarted()" check, so we track it ourselves
  // If Wire was already started by another component, we just set the clock
  static bool wireStarted = false;
  if (!wireStarted)
  {
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    wireStarted = true;
  }
  else
  {
    // Wire already started - just ensure clock is set
  }
  Wire.setClock(400000);

  // Initialize controller
  Wire.beginTransmission(m_address);
  Wire.write(0xF0);
  Wire.write(0x55);
  if (Wire.endTransmission() != 0)
  {
    return false;
  }

  delay(1);

  Wire.beginTransmission(m_address);
  Wire.write(0xFB);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0)
  {
    return false;
  }

  delay(1);

  // Initial read
  readData();
  return true;
}

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

    // Decode joystick (typically 0x80 is center)
    joyX = (int)m_status[0] - 0x80;
    joyY = (int)m_status[1] - 0x80;

    // Z button (bit 0) - held for close
    m_zPressed = !(m_status[5] & 0x01);

    // C button (bit 1) - held for wide
    m_cPressed = !((m_status[5] >> 1) & 0x01);
  }

  // Convert to normalized values
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

  // Z button edge-triggered blink (on press only)
  if (m_zPressed && !m_lastZPressed)
  {
    m_wantsBlink = true;
  }
  m_lastZPressed = m_zPressed;

  // C button edge-triggered boop (on press only)
  if (m_cPressed && !m_lastCPressed)
  {
    m_wantsBoop = true;
  }
  m_lastCPressed = m_cPressed;
}

bool WiiChuckInput::update()
{
  uint32_t now = micros();
  static uint32_t lastRead = 0;

  // Read at ~60Hz
  if (now - lastRead > 16667)
  {
    readData();
    lastRead = now;
    return true;
  }
  return false;
}