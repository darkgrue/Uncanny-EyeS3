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

/**
 * @brief Construct a WiiChuckInput with the given I2C address.
 * @param address I2C address of the Nunchuck peripheral (default 0x52).
 */
WiiChuckInput::WiiChuckInput(uint8_t address, TwoWire &wire)
    : m_wire(wire), m_address(address)
{
}

/**
 * @brief Initialize I2C communication with the Nunchuck.
 * @return true if the controller acknowledged the handshake and was identified
 *         as a NUNCHUCK; false on I2C error or unexpected controller type.
 */
bool WiiChuckInput::begin()
{
  // The Nunchuck needs time to stabilise after power-on before it will ACK
  // the handshake. 100 ms covers cold-start and warm-reset races.
  delay(100);

  constexpr int     MAX_ATTEMPTS   = 5;
  constexpr uint32_t RETRY_DELAY_MS = 50;

  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++)
  {
    // Step 1: enter unencrypted mode (0xF0 / 0x55)
    m_wire.beginTransmission(m_address);
    m_wire.write(0xF0);
    m_wire.write(0x55);
    uint8_t err = m_wire.endTransmission(true);
    if (err != 0)
    {
      Serial.printf("[WiiChuckInput] attempt %d/%d: mode-enter NACK (err %u), retrying\n",
                    attempt, MAX_ATTEMPTS, err);
      delay(RETRY_DELAY_MS);
      continue;
    }

    // Step 2: request raw (unencrypted) data (0xFB / 0x00)
    m_wire.beginTransmission(m_address);
    m_wire.write(0xFB);
    m_wire.write(0x00);
    err = m_wire.endTransmission(true);
    if (err != 0)
    {
      Serial.printf("[WiiChuckInput] attempt %d/%d: raw-mode NACK (err %u), retrying\n",
                    attempt, MAX_ATTEMPTS, err);
      delay(RETRY_DELAY_MS);
      continue;
    }

    // Handshake succeeded — confirm controller type.
    if (identifyController() == NUNCHUCK)
    {
      Serial.println("[WiiChuckInput] WiiChuck initialized successfully.");
      return true;
    }

    Serial.println("[WiiChuckInput] WARNING: Unexpected controller type.");
    return false;
  }

  Serial.printf("[WiiChuckInput] ERROR: handshake failed after %d attempts.\n", MAX_ATTEMPTS);
  return false;
}

/**
 * @brief Probe the 0xFA identification register and classify the controller.
 * @return The detected ControllerType, or UnknownChuck if the ID bytes do not
 *         match any known peripheral.
 */
