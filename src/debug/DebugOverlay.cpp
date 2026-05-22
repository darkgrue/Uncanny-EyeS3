/**
 * @file DebugOverlay.cpp
 * @brief Implementation of the runtime debug HUD overlay.
 *
 * Measures actual FPS by counting frames over 1-second windows and reads
 * battery voltage from an ADC pin with configurable min/max raw values.
 * The overlay is rendered as a small text block (FPS + battery %) on the
 * display and can be toggled at runtime.
 */
#include "DebugOverlay.h"
#include <Arduino.h>

DebugOverlay::DebugOverlay()
    : m_display(nullptr), m_enabled(false), m_initialized(false), m_batteryPin(-1), m_batteryMinRaw(0), m_batteryMaxRaw(4095), m_batteryPercent(0), m_lastFrameTime(0), m_frameCount(0), m_fpsUpdateTime(0), m_fps(0.0f)
{
}

/**
 * @brief Initialize the overlay with the current display.
 * @param display Pointer to the DisplayHAL.
 */
void DebugOverlay::begin(DisplayHAL *display)
{
  m_display = display;
  m_lastFrameTime = micros();
  m_fpsUpdateTime = millis();
  m_initialized = true;
}

/**
 * @brief Configure the battery voltage ADC pin and its raw value range.
 * @param pin ADC pin number, or -1 to disable battery reading.
 * @param minRaw Raw ADC value at fully discharged.
 * @param maxRaw Raw ADC value at fully charged.
 */
void DebugOverlay::setBatteryPin(int pin, uint16_t minRaw, uint16_t maxRaw)
{
  m_batteryPin = pin;
  m_batteryMinRaw = minRaw;
  m_batteryMaxRaw = maxRaw;
  if (pin >= 0)
  {
    pinMode(pin, INPUT);
  }
}

/**
 * @brief Update FPS counter and battery reading.
 *
 * Called each frame when enabled. Checks if a second has elapsed
 * to compute the FPS, and reads the battery ADC if configured.
 */
void DebugOverlay::update()
{
  if (!m_initialized || !m_enabled)
    return;

  updateFps();
  updateBattery();
}

/** @brief Count frames and compute FPS every second using micros() timestamps. */
void DebugOverlay::updateFps()
{
  uint32_t now = micros();
  m_frameCount++;

  if (now - m_lastFrameTime >= 1000000)
  {
    m_fps = (float)m_frameCount * 1000000.0f / (float)(now - m_lastFrameTime);
    m_frameCount = 0;
    m_lastFrameTime = now;
  }
}

/**
 * @brief Read battery voltage from ADC and compute percentage.
 *
 * Constrains the raw reading to the configured min/max range,
 * normalizes to 0-100%, and rounds to the nearest integer.
 */
void DebugOverlay::updateBattery()
{
  if (m_batteryPin < 0)
    return;

  uint32_t raw = analogRead(m_batteryPin);
  raw = constrain(raw, m_batteryMinRaw, m_batteryMaxRaw);
  float normalized = (float)(raw - m_batteryMinRaw) / (float)(m_batteryMaxRaw - m_batteryMinRaw);
  m_batteryPercent = (int)(normalized * 100.0f + 0.5f);
  m_batteryPercent = constrain(m_batteryPercent, 0, 100);
}

/**
 * @brief Render the FPS and battery percentage strings onto the display.
 *
 * Draws two lines: "FPS:XXX" at (4,4) and "BAT:XXX%" at (4,16) in green.
 * Only renders if the overlay is enabled and initialized.
 */
void DebugOverlay::render()
{
  if (!m_initialized || !m_enabled || !m_display)
    return;

  char buf[32];
  uint16_t color = 0x07E0; // Green RGB565

  snprintf(buf, sizeof(buf), "FPS:%3d", (int)(m_fps + 0.5f));
  m_display->drawString(4, 4, buf, color);

  if (m_batteryPin >= 0)
  {
    snprintf(buf, sizeof(buf), "BAT:%3d%%", m_batteryPercent);
    m_display->drawString(4, 16, buf, color);
  }
}