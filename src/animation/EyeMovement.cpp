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
    float r = m_boundsRadius * 0.5f;  // Reduced radius - smaller movements

    // Random position within circle
    float angle = random(0, 6283) / 1000.0f;  // 0 to 2*PI
    float radius = sqrt((float)random(0, 1000) / 1000.0f) * r;

    m_targetX = cos(angle) * radius;
    m_targetY = sin(angle) * radius;

    m_moveDuration = random(m_minDuration, m_maxDuration);
    m_moveStartTime = millis();
    m_moving = true;
    m_idle = false;  // No longer idle - actively moving
}

void EyeMovement::moveTo(float x, float y, uint32_t durationMs) {
    setTarget(x, y);
    m_moveDuration = durationMs;
    m_moveStartTime = millis();
    m_moving = true;
}

bool EyeMovement::update(uint32_t dt) {
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
// Set up next pause before random movement
            m_moveDuration = random(500, 2000);  // 500ms to 2s pause between movements
            m_moveStartTime = now;
        }
        return true;
    }
    
    // Easing function: 3e^2 - 2e^3 (smoothstep)
    float e = (float)elapsed / (float)m_moveDuration;
    e = 3.0f * e * e - 2.0f * e * e * e;
    
    m_currentX = m_startX + (m_targetX - m_startX) * e;
    m_currentY = m_startY + (m_targetY - m_startY) * e;
    
    return true;
}
