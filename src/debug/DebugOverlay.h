/**
 * @file DebugOverlay.h
 * @brief Runtime debug overlay showing FPS and battery voltage.
 *
 * Renders a small semi-transparent HUD on the display showing:
 * - Measured frames per second
 * - Battery voltage percentage (read from an ADC pin)
 *
 * The overlay is rendered on top of the eye frame during development.
 * It can be toggled at runtime via setEnabled().
 */
#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <stdint.h>
#include "common/DisplayHAL.h"

/**
 * @brief Displays a debug HUD overlay on the eye display.
 *
 * Measures actual frame rate by counting frames over 1-second windows
 * and reads battery voltage from an ADC pin with configurable min/max
 * raw values. The overlay is drawn as a small text block on the display.
 */
class DebugOverlay
{
public:
  DebugOverlay();

  /**
   * @brief Initialize with the current display.
   * @param display Pointer to the display HAL.
   */
  void begin(DisplayHAL *display);

  /** @brief Enable or disable the overlay. */
  void setEnabled(bool enabled) { m_enabled = enabled; }

  /** @brief Returns true if the overlay is currently visible. */
  bool isEnabled() const { return m_enabled; }

  /**
   * @brief Configure the battery voltage ADC pin.
   * @param pin ADC pin number.
   * @param minRaw Minimum raw ADC value (discharged, e.g. 0).
   * @param maxRaw Maximum raw ADC value (fully charged, e.g. 4095).
   */
  void setBatteryPin(int pin, uint16_t minRaw, uint16_t maxRaw);

  /** @brief Update FPS counter and battery reading (call each frame). */
  void update();

  /** @brief Render the overlay onto the display. */
  void render();

  /** @brief Last measured FPS. */
  float getFps() const { return m_fps; }

private:
  void updateFps();
  void updateBattery();

  DisplayHAL *m_display = nullptr;
  bool m_enabled = false;
  bool m_initialized = false;

  int m_batteryPin = -1;
  uint16_t m_batteryMinRaw = 0;
  uint16_t m_batteryMaxRaw = 4095;
  int m_batteryPercent = 0;

  uint32_t m_lastFrameTime = 0;
  uint32_t m_frameCount = 0;
  uint32_t m_fpsUpdateTime = 0;
  float m_fps = 0.0f;
};

#endif // DEBUG_OVERLAY_H