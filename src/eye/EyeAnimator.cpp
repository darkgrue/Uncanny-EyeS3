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
    m_movement.setTarget(tx, ty);
    m_movement.setRandomMode(false);
    if (!m_movement.isMoving())
    {
      m_movement.moveTo(tx, ty, EYE_MOVE_DURATION_MIN);
    }
  }
  else if (m_faceInput && m_faceInput->hasExclusiveControl())
  {
    m_faceWasTracking = true;
    m_movement.setTargetAcquired();
    m_movement.setTarget(m_faceInput->getTargetX(), m_faceInput->getTargetY());
    m_movement.setRandomMode(false);
  }
  else
  {
    if (m_faceWasTracking)
    {
      m_movement.setTargetLost();
      m_movement.setRandomMode(true);
      m_faceWasTracking = false;
    }
    else if (m_input && !m_movement.isMoving() &&
             m_movement.getTargetX() == 0 && m_movement.getTargetY() == 0)
    {
      m_movement.setTargetLost();
      m_movement.setRandomMode(true);
    }
  }

  if (m_input)
  {
    if (m_input->wantsBoop())
    {
      eyesBoop();
      m_input->clearBoopFlag();
    }
    else if (m_input->wantsWide())
    {
      eyesWide();
    }
    else if (m_input->wantsClose())
    {
      eyesClose();
    }
    else
    {
      eyesNormal();
    }

    if (m_input->wantsBlink())
    {
      eyesBlink();
      m_input->clearBlinkFlag();
    }
  }

  processNetworkInput();

  uint32_t dt = millis() - m_lastLightRead;
  m_movement.update(dt);

  m_blink.update(micros());

  if (m_lightSensorPin >= 0)
  {
    updateLightSensor(now);
  }
  else
  {
    updateIrisAutonomous(now);
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
  if (m_sync && m_sync->getPeerCount() > 0)
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
  msg.blinkState = (uint8_t)m_blink.getState();
  msg.timestamp = millis();

  msg.command = CMD_NONE;
  if (m_input && m_input->wantsClose())
  {
    msg.command = CMD_CLOSE;
  }
  else if (m_input && m_input->wantsWide())
  {
    msg.command = CMD_WIDE;
  }
  else if (m_input && m_input->wantsBlink())
  {
    msg.command = CMD_BLINK;
  }
  else if (m_input && m_input->wantsBoop())
  {
    msg.command = CMD_BOOP;
  }

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
void EyeAnimator::processNetworkInput()
{
  if (!m_sync || !m_sync->hasController())
    return;

  uint32_t now = millis();

  if (m_sync->getLastRemoteTime() > 0 && m_sync->getLastRemoteState().eyeX != 0)
  {
    float remoteX = m_sync->getLastRemoteState().eyeX;
    float remoteY = m_sync->getLastRemoteState().eyeY;

    if ((now - m_sync->getLastRemoteTime()) < 100)
    {
      m_movement.setTargetAcquired();
      m_movement.setTarget(remoteX, remoteY);
      m_movement.setRandomMode(false);

      EyeSyncMessage msg = m_sync->getLastRemoteState();
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
        eyesNormal();
        break;
      }
    }
  }
  else
  {
    m_movement.setTargetLost();
  }
}