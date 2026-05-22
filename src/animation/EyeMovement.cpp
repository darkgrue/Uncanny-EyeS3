#include "EyeMovement.h"
#include <Arduino.h>
#include <math.h>

EyeMovement::EyeMovement() {
    m_currentX = 0.5f;
    m_currentY = 0.5f;
    m_startX = m_currentX;
    m_startY = m_currentY;
    m_targetX = m_currentX;
    m_targetY = m_currentY;
}

void EyeMovement::setTarget(float x, float y) {
    // Clamp to bounds
    float r = sqrt(x * x + y * y);
    if (r > m_boundsRadius) {
        x = (x / r) * m_boundsRadius;
        y = (y / r) * m_boundsRadius;
    }

    m_targetX = x;
    m_targetY = y;

    // If user sets a target, we're no longer idle
    if (r > 0.01f) {
        m_idle = false;
    }
}

void EyeMovement::setTargetAcquired() {
    m_lastTrackTime = millis();
    m_idle = true;
}

void EyeMovement::setTargetLost() {
    m_lastTrackTime = millis();
    m_idle = true;
}

void EyeMovement::setRandomDuration(uint32_t minMs, uint32_t maxMs) {
    m_minDuration = minMs;
    m_maxDuration = maxMs;
}

void EyeMovement::setRandomMode(bool enabled) {
    m_randomMode = enabled;
    if (enabled && !m_moving) {
        startRandomMove();
    }
}

void EyeMovement::startRandomMove() {
    // Store current position as start point for smooth interpolation
    m_startX = m_currentX;
    m_startY = m_currentY;

    // Generate random movement amplitude using lognormal distribution
    // This makes small movements MUCH more common than large ones
    // Real saccades follow a lognormal distribution - most are small microsaccades
    float u1 = (float)random(0, 1000) / 1000.0f;
    float u2 = (float)random(0, 1000) / 1000.0f;

    // Box-Muller transform for normal distribution, then exp for lognormal
    float normalSample = sqrt(-2.0f * log(u1 + 0.0001f)) * cos(2.0f * PI * u2);
    float lognormalSample = exp(normalSample * EYE_MOVE_LOGNORMAL_SIGMA + EYE_MOVE_LOGNORMAL_OFFSET);

    // Clamp amplitude to bounds using defines
    float amplitude = lognormalSample * m_boundsRadius * EYE_MOVE_AMPLITUDE_SCALE;
    amplitude = constrain(amplitude, EYE_MOVE_MIN_AMPLITUDE, m_boundsRadius * EYE_MOVE_MAX_AMPLITUDE_SCALE);

    // Random direction (uniform on circle)
    float angle = (float)random(0, 6283) / 1000.0f;  // 0 to 2*PI

    // Initial random target
    float targetX = m_currentX + cos(angle) * amplitude;
    float targetY = m_currentY + sin(angle) * amplitude;

    // Compute current distance from center (0.5, 0.5)
    float distX = m_currentX - 0.5f;
    float distY = m_currentY - 0.5f;
    float currentDist = sqrt(distX * distX + distY * distY);

    // Apply centering bias - the further from center, the stronger the pull back
    // This mimics natural human eye behavior where peripheral movements
    // often end with a corrective glance back toward center
    float centerBias = currentDist * EYE_MOVE_CENTER_BIAS_FACTOR;
    centerBias = constrain(centerBias, 0.0f, EYE_MOVE_CENTER_BIAS_MAX);

    // Blend target toward center based on how far we currently are
    float biasedTargetX = targetX * (1.0f - centerBias);
    float biasedTargetY = targetY * (1.0f - centerBias);

    m_targetX = biasedTargetX;
    m_targetY = biasedTargetY;

    // Clamp target to bounds
    float tr = sqrt(m_targetX * m_targetX + m_targetY * m_targetY);
    if (tr > m_boundsRadius) {
        m_targetX = (m_targetX / tr) * m_boundsRadius;
        m_targetY = (m_targetY / tr) * m_boundsRadius;
    }

    // Scale duration with movement distance - larger movements take longer
    float moveX = m_targetX - m_startX;
    float moveY = m_targetY - m_startY;
    float distance = sqrt(moveX * moveX + moveY * moveY);
    float distanceRatio = distance / (m_boundsRadius * 0.5f);

    // Base duration for small movements, longer for larger ones
    uint32_t baseDuration = random(EYE_MOVE_BASE_DURATION_MIN, EYE_MOVE_BASE_DURATION_MAX);
    uint32_t extraDuration = (uint32_t)(distanceRatio * EYE_MOVE_DURATION_DISTANCE_SCALE);
    m_moveDuration = baseDuration + extraDuration;
    m_moveDuration = constrain(m_moveDuration, EYE_MOVE_DURATION_MIN, EYE_MOVE_DURATION_MAX);

    m_moveStartTime = millis();
    m_moving = true;
    m_idle = false;
}

void EyeMovement::moveTo(float x, float y, uint32_t durationMs) {
    setTarget(x, y);
    m_moveDuration = durationMs;
    m_moveStartTime = millis();
    m_moving = true;
}

// Sigmoid-shaped easing for smooth acceleration and deceleration
// Approximates natural saccade velocity profile
static float saccadeEasing(float t) {
    float steepness = EYE_MOVE_EASING_STEEPNESS;
    if (t < 0.5f) {
        return 0.5f * (1.0f - tanh(steepness * (1.0f - 2.0f * t)));
    } else {
        return 0.5f * (1.0f + tanh(steepness * (2.0f * t - 1.0f)));
    }
}

bool EyeMovement::update(uint32_t dt) {
    (void)dt;  // Duration is time-based, not frame-based
    if (!m_moving) {
        if (m_randomMode) {
            if (m_idle) {
                // Wait for saccade delay after losing target
                uint32_t now = millis();
                if (now - m_lastTrackTime >= m_saccadeDelayAfterTrack) {
                    m_idle = false;
                    startRandomMove();
                }
                return false;
            } else {
                startRandomMove();
            }
        }
        return false;
    }

    uint32_t now = millis();
    uint32_t elapsed = now - m_moveStartTime;

    if (elapsed >= m_moveDuration) {
        // Movement complete
        m_currentX = m_targetX;
        m_currentY = m_targetY;
        m_moving = false;

        if (m_randomMode) {
            // Enter idle state and record completion time
            // Next saccade will be delayed by fixation pause
            m_idle = true;
            m_lastTrackTime = now;
            // Duration for next pause (time spent idle before next movement)
            m_moveDuration = random(EYE_MOVE_FIXATION_PAUSE_MIN, EYE_MOVE_FIXATION_PAUSE_MAX);
            m_moveStartTime = now;
        }
        return true;
    }

    // Frame-rate independent sigmoid easing
    float t = (float)elapsed / (float)m_moveDuration;
    float e = saccadeEasing(t);

    m_currentX = m_startX + (m_targetX - m_startX) * e;
    m_currentY = m_startY + (m_targetY - m_startY) * e;

    return true;
}