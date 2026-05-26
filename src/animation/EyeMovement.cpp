/**
 * @file EyeMovement.cpp
 * @brief Implementation of saccadic eye movement with lognormal distribution.
 *
 * Generates realistic random eye movements using a lognormal amplitude
 * distribution (most movements are small microsaccades) combined with a
 * centering bias that pulls the eye back toward center after peripheral
 * movements. Movement timing scales with distance, and a sigmoid easing
 * function approximates the characteristic velocity profile of real saccades.
 */
#include "EyeMovement.h"
#include <Arduino.h>
#include <math.h>

EyeMovement::EyeMovement()
{
  m_currentX = 0.0f;
  m_currentY = 0.0f;
  m_startX = m_currentX;
  m_startY = m_currentY;
  m_targetX = m_currentX;
  m_targetY = m_currentY;
}

/**
 * @brief Set the target eye position.
 *
 * Clamps the target to the movement bounds radius. If the target radius
 * is > 0.01, marks the eye as no longer idle (external target is active).
 */
void EyeMovement::setTarget(float x, float y)
{
  float r = sqrt(x * x + y * y);
  if (r > m_boundsRadius)
  {
    x = (x / r) * m_boundsRadius;
    y = (y / r) * m_boundsRadius;
  }

  m_targetX = x;
  m_targetY = y;

  if (r > 0.01f)
  {
    m_idle = false;
  }
}

/** @brief Mark that an external target is lost; starts the saccade delay countdown. */
void EyeMovement::setTargetLost()
{
  m_lastTrackTime = millis();
  m_moveDuration = m_saccadeDelayAfterTrack;
  m_idle = true;
}

/** @brief Set the duration range for random movements. */
void EyeMovement::setRandomDuration(uint32_t minMs, uint32_t maxMs)
{
  m_minDuration = minMs;
  m_maxDuration = maxMs;
}

/**
 * @brief Enable or disable autonomous random movement mode.
 *
 * When enabled, starts a random saccade immediately only if not already
 * moving and not in an idle fixation pause. If the idle pause is active,
 * it is left intact and will expire naturally via update().
 */
void EyeMovement::setRandomMode(bool enabled)
{
  m_randomMode = enabled;
  if (enabled && !m_moving && !m_idle)
  {
    startRandomMove();
  }
}

/**
 * @brief Generate and execute a new random saccade.
 *
 * Uses Box-Muller transform to sample a lognormal distribution for movement
 * amplitude. Random direction is uniform on the circle. A centering bias
 * nudges the target toward center proportional to how far the eye currently
 * is. Duration scales with movement distance for realistic timing.
 */
void EyeMovement::startRandomMove()
{
  m_startX = m_currentX;
  m_startY = m_currentY;

  float u1 = (float)random(0, 1000) / 1000.0f;
  float u2 = (float)random(0, 1000) / 1000.0f;

  float normalSample = sqrt(-2.0f * log(u1 + 0.0001f)) * cos(2.0f * PI * u2);
  float lognormalSample = exp(normalSample * EYE_MOVE_LOGNORMAL_SIGMA + EYE_MOVE_LOGNORMAL_OFFSET);

  float amplitude = lognormalSample * m_boundsRadius * EYE_MOVE_AMPLITUDE_SCALE;
  amplitude = constrain(amplitude, EYE_MOVE_MIN_AMPLITUDE, m_boundsRadius * EYE_MOVE_MAX_AMPLITUDE_SCALE);

  float angle = (float)random(0, 6283) / 1000.0f;

  float targetX = m_currentX + cos(angle) * amplitude;
  float targetY = m_currentY + sin(angle) * amplitude;

  float distX = m_currentX;
  float distY = m_currentY;
  float currentDist = sqrt(distX * distX + distY * distY);

  float centerBias = currentDist * EYE_MOVE_CENTER_BIAS_FACTOR;
  centerBias = constrain(centerBias, 0.0f, EYE_MOVE_CENTER_BIAS_MAX);

  float biasedTargetX = targetX * (1.0f - centerBias);
  float biasedTargetY = targetY * (1.0f - centerBias);

  m_targetX = biasedTargetX;
  m_targetY = biasedTargetY;

  float tr = sqrt(m_targetX * m_targetX + m_targetY * m_targetY);
  if (tr > m_boundsRadius)
  {
    m_targetX = (m_targetX / tr) * m_boundsRadius;
    m_targetY = (m_targetY / tr) * m_boundsRadius;
  }

  float moveX = m_targetX - m_startX;
  float moveY = m_targetY - m_startY;
  float distance = sqrt(moveX * moveX + moveY * moveY);
  float distanceRatio = distance / (m_boundsRadius * 0.5f);

  uint32_t baseDuration = random(EYE_MOVE_BASE_DURATION_MIN, EYE_MOVE_BASE_DURATION_MAX);
  uint32_t extraDuration = (uint32_t)(distanceRatio * EYE_MOVE_DURATION_DISTANCE_SCALE);
  m_moveDuration = baseDuration + extraDuration;
  m_moveDuration = constrain(m_moveDuration, EYE_MOVE_DURATION_MIN, EYE_MOVE_DURATION_MAX);

  m_moveStartTime = millis();
  m_moving = true;
  m_idle = false;
}

