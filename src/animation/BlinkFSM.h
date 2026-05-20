#ifndef BLINK_FSM_H
#define BLINK_FSM_H

#include "common/EyeState.h"

// Finite State Machine for eye blink animation
// Mirrors the M4_Eyes blink behavior
class BlinkFSM {
public:
    BlinkFSM();
    
    // Reset to initial state
    void reset();
    
    // Trigger a blink (resets next automatic blink timer)
    void trigger();
    
    // Force eyelids closed
    void close();
    
    // Force eyelids wide open
    void wide();
    
    // Return to normal tracking
    void normal();
    
    // Update state, returns true if state changed
    // elapsed = micros() since last update
    bool update(uint32_t elapsed);
    
    // Get current blink factor (0.0 = open, 1.0 = closed)
    float getFactor() const { return m_factor; }
    
    // Get current state
    EyeBlinkState getState() const { return m_state; }
    
    // Check if eyelids are forced (not tracking)
    bool isForced() const { return m_forced; }

private:
    EyeBlinkState m_state = NOBLINK;
    float m_factor = 0.0f;
    bool m_forced = false;
    
    uint32_t m_stateStart = 0;
    uint32_t m_blinkDuration = 0;
    uint32_t m_nextBlinkTime = 0;
    
    // Timing constants (microseconds)
    static constexpr uint32_t MIN_BLINK_DURATION = 36000;   // ~1/28 sec
    static constexpr uint32_t MAX_BLINK_DURATION = 72000;    // ~1/14 sec
    static constexpr uint32_t MIN_TIME_TO_BLINK = 2000000;  // 2 sec minimum
    static constexpr uint32_t MAX_TIME_TO_BLINK = 8000000;  // 8 sec maximum
};

#endif // BLINK_FSM_H