ControllerType WiiChuckInput::identifyController()
{
  Serial.println("[WiiChuckInput] Reading controller identification bytes...");
  m_wire.beginTransmission(m_address);
  m_wire.write(0xFA);
  if (m_wire.endTransmission(true) != 0)
  {
    Serial.println("[WiiChuckInput] ERROR: Failed to request controller ID!");
    return UnknownChuck;
  }

  if (m_wire.requestFrom(m_address, (uint8_t)6) < 6)
  {
    Serial.println("[WiiChuckInput] ERROR: Failed to read controller ID!");
    m_wire.endTransmission(true);
    return UnknownChuck;
  }

  Serial.print("[WiiChuckInput] Controller ID: ");
  for (uint8_t i = 0; i < 6; i++)
  {
    m_status[i] = m_wire.read();
    Serial.print(m_status[i], HEX);
  }
  Serial.println(".");

  m_wire.endTransmission(true);

  // Nunchuck: bytes 4-5 = 0x00 0x00
  if (m_status[4] == 0x00 && m_status[5] == 0x00)
    return NUNCHUCK;

  // Classic Controller: bytes 4-5 = 0x01 0x01
  if (m_status[4] == 0x01 && m_status[5] == 0x01)
    return WIICLASSIC;

  // Guitar Hero Controller: [0..3] = 0x00 0x00 0xA4 0x20, [4..5] = 0x01 0x03
  if (m_status[0] == 0x00 && m_status[1] == 0x00 && m_status[2] == 0xA4 &&
      m_status[3] == 0x20 && m_status[4] == 0x01 && m_status[5] == 0x03)
    return GuitarHeroController;

  // Guitar Hero World Tour Drums: [0..3] = 0x01 0x00 0xA4 0x20, [4..5] = 0x01 0x03
  if (m_status[0] == 0x01 && m_status[1] == 0x00 && m_status[2] == 0xA4 &&
      m_status[3] == 0x20 && m_status[4] == 0x01 && m_status[5] == 0x03)
    return GuitarHeroWorldTourDrums;

  // Turntable: [0..3] = 0x03 0x00 0xA4 0x20, [4..5] = 0x01 0x03
  if (m_status[0] == 0x03 && m_status[1] == 0x00 && m_status[2] == 0xA4 &&
      m_status[3] == 0x20 && m_status[4] == 0x01 && m_status[5] == 0x03)
    return Turntable;

  // Taiko no Tatsujin TaTaCon (Drum controller): [0..3] = 0x00 0x00 0xA4 0x20, [4..5] = 0x01 0x11
  if (m_status[0] == 0x00 && m_status[1] == 0x00 && m_status[2] == 0xA4 &&
      m_status[3] == 0x20 && m_status[4] == 0x01 && m_status[5] == 0x11)
    return DrumController;

  // Drawsome Tablet: [0..3] = 0xFF 0x00 0xA4 0x20, [4..5] = 0x00 0x13
  if (m_status[0] == 0xFF && m_status[1] == 0x00 && m_status[2] == 0xA4 &&
      m_status[3] == 0x20 && m_status[4] == 0x00 && m_status[5] == 0x13)
    return DrawsomeTablet;

  return UnknownChuck;
}

/**
 * @brief Read and decode joystick and button state from Nunchuck.
 *
 * Sends a 0x00 register-request byte to latch a fresh data snapshot, waits
 * 200µs for the controller to prepare data, then reads 6 bytes. Byte 0 =
 * joystick X (0x80 = center), byte 1 = joystick Y (0x80 = center), byte 5 =
 * button bits (bit 0 = Z active-low = close, bit 1 = C active-low = wide).
 * Joystick values outside the 10-count deadzone set m_hasStick = true.
 *
 * The 0x00 write is required by the Nunchuck protocol — omitting it causes
 * requestFrom() to fail or return stale data.
 */
void WiiChuckInput::readData()
{
  int joyX = 0, joyY = 0;

  // Latch a fresh data snapshot before reading.
  m_wire.beginTransmission(m_address);
  m_wire.write(0x00);
  m_wire.endTransmission();
  delayMicroseconds(200);

  m_wire.requestFrom((uint8_t)m_address, (uint8_t)6);
  if (m_wire.available() >= 6)
  {
    m_status[0] = m_wire.read();
    m_status[1] = m_wire.read();
    m_status[2] = m_wire.read();
    m_status[3] = m_wire.read();
    m_status[4] = m_wire.read();
    m_status[5] = m_wire.read();

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

  // Boop fires when the second button of a C+Z chord lands (either order).
  // Snapshot prev state before updating, so both flags reference the same frame.
  bool bothNow = m_cPressed && m_zPressed;
  bool bothPrev = m_lastCPressed && m_lastZPressed;
  if (bothNow && !bothPrev)
  {
    m_wantsBoop = true;
  }
  m_lastCPressed = m_cPressed;
  m_lastZPressed = m_zPressed;
}

/**
 * @brief Poll the Nunchuck at ~60 Hz and update internal state.
 *
 * Reads are throttled to prevent I2C bus flooding. The 16.7 ms interval
 * matches a ~60 Hz poll rate without requiring a dedicated timer.
 *
 * @return true if a fresh data packet was read this call; false if the
 *         16.7 ms throttle period has not yet elapsed.
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