/**
 * @file BlinkFSM.cpp
 * @brief Implementation of blink animation state machine.
 *
 * Uses micros()-based timing for frame-independent animation. Closing and
 * opening durations are randomized within configured ranges. Automatic blink
 * interval is also randomized (2-8 seconds) for natural variation. Supports
 * optional pause at full closure before opening.
 */
#include "BlinkFSM.h"
#include "EyeMovement.h"
#include <Arduino.h>

BlinkFSM::BlinkFSM()
{
  reset();
}

void BlinkFSM::reset()
{
  m_state = NOBLINK;
  m_factor = 0.0f;
  m_forced = false;
  m_targetGap = 0.0f;
  m_stateStart = 0;
  m_blinkDuration = 0;
  m_nextBlinkTime = random(BLINK_INTERVAL_MIN * 1000UL, BLINK_INTERVAL_MAX * 1000UL);
}

void BlinkFSM::trigger()
{
  m_state = ENBLINK;
  m_forced = false;
  m_stateStart = micros();
  m_blinkDuration = random(BLINK_DURATION_CLOSE_MIN * 1000UL, BLINK_DURATION_CLOSE_MAX * 1000UL);
  m_nextBlinkTime = m_blinkDuration * 3 + random(BLINK_INTERVAL_MIN * 1000UL);
}

void BlinkFSM::close()
{
  if (m_state == FORCINGCLOSE)
    return; // already animating or holding — don't restart
  m_forced = true;
  m_forceStart = m_factor;
  m_forceTarget = 1.0f;
  m_blinkDuration = EYELID_CLOSE_DURATION * 1000UL;
  m_stateStart = micros();
  m_state = FORCINGCLOSE;
}

void BlinkFSM::wide()
{
  wideTo(0.0f);
}

void BlinkFSM::wideTo(float gap)
{
  if (m_state == FORCINGWIDE && fabsf(m_forceTarget - gap) < 0.001f)
    return; // already animating toward this target — don't restart
  m_forced = true;
  m_forceStart = m_factor;
  m_forceTarget = gap;
  m_blinkDuration = EYELID_WIDE_DURATION * 1000UL;
  m_stateStart = micros();
  m_state = FORCINGWIDE;
}

void BlinkFSM::setNormalGap(float gap)
{
  m_targetGap = gap;
  if (m_state == NOBLINK)
    m_factor = gap;
}

void BlinkFSM::normal()
{
  if (m_state == FORCINGNORMAL)
    return; // already animating back to normal
  if (!m_forced)
    return; // only exit forced states; autonomous blinks must not be interrupted
  m_forced = true; // hold off auto-blink while the transition plays out
  m_forceStart = m_factor;
  m_forceTarget = m_targetGap;
  m_blinkDuration = EYELID_NORMAL_DURATION * 1000UL;
  m_stateStart = micros();
  m_state = FORCINGNORMAL;
}

/**
 * @brief Advance the FSM by elapsed microseconds.
 *
 * When not forced, checks if an automatic blink is due and triggers one.
 * During ENBLINK/DEBLINK, computes a smoothstep interpolation for natural
 * acceleration/deceleration. Optionally pauses at full closure before opening.
 */
bool BlinkFSM::update()
{
  uint32_t now = micros();

  // Animated forced transitions — run before the m_forced guard so they
  // actually advance each frame.
  if (m_state == FORCINGCLOSE || m_state == FORCINGWIDE || m_state == FORCINGNORMAL)
  {
    uint32_t dt = now - m_stateStart;

    if (dt >= m_blinkDuration)
    {
      m_factor = m_forceTarget;
      if (m_state == FORCINGNORMAL)
      {
        // Transition complete: resume normal idle operation.
        m_state = NOBLINK;
        m_forced = false;
        m_stateStart = now;
        m_nextBlinkTime = random(BLINK_INTERVAL_MIN * 1000UL, BLINK_INTERVAL_MAX * 1000UL);
        return true;
      }
      // FORCINGCLOSE / FORCINGWIDE: hold at target until button is released.
      return false;
    }

    float t = (float)dt / (float)m_blinkDuration;
    t = t * t * (3.0f - 2.0f * t); // smoothstep
    m_factor = m_forceStart + (m_forceTarget - m_forceStart) * t;
    return true;
  }

  if (m_forced)
  {
    return false;
  }

  if (m_state == NOBLINK && (now - m_stateStart) >= m_nextBlinkTime)
  {
    trigger();
  }

  if (m_state == ENBLINK || m_state == DEBLINK)
  {
    uint32_t dt = now - m_stateStart;

    if (dt >= m_blinkDuration)
    {
      if (m_state == ENBLINK)
      {
#if BLINK_PAUSE_AT_CLOSURE > 0
        if (dt < m_blinkDuration + BLINK_PAUSE_AT_CLOSURE * 1000UL)
        {
          m_factor = 1.0f;
          return true;
        }
#endif
        m_state = DEBLINK;
        m_stateStart = now;
        m_blinkDuration = random(BLINK_DURATION_OPEN_MIN * 1000UL, BLINK_DURATION_OPEN_MAX * 1000UL);
        m_factor = 1.0f;
      }
      else
      {
        m_state = NOBLINK;
        m_factor = m_targetGap;
        m_nextBlinkTime = random(BLINK_INTERVAL_MIN * 1000UL, BLINK_INTERVAL_MAX * 1000UL);
      }
    }
    else
    {
      float t = (float)dt / (float)m_blinkDuration;

#if BLINK_USE_SMOOTHSTEP
      t = t * t * (3.0f - 2.0f * t);
#endif

      if (m_state == ENBLINK)
      {
        m_factor = m_targetGap + (1.0f - m_targetGap) * t;
      }
      else
      {
        m_factor = 1.0f - (1.0f - m_targetGap) * t;
      }
    }
    return true;
  }

  return false;
}