/**
 * @brief Directly set the current eye position, cancelling any active movement.
 *
 * Bounds-clamps the position to the same circle used by setTarget(), then
 * writes it to all position fields so the saccade system restarts cleanly
 * from this point when control returns to random or face-tracking mode.
 */
void EyeMovement::setCurrentPosition(float x, float y)
{
  float r = sqrtf(x * x + y * y);
  if (r > m_boundsRadius)
  {
    x = (x / r) * m_boundsRadius;
    y = (y / r) * m_boundsRadius;
  }
  m_currentX = x;
  m_currentY = y;
  m_startX   = x;
  m_startY   = y;
  m_targetX  = x;
  m_targetY  = y;
  m_moving   = false;
}

/**
 * @brief Start a directed movement to a specific position.
 * @param x Target X position.
 * @param y Target Y position.
 * @param durationMs Movement duration in milliseconds.
 */
void EyeMovement::moveTo(float x, float y, uint32_t durationMs)
{
  m_startX = m_currentX;
  m_startY = m_currentY;
  setTarget(x, y);
  m_moveDuration = durationMs;
  m_moveStartTime = millis();
  m_moving = true;
}

/**
 * @brief Sigmoid-shaped easing approximating natural saccade velocity.
 *
 * Uses tanh for smooth acceleration/deceleration with steepness controlled
 * by EYE_MOVE_EASING_STEEPNESS. At t=0.5 the output crosses 0.5 (center
 * of movement), matching the peak-velocity-mid movement characteristic.
 */
static float saccadeEasing(float t)
{
  float steepness = EYE_MOVE_EASING_STEEPNESS;
  if (t < 0.5f)
  {
    return 0.5f * (1.0f - tanh(steepness * (1.0f - 2.0f * t)));
  }
  else
  {
    return 0.5f * (1.0f + tanh(steepness * (2.0f * t - 1.0f)));
  }
}

/**
 * @brief Advance movement by one tick.
 *
 * When not moving in random mode: if idle, waits for the saccade delay after
 * losing a target; otherwise starts a new random move immediately.
 *
 * During movement: applies sigmoid easing to interpolate from start to target
 * based on elapsed/ duration. On completion, enters fixation idle state with
 * a random pause duration before the next saccade.
 *
 * @return true if the eye moved this tick.
 */
bool EyeMovement::update()
{
  uint32_t now = millis();

  if (!m_moving)
  {
    if (m_randomMode)
    {
      if (m_idle)
      {
        if (now - m_lastTrackTime >= m_moveDuration)
        {
          m_idle = false;
          startRandomMove();
        }
        return false;
      }
      else
      {
        startRandomMove();
      }
    }
    return false;
  }

  uint32_t elapsed = now - m_moveStartTime;

  if (elapsed >= m_moveDuration)
  {
    m_currentX = m_targetX;
    m_currentY = m_targetY;
    m_moving = false;

    if (m_randomMode)
    {
      m_idle = true;
      m_lastTrackTime = now;
      if (m_postMoveIdleMs > 0)
      {
        m_moveDuration = m_postMoveIdleMs;
        m_postMoveIdleMs = 0;
      }
      else
      {
        m_moveDuration = random(EYE_MOVE_FIXATION_PAUSE_MIN, EYE_MOVE_FIXATION_PAUSE_MAX);
      }
    }
    return true;
  }

  float t = (float)elapsed / (float)m_moveDuration;
  float e = saccadeEasing(t);

  m_currentX = m_startX + (m_targetX - m_startX) * e;
  m_currentY = m_startY + (m_targetY - m_startY) * e;

  return true;
}