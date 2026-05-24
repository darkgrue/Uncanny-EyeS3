/**
 * @file EyeAnimator.h
 * @brief Central animation controller for the eye display.
 *
 * Coordinates input sources, movement generation, blinking, network sync, and
 * iris/pupil control. Maintains the render loop state and delegates to sub-systems
 * (EyeRenderer, EyeMovement, BlinkFSM).
 */
#ifndef EYE_ANIMATOR_H
#define EYE_ANIMATOR_H

#include "common/EyeState.h"
#include "EyeConfig.h"
#include "EyeRenderer.h"
#include "animation/BlinkFSM.h"
#include "animation/EyeMovement.h"
#include "common/DisplayHAL.h"
#include "input/InputBase.h"
#include "network/EyeSync.h"
#include "eyes.h"
#include <atomic>

/**
 * @brief Central animation state machine for eye rendering.
 *
 * Owns the display renderer, movement generator, blink FSM, and input handling.
 * Call update() at high frequency (~120 FPS), call needsRender() to check if
 * a new frame should be drawn, then call renderFrame() on the renderer.
 */
class EyeAnimator
{
public:
  EyeAnimator();

  /**
   * @brief Initialize with display and eye definition.
   * @param display Pointer to the display HAL.
   * @param eyeDef Eye definition containing precomputed polar maps.
   * @return true if initialization succeeded.
   */
  bool begin(DisplayHAL *display, const EyeDefinition &eyeDef);

  /** @brief Attach an input source (e.g., WiiChuckInput). nullptr to clear. */
  void setInput(InputBase *input) { m_input = input; }

  /**
   * @brief Attach a secondary face-tracking input (e.g., GestureFaceInput).
   *
   * When set, the face sensor drives eye position if the primary input
   * (WiiChuck) does not have exclusive control. nullptr to disable.
   */
  void setFaceInput(InputBase *face) { m_faceInput = face; }

  /** @brief Attach a network sync manager. nullptr if solo operation. */
  void setSyncManager(EyeSyncManager *sync) { m_sync = sync; }

  /**
   * @brief Configure the light sensor pin for pupil control.
   * @param pin ADC pin number, or -1 to disable and use autonomous iris.
   * @param minVal Raw ADC value representing minimum (brightest) light.
   * @param maxVal Raw ADC value representing maximum (darkest) light.
   * @param curve Power curve exponent (1.0 = linear).
   */
  void setLightSensor(int pin, uint16_t minVal, uint16_t maxVal, float curve = 1.0f);

  /**
   * @brief Set the pupil size range for light-driven dilation.
   * @param minPupil Smallest pupil fraction (fully dilated, e.g. 0.45).
   * @param maxPupil Largest pupil fraction (fully constricted, e.g. 0.8).
   */
  void setPupilRange(float minPupil, float maxPupil);

  /**
   * @brief Main update loop. Call frequently from the render task.
   * @param now Current time in milliseconds.
   */
  void update(uint32_t now);

  /**
   * @brief Broadcast current eye state to ESP-NOW peers.
   * @return true if broadcast was sent, false if no network or no peers.
   */
  bool broadcastState();

  /** @brief Returns true when an input device has exclusive control. */
  bool isController() const { return m_input && m_input->hasExclusiveControl(); }

  /** @brief Current eye X position (-1.0 to +1.0). */
  float getEyeX() const { return m_movement.getX(); }

  /** @brief Current eye Y position (-1.0 to +1.0). */
  float getEyeY() const { return m_movement.getY(); }

  /** @brief Current pupil constriction factor (0.45-0.8 typical range). */
  float getPupilFactor() const { return m_currentIris; }

  /** @brief Current eyelid closure fraction (0.0 = open, 1.0 = closed). */
  float getBlinkFactor() const { return m_blink.getFactor(); }

  /** @brief Returns true when a new frame should be rendered. */
  bool needsRender() const { return m_needsRender; }

  /** @brief Access the eye renderer for custom draw calls. */
  EyeRenderer *getRenderer() { return &m_renderer; }

