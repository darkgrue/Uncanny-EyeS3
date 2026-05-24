/**
 * @file EyeAnimator.cpp
 * @brief Central animation controller implementation.
 *
 * Coordinates EyeRenderer, EyeMovement, BlinkFSM, and input sources each
 * frame. Handles light sensor reads, autonomous iris animation when no
 * sensor is present, network state broadcast, and expression command routing.
 * Runs on a dedicated FreeRTOS task at ~120 FPS.
 */
#include "EyeAnimator.h"
#include "EyeLibrary.h"
#include <Arduino.h>
#include <WiFi.h>

EyeAnimator::EyeAnimator()
    : m_display(nullptr), m_input(nullptr), m_sync(nullptr), m_eyeDef(nullptr),
      m_lightSensorPin(-1), m_lastLightRead(0), m_booped(false),
      m_needsRender(true), m_initialized(false)
{
}

/**
 * @brief Initialize with display and eye definition.
 *
 * Sets up the EyeRenderer with double-buffered PSRAM frames, configures
 * the EyeMovement system with default saccade parameters, and initializes
 * the autonomous iris animation arrays.
 */
bool EyeAnimator::begin(DisplayHAL *display, const EyeDefinition &eyeDef)
{
  m_display = display;
  m_eyeDef = &eyeDef;

  m_normalClosure = eyeDef.eyelid.normalClosure;
  m_wideClosure = eyeDef.eyelid.wideClosure;
  

  m_blink.setNormalGap(m_normalClosure);
  m_blink.normal();

  if (!m_renderer.begin(display, eyeDef))
  {
    return false;
  }

  m_movement.setBounds(0.6f);
  m_movement.setRandomDuration(250, 500);
  m_movement.setSaccadeDelay(4000);
  m_movement.setRandomMode(true);

  for (int i = 0; i < IRIS_LEVELS; i++)
  {
    m_irisPrev[i] = 0;
    m_irisNext[i] = -0.5f + random(0, 1000) / 1000.0f;
  }
  m_irisFrame = 0;
  m_currentIris = 0.5f;

  m_initialized = true;
  return true;
}

/**
 * @brief Configure the light sensor ADC pin and calibration values.
 *
 * When pin >= 0, enables light-driven pupil control. When pin < 0,
 * disables the sensor and enables autonomous iris animation instead.
 */
void EyeAnimator::setLightSensor(int pin, uint16_t minVal, uint16_t maxVal, float curve)
{
  m_lightSensorPin = pin;
  m_lightMin = minVal;
  m_lightMax = maxVal;
  m_lightCurve = curve;
  if (m_lightSensorPin >= 0)
  {
    pinMode(pin, INPUT);
  }
}

/**
 * @brief Set the minimum and maximum pupil size.
 *
 * @param minPupil Fully dilated fraction (e.g. 0.45).
 * @param maxPupil Fully constricted fraction (e.g. 0.8).
 */
void EyeAnimator::setPupilRange(float minPupil, float maxPupil)
{
  m_irisMin = minPupil;
  m_irisRange = maxPupil - minPupil;
}

/**
 * @brief Switch to a different eye from the registry by index.
 *
 * Reinitializes the renderer with the new eye definition and marks
 * the frame as dirty for a full refresh.
 */
bool EyeAnimator::setEyeIndex(int index)
{
  if (index < 0 || index >= s_eyeCount)
    return false;
  // Store the request; the actual switch is applied at the top of update()
  // on Core 1 to avoid a race with renderFrame() running on the same core.
  m_pendingEyeIndex.store(index, std::memory_order_relaxed);
  return true;
}

/**
 * @brief Main animation update â€” call frequently from the render task.
 *
 * Resolves eye position from input sources in priority order: primary input
 * (WiiChuck, exclusive control) â†’ face detection (m_faceInput, face visible)
 * â†’ autonomous random saccades. Applies network input, advances the movement
 * system and blink FSM, and updates either the light sensor value or the
 * autonomous iris. Marks needsRender = true after processing.
 *
 * @param now Current time in milliseconds.
 */
