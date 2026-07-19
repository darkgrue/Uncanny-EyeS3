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
#include "input/LuxSensor.h"
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
   * @brief Force this device to broadcast as controller even without a local
   * input device attached (e.g. two purely-autonomous devices syncing).
   */
  void setForceController(bool force) { m_forceController = force; }

  /**
   * @brief Configure the light sensor pin for pupil control.
   * @param pin ADC pin number, or -1 to disable and use autonomous iris.
   * @param minVal Raw ADC value representing minimum (brightest) light.
   * @param maxVal Raw ADC value representing maximum (darkest) light.
   * @param curve Power curve exponent (1.0 = linear).
   */
  void setLightSensor(int pin, uint16_t minVal, uint16_t maxVal, float curve = 1.0f);

  /**
   * @brief Configure the MAX44009 lux sensor for pupil control.
   * @param sensor Pointer to the LuxSensor instance, or nullptr to disable.
   */
  void setLuxSensor(LuxSensor *sensor);

  /**
   * @brief Set the pupil size range (fraction of iris radius).
   * @param minPupil Most constricted (smallest) pupil fraction, e.g. 0.35.
   * @param maxPupil Most dilated (largest) pupil fraction, e.g. 1.67.
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

  /** @brief Returns true when a local input device is attached or forceController is set. */
  bool isController() const { return m_input != nullptr || m_forceController; }

  /** @brief Current eye X position (-1.0 to +1.0). */
  float getEyeX() const { return m_movement.getX(); }

  /** @brief Current eye Y position (-1.0 to +1.0). */
  float getEyeY() const { return m_movement.getY(); }

  /** @brief Current pupil constriction factor (0.45-0.8 typical range). */
  float getPupilFactor() const { return m_currentIris; }

  /**
   * @brief Current eyelid closure fraction (0.0 = open, 1.0 = closed).
   *
   * Returns the controller's broadcasted factor when in follower mode so that
   * autonomous blinks and all expression animations are mirrored exactly.
   * Falls back to the local BlinkFSM when solo or when controller data is stale.
   */
  float getBlinkFactor() const
  {
    return (m_remoteBlinkFactor >= 0.0f) ? m_remoteBlinkFactor : m_blink.getFactor();
  }

  /** @brief Returns true when a new frame should be rendered. */
  bool needsRender() const { return m_needsRender; }

  /** @brief Access the eye renderer for custom draw calls. */
  EyeRenderer *getRenderer() { return &m_renderer; }

  // --- Expression commands ---

  /** @brief Trigger a single blink. */
  void eyesBlink() { m_blink.trigger(); }

  /**
   * @brief Trigger a boop expression.
   *
   * Squints eyelids to BOOP_SQUINT_FACTOR and dilates pupils fully for
   * BOOP_DURATION_MS, then automatically returns to normal. Distinct from
   * eyesWide() (fully open lids, dilated pupils) by the squinted eyelids.
   */
  void eyesBoop()
  {
    if (!m_booped)        // don't restart if already mid-boop
    {
      m_boopStart = millis();
      m_pupilBooping = true;
      m_pupilBoopFrom = m_currentIris;
      m_pupilBoopStart = millis();
    }
    m_booped = true;
  }

  /** @brief Hold eyelids closed. */
  void eyesClose() { m_blink.close(); }

  /** @brief Return to normal eyelid state. Movement mode is managed by the update() control block. */
  void eyesNormal()
  {
    m_blink.setNormalGap(m_normalClosure);
    m_blink.normal();
    if (m_wideActive)
      m_wideJustDeactivated = true;
    m_wideActive = false;
  }

  /** @brief Open eyelids wide and constrict pupils (surprise expression). */
  void eyesWide()
  {
    m_blink.wideTo(m_wideClosure);
    if (!m_wideActive)
    {
      m_wideJustActivated = true;
      m_pupilReleasing    = false; // cancel any in-progress release
    }
    m_wideActive = true;
  }

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
  bool m_forceController = false;  // Broadcast as controller even without m_input attached

  const EyeDefinition *m_eyeDef = nullptr; // Current eye definition
  int m_eyeIndex = 0;                      // Current eye registry index

  EyeRenderer m_renderer; // Frame renderer with double-buffering
  EyeMovement m_movement; // Saccadic movement generator
  BlinkFSM m_blink;       // Eyelid animation state machine

  // Light sensor / iris state
  int m_lightSensorPin = -1;    // ADC pin for light sensor, -1 if disabled
  uint16_t m_lightMin = 0;      // Calibrated bright ADC value
  uint16_t m_lightMax = 1023;   // Calibrated dark ADC value
  float m_lightCurve = 1.0f;   // Power curve exponent
  LuxSensor *m_luxSensor = nullptr; // I2C lux sensor (nullptr if not used)
  float m_irisMin = 0.35f;      // Minimum (most constricted) pupil fraction from eye definition
  float m_irisRange = 1.32f;    // Iris range (maxFraction - minFraction)
  float m_irisCenter = 0.5f;    // Normalized center [0,1] for hippus oscillation; 0.5=midpoint, sensor-derived when sensor present
  float m_irisCenterPrev = 0.5f; // Previous smoothed iris center for EMA
  uint32_t m_lastLightRead = 0; // Timestamp of last sensor read
  float m_lightSmoothAlpha = 0.2f; // EMA smoothing for light sensor (lower = smoother)
  float m_currentIris = 0.5f;   // Current pupil factor

  // Autonomous iris animation (used when no light sensor)
  static constexpr int IRIS_LEVELS = 7;
  float m_irisPrev[IRIS_LEVELS] = {0};
  float m_irisNext[IRIS_LEVELS] = {0};
  uint16_t m_irisFrame = 0;

  float m_irisTarget = 0.0f;               // Target iris offset (-0.5 to +0.5)
  float m_irisSmooth = 0.0f;               // Smoothed iris value after easing
  uint32_t m_lastIrisChange = 0;           // Timestamp of last target change
  uint32_t m_irisHoldDuration       = IRIS_HOLD_MIN;       // Hold time between drift targets
  uint32_t m_irisTransitionDuration = IRIS_TRANSITION_MIN; // Transition duration

  // Pending eye switch requested from Core 0; applied at the start of update() on Core 1.
  // Value of -1 means no switch is pending.
  std::atomic<int> m_pendingEyeIndex{-1};

  // Joystick smooth-follow state
  float m_joystickSmX = 0.0f;       // Exponentially-smoothed joystick X target
  float m_joystickSmY = 0.0f;       // Exponentially-smoothed joystick Y target
  bool  m_hadJoystickControl = false; // True on the previous frame if joystick was driving

  // Face tracking smooth-follow state
  float m_faceSmX = 0.0f;           // Exponentially-smoothed face X target
  float m_faceSmY = 0.0f;           // Exponentially-smoothed face Y target
  bool     m_faceWasTracking = false;     // True when face input had control last frame
  bool     m_wideActive = false;          // True while eyesWide() is held
  bool     m_wideJustActivated = false;   // True on the first frame of a new wide activation
  bool     m_wideJustDeactivated = false; // True on the first frame after wide is released
  float    m_pupilAnimFrom = 0.0f;        // Pupil value when wide press animation started
  uint32_t m_pupilAnimStart = 0;          // millis() when wide press animation started
  bool     m_pupilReleasing = false;      // True while animating back from wide
  float    m_pupilReleaseFrom = 0.0f;     // Pupil value when release animation started
  uint32_t m_pupilReleaseStart = 0;       // millis() when release animation started
  bool     m_booped = false;              // True when boop expression is active
  uint32_t m_boopStart = 0;           // millis() when current boop began
bool     m_pupilBooping = false;       // True while animating pupil during boop entry/exit
  float    m_pupilBoopFrom = 0.0f;       // Pupil value when boop pupil animation started
  uint32_t m_pupilBoopStart = 0;         // millis() when boop pupil animation started
  float    m_pupilPreBoop = 0.5f;        // Pupil value to restore when boop exits
  bool     m_needsRender = true;      // Flag to request a new frame render
  bool     m_initialized = false;     // True after successful begin()

  EyeCommand m_broadcastCommand  = CMD_NONE; // Expression command set in update(), read by broadcastState()
  float      m_remoteBlinkFactor = -1.0f;    // Controller's blinkFactor; -1 = not following (use local FSM)
  float      m_remotePupilFactor = -1.0f;    // Controller's pupilFactor; -1 = not following (use local iris)
  bool       m_networkWasStale   = false;    // True once stale-data transition has been applied

  float m_normalClosure = EYELID_NORMAL_CLOSURE_DEFAULT; // Eyelid coverage at rest
  float m_wideClosure   = EYELID_WIDE_CLOSURE_DEFAULT;   // Eyelid coverage when wide/surprised
};

#endif // EYE_ANIMATOR_H