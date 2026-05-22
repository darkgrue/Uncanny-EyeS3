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
  m_forced = true;
  m_factor = 1.0f;
}

void BlinkFSM::wide()
{
  m_forced = true;
  m_factor = 0.0f;
}

void BlinkFSM::normal()
{
  m_forced = false;
}

/**
 * @brief Advance the FSM by elapsed microseconds.
 *
 * When not forced, checks if an automatic blink is due and triggers one.
 * During ENBLINK/DEBLINK, computes a smoothstep interpolation for natural
 * acceleration/deceleration. Optionally pauses at full closure before opening.
 */
bool BlinkFSM::update(uint32_t elapsed)
{
  if (m_forced)
  {
    return false;
  }

  uint32_t now = micros();

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
        m_factor = 0.0f;
        m_nextBlinkTime = random(BLINK_INTERVAL_MIN * 1000UL, BLINK_INTERVAL_MAX * 1000UL);
      }
    }
    else
    {
      float t = (float)dt / (float)m_blinkDuration;

#if BLINK_USE_SMOOTHstep
      t = t * t * (3.0f - 2.0f * t);
#endif

      if (m_state == ENBLINK)
      {
        m_factor = t;
      }
      else
      {
        m_factor = 1.0f - t;
      }
    }
    return true;
  }

  return false;
}