void EyeAnimator::update(uint32_t now)
{
  if (!m_initialized)
    return;

  // Apply any eye switch requested by Core 0 (serial command handler).
  // Runs here, on Core 1, so it cannot race with renderFrame().
  int pending = m_pendingEyeIndex.exchange(-1, std::memory_order_relaxed);
  if (pending >= 0 && pending < s_eyeCount)
  {
    m_eyeIndex = pending;
    m_eyeDef = s_eyeRegistry[pending];
    m_renderer.begin(m_display, *m_eyeDef);
    m_needsRender = true;
  }

  if (m_input)
    m_input->update();
  if (m_faceInput)
    m_faceInput->update();

  if (m_input && m_input->hasExclusiveControl())
  {
    float tx = m_input->getTargetX();
    float ty = m_input->getTargetY();
    m_faceWasTracking = false;
    m_movement.setRandomMode(false);

    // Sync smooth position to current eye position on the first frame of control
    // so the eye doesn't jump from wherever it was during autonomous movement.
    if (!m_hadJoystickControl)
    {
      m_joystickSmX = m_movement.getX();
      m_joystickSmY = m_movement.getY();
      m_hadJoystickControl = true;
    }

    // Adaptive exponential smoothing: blend factor scales with distance so large
    // joystick deflections get a fast response while small ones stay smooth.
    float dx    = tx - m_joystickSmX;
    float dy    = ty - m_joystickSmY;
    float dist  = sqrtf(dx * dx + dy * dy);
    float alpha = constrain(JOYSTICK_BASE_ALPHA + dist * JOYSTICK_DIST_ALPHA, 0.0f, 1.0f);
    m_joystickSmX += alpha * dx;
    m_joystickSmY += alpha * dy;

    m_movement.setCurrentPosition(m_joystickSmX, m_joystickSmY);
  }
  else if (m_faceInput && m_faceInput->hasExclusiveControl())
  {
    m_hadJoystickControl = false;
    m_faceWasTracking = true;
    m_movement.setTarget(m_faceInput->getTargetX(), m_faceInput->getTargetY());
    m_movement.setRandomMode(false);
  }
  else
  {
    bool wasJoystick = m_hadJoystickControl;
    m_hadJoystickControl = false;
    if (m_faceWasTracking)
    {
      m_movement.setTargetLost();
      m_movement.setRandomMode(true);
      m_faceWasTracking = false;
    }
    else if (wasJoystick)
    {
      // Joystick just returned to deadzone — resume autonomous saccades.
      m_movement.setTargetLost();
      m_movement.setRandomMode(true);
    }
  }

  // Determine the expression command to broadcast this frame before any flags
  // are cleared, so broadcastState() can read m_broadcastCommand after update().
  m_broadcastCommand = CMD_NONE;

  if (m_input)
  {
    if (m_input->wantsBoop())
    {
      eyesBoop();
      m_input->clearBoopFlag();
      m_broadcastCommand = CMD_BOOP;
    }
    else if (m_booped)
    {
      // Boop is playing out — keep CMD_NONE so the follower's own boop timer
      // runs undisturbed instead of being cancelled by a premature CMD_NORMAL.
    }
    else if (m_input->wantsWide())
    {
      eyesWide();
      m_broadcastCommand = CMD_WIDE;
    }
    else if (m_input->wantsClose())
    {
      eyesClose();
      m_broadcastCommand = CMD_CLOSE;
    }
    else
    {
      eyesNormal();
      m_broadcastCommand = CMD_NORMAL; // Tells follower to exit any forced state.
    }

    if (m_input->wantsBlink())
    {
      eyesBlink();
      m_input->clearBlinkFlag();
      m_broadcastCommand = CMD_BLINK; // Edge-triggered; captured before flag is cleared.
    }
  }

  processNetworkInput();

  m_movement.update();

  m_blink.update();

  if (m_lightSensorPin >= 0)
  {
    updateLightSensor(now);
  }
  else if (m_remotePupilFactor < 0.0f)
  {
    // Only run autonomous iris when not mirroring a controller. Suppressing it
    // here prevents the accumulating autonomous value from bleeding through on
    // any frame where network data arrives slightly late.
    updateIrisAutonomous(now);
  }

  if (m_remotePupilFactor >= 0.0f)
    m_currentIris = m_remotePupilFactor;

  if (m_wideActive)
    m_currentIris = m_irisMin; // fully dilated pupils while wide

  if (m_booped)
  {
    if (millis() - m_boopStart < BOOP_DURATION_MS)
    {
      m_blink.wideTo(BOOP_SQUINT_FACTOR); // heavy squint — overrides whatever normal() set
      m_currentIris = m_irisMin;          // fully dilated pupils
    }
    else
    {
      m_booped = false;
      eyesNormal();
    }
  }

  m_needsRender = true;
}