  // --- Expression commands ---

  /** @brief Trigger a single blink. */
  void eyesBlink() { m_blink.trigger(); }

  /** @brief Trigger a boop expression (tongue out). */
  void eyesBoop() { m_booped = true; }

  /** @brief Hold eyelids closed. */
  void eyesClose() { m_blink.close(); }

  /** @brief Return to normal eyelid state and resume random movement. */
  void eyesNormal()
  {
    m_blink.setNormalGap(m_normalClosure);
    m_blink.normal();
    m_movement.setRandomMode(true);
  }

  /** @brief Open eyelids wide (surprise expression). */
  void eyesWide() { m_blink.wideTo(m_wideClosure); }

  /** @brief Get the currently active eye index. */
  int getEyeIndex() const { return m_eyeIndex; }

  /** @brief Switch to a different eye definition by index. */
  bool setEyeIndex(int index);

private:
  /** @brief Poll the light sensor ADC and update m_currentIris. */
  void updateLightSensor(uint32_t now);

  /** @brief Advance autonomous iris animation with saccade-like timing. */
  void updateIrisAutonomous(uint32_t now);

  /** @brief Apply remote state from ESP-NOW peer if controller data is fresh. */
  void processNetworkInput();

  DisplayHAL *m_display = nullptr;  // Display abstraction layer
  InputBase *m_input = nullptr;     // Primary input source (e.g. WiiChuck)
  InputBase *m_faceInput = nullptr; // Secondary input source (e.g. GestureFace)
  EyeSyncManager *m_sync = nullptr; // ESP-NOW sync manager

  const EyeDefinition *m_eyeDef = nullptr; // Current eye definition
  int m_eyeIndex = 0;                      // Current eye registry index

  EyeRenderer m_renderer; // Frame renderer with double-buffering
  EyeMovement m_movement; // Saccadic movement generator
  BlinkFSM m_blink;       // Eyelid animation state machine

  // Light sensor / iris state
  int m_lightSensorPin = -1;    // ADC pin for light sensor, -1 if disabled
  uint16_t m_lightMin = 0;      // Calibrated bright ADC value
  uint16_t m_lightMax = 1023;   // Calibrated dark ADC value
  float m_lightCurve = 1.0f;    // Power curve exponent
  float m_irisMin = 0.45f;      // Minimum pupil fraction (dilated)
  float m_irisRange = 0.35f;    // Iris range (max - min)
  uint32_t m_lastLightRead = 0; // Timestamp of last sensor read
  float m_currentIris = 0.5f;   // Current pupil factor

  // Autonomous iris animation (used when no light sensor)
  static constexpr int IRIS_LEVELS = 7;
  float m_irisPrev[IRIS_LEVELS] = {0};
  float m_irisNext[IRIS_LEVELS] = {0};
  uint16_t m_irisFrame = 0;

  float m_irisTarget = 0.0f;               // Target iris offset (-0.5 to +0.5)
  float m_irisSmooth = 0.0f;               // Smoothed iris value after easing
  uint32_t m_lastIrisChange = 0;           // Timestamp of last target change
  uint32_t m_irisHoldDuration = 3000;      // Hold time between changes (ms)
  uint32_t m_irisTransitionDuration = 800; // Transition duration (ms)

  // Pending eye switch requested from Core 0; applied at the start of update() on Core 1.
  // Value of -1 means no switch is pending.
  std::atomic<int> m_pendingEyeIndex{-1};

  bool m_faceWasTracking = false; // True when face input had control last frame
  bool m_booped = false;          // True when boop expression is active
  bool m_needsRender = true;      // Flag to request a new frame render
  bool m_initialized = false;     // True after successful begin()

  float m_normalClosure = 0.15f; // Eyelid coverage at rest (0.0=fully open, 1.0=fully closed)
  float m_wideClosure = 0.0f;    // Eyelid coverage when wide/surprised (0.0=fully retracted, 1.0=fully closed)
};

#endif // EYE_ANIMATOR_H