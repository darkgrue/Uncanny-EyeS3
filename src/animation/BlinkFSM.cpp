#include "BlinkFSM.h"
#include <Arduino.h>

BlinkFSM::BlinkFSM() {
    reset();
}

void BlinkFSM::reset() {
    m_state = NOBLINK;
    m_factor = 0.0f;
    m_forced = false;
    m_stateStart = 0;
    m_blinkDuration = 0;
    m_nextBlinkTime = random(MIN_TIME_TO_BLINK, MAX_TIME_TO_BLINK);
}

void BlinkFSM::trigger() {
    m_state = ENBLINK;
    m_forced = false;
    m_stateStart = micros();
    m_blinkDuration = random(MIN_BLINK_DURATION, MAX_BLINK_DURATION);
    m_nextBlinkTime = m_blinkDuration * 3 + random(4000000);
}

void BlinkFSM::close() {
    m_forced = true;
    m_factor = 1.0f;
}

void BlinkFSM::wide() {
    m_forced = true;
    m_factor = 0.0f;
}

void BlinkFSM::normal() {
    m_forced = false;
}

bool BlinkFSM::update(uint32_t elapsed) {
    if (m_forced) {
        // Forced mode: no automatic blinking
        return false;
    }
    
    uint32_t now = micros();
    
    // Check for automatic blink trigger
    if (m_state == NOBLINK && (now - m_stateStart) >= m_nextBlinkTime) {
        trigger();
    }
    
    // Update blink animation
    if (m_state == ENBLINK || m_state == DEBLINK) {
        uint32_t dt = now - m_stateStart;
        
        if (dt >= m_blinkDuration) {
            // State transition
            if (m_state == ENBLINK) {
                m_state = DEBLINK;
                m_stateStart = now;
                m_blinkDuration = m_blinkDuration * 2;  // Opening is slower
                m_factor = 1.0f;
            } else {
                m_state = NOBLINK;
                m_factor = 0.0f;
                m_nextBlinkTime = random(MIN_TIME_TO_BLINK, MAX_TIME_TO_BLINK);
            }
        } else {
            // Interpolate factor
            float t = (float)dt / (float)m_blinkDuration;
            if (m_state == ENBLINK) {
                m_factor = t;  // 0 -> 1 (closing)
            } else {
                m_factor = 1.0f - t;  // 1 -> 0 (opening)
            }
        }
        return true;
    }
    
    return false;
}