/**
 * @brief Broadcast current eye state to all ESP-NOW peers.
 *
 * Builds an EyeSyncMessage from current position, pupil, blink state,
 * and any active expression command, then broadcasts to all registered
 * peers via EyeSyncManager. Returns false if no network or no peers.
 */
bool EyeAnimator::broadcastState()
{
  if (!m_sync)
    return false;

  bool shouldBroadcast = isController();
  if (m_sync->getPeerCount() > 0)
  {
    shouldBroadcast = true;
  }
  if (!shouldBroadcast)
    return false;

  EyeSyncMessage msg;
  memset(&msg, 0, sizeof(msg));

  uint8_t mac[6];
  WiFi.macAddress(mac);
  memcpy(msg.macAddress, mac, 6);

  msg.eyeX = getEyeX();
  msg.eyeY = getEyeY();
  msg.pupilFactor = m_currentIris;
  msg.blinkState  = (uint8_t)m_blink.getState();
  msg.blinkFactor = m_blink.getFactor();
  msg.timestamp   = millis();

  msg.command = m_broadcastCommand;

  m_sync->broadcast(msg);
  return true;
}

/**
 * @brief Poll the light sensor ADC and update the pupil factor.
 *
 * Throttled to 10 Hz maximum. Normalizes the raw ADC value to 0.0-1.0
 * using the configured min/max, applies the power curve, and maps to the
 * iris range to produce the final pupil factor.
 */
void EyeAnimator::updateLightSensor(uint32_t now)
{
  constexpr uint32_t LIGHT_INTERVAL = 100; // 10 Hz max polling (100ms)

  if (now - m_lastLightRead < LIGHT_INTERVAL)
    return;

  uint16_t raw = analogRead(m_lightSensorPin);
  if (raw > 1023)
    raw = 1023;

  raw = constrain(raw, m_lightMin, m_lightMax);
  float normalized = (float)(raw - m_lightMin) / (float)(m_lightMax - m_lightMin);
  normalized = pow(normalized, m_lightCurve);

  m_currentIris = m_irisMin + normalized * m_irisRange;
  m_lastLightRead = now;
}

/**
 * @brief Autonomous iris animation mimicking human pupillary unrest.
 *
 * Generates new target pupil sizes at 2-5 second intervals using a
 * lognormal distribution, then smoothly transitions over 600-1000ms
 * using smoothstep easing. Keeps the iris in a valid 0.3-0.7 range
 * centered around 0.5 when no light sensor is present.
 */
void EyeAnimator::updateIrisAutonomous(uint32_t now)
{
  uint32_t dt = now - m_lastIrisChange;

  if (dt >= m_irisHoldDuration)
  {
    // Save current smooth value as the starting point for the new transition.
    m_irisPrev[0] = m_irisSmooth;

    float u1 = (float)random(0, 1000) / 1000.0f;
    float u2 = (float)random(0, 1000) / 1000.0f;

    float normalSample = sqrt(-2.0f * log(u1 + 0.0001f)) * cos(2.0f * PI * u2);
    float lognormalSample = normalSample * 0.3f - 0.1f;

    m_irisTarget = constrain(lognormalSample, -0.3f, 0.3f);

    m_irisHoldDuration = 2000 + random(0, 3000);
    m_irisTransitionDuration = 600 + random(0, 400);

    m_lastIrisChange = now;
    dt = 0; // Reset so the transition calculation below starts from 0.
  }

  float t = (float)dt / (float)m_irisTransitionDuration;
  t = constrain(t, 0.0f, 1.0f);

  float eased = t * t * (3.0f - 2.0f * t);

  m_irisSmooth = m_irisPrev[0] + (m_irisTarget - m_irisPrev[0]) * eased;

  float sum = 0.5f + m_irisSmooth;
  sum = constrain(sum, 0.3f, 0.7f);

  m_currentIris = m_irisMin + (sum * m_irisRange);
}

/**
 * @brief Apply remote state received over ESP-NOW from the controller.
 *
 * Only applies network values if a controller is registered and the
 * data is fresh (within 100ms). Routes expression commands from the
 * remote state to the corresponding eye expression methods.
 */
/**
 * @brief Apply remote state received over ESP-NOW from the controller.
 *
 * Pupil stability is treated differently from blink/movement: on brief packet
 * gaps the last remote pupil value is held rather than handing off to the
 * autonomous iris, which would produce a visible twitch. The autonomous iris
 * only resumes when the controller peer is fully dropped by pruneDropped()
 * (default 5 s timeout). At that point m_irisSmooth is seeded from the last
 * remote value so the hand-off is seamless.
 */
void EyeAnimator::processNetworkInput()
{
  if (!m_sync)
    return;
  if (m_input)
    return; // controller device: local input always takes precedence

  if (!m_sync->hasController())
  {
    // Controller peer dropped by pruneDropped(). Seed the autonomous iris
    // smoother from the last remote value so there is no visible jump.
    if (m_remotePupilFactor >= 0.0f)
    {
      m_irisSmooth = constrain((m_remotePupilFactor - m_irisMin) / m_irisRange - 0.5f, -0.3f, 0.3f);
      m_irisPrev[0] = m_irisSmooth;
      m_lastIrisChange = millis();
    }
    m_remoteBlinkFactor = -1.0f;
    m_remotePupilFactor = -1.0f;
    return;
  }

  if (m_sync->getLastRemoteTime() == 0)
    return; // no data received yet

  uint32_t now = millis();

  if ((now - m_sync->getLastRemoteTime()) < 250)
  {
    EyeSyncMessage msg = m_sync->getLastRemoteState();

    m_movement.setCurrentPosition(msg.eyeX, msg.eyeY);
    m_movement.setRandomMode(false);

    // Mirror the controller's eyelid and pupil factors directly.
    // The follower's own BlinkFSM keeps running for state tracking (isForced)
    // but its getFactor() output is bypassed by getBlinkFactor() below.
    m_remoteBlinkFactor = msg.blinkFactor;
    m_remotePupilFactor = msg.pupilFactor;

    switch (msg.command)
    {
    case CMD_BLINK:
      eyesBlink();
      break;
    case CMD_BOOP:
      eyesBoop();
      break;
    case CMD_CLOSE:
      eyesClose();
      break;
    case CMD_WIDE:
      eyesWide();
      break;
    case CMD_NORMAL:
      // Only exit a forced expression (close/wide); don't interrupt an
      // autonomous blink or an active boop that is still timing out.
      if (m_blink.isForced() && !m_booped)
        eyesNormal();
      break;
    default:
      break;
    }
  }
  else
  {
    // Controller registered but data is stale. Resume autonomous blink and
    // movement, but hold m_remotePupilFactor at the last known value rather
    // than handing off to the autonomous iris — that hand-off would produce a
    // visible twitch on brief WiFi gaps. The pupil reverts to autonomous only
    // when hasController() becomes false (pruneDropped timeout).
    m_remoteBlinkFactor = -1.0f;
    m_movement.setTargetLost();
    m_movement.setRandomMode(true);
  }
}