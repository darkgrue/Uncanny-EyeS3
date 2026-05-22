#ifndef EYE_MOVEMENT_H
#define EYE_MOVEMENT_H

#include "common/EyeState.h"

// Handles smooth eye movement with easing
class EyeMovement {
public:
    EyeMovement();
    
    // Set target position (normalized -1.0 to +1.0)
    void setTarget(float x, float y);
    
    // Set movement bounds radius (0.0 to 1.0)
    void setBounds(float radius) { m_boundsRadius = radius; }
    
    // Set duration range for random movements
    void setRandomDuration(uint32_t minMs, uint32_t maxMs);
    
    // Enable/disable random movement
    void setRandomMode(bool enabled);
    
    // Set delay before starting random movement after losing target
    void setSaccadeDelay(uint32_t ms) { m_saccadeDelayAfterTrack = ms; }
    
    // Notify that a target is acquired (resets idle timer)
    void setTargetAcquired();
    
    // Notify that target is lost (starts idle delay countdown)
    void setTargetLost();
    
    // Check if currently in idle mode (no target, waiting for saccade delay)
    bool isIdle() const { return m_idle; }
    
    // Update movement, returns true if eye moved
    // dt = micros() since last update
    bool update(uint32_t dt);
    
    // Get current interpolated position
    float getX() const { return m_currentX; }
    float getY() const { return m_currentY; }
    
    // Get raw target (before bounds clamping)
    float getTargetX() const { return m_targetX; }
    float getTargetY() const { return m_targetY; }
    
    // Check if currently moving
    bool isMoving() const { return m_moving; }
    
    // Start a new random movement
    void startRandomMove();
    
    // Start moving to specific position
    void moveTo(float x, float y, uint32_t durationMs);

private:
    void updatePosition();
    
    float m_currentX = 0.5f;
    float m_currentY = 0.5f;
    float m_startX = 0.5f;
    float m_startY = 0.5f;
    float m_targetX = 0.5f;
    float m_targetY = 0.5f;
    float m_boundsRadius = 0.5f;
    
    bool m_moving = false;
    bool m_randomMode = true;
    
    uint32_t m_moveStartTime = 0;
    uint32_t m_moveDuration = 0;
    
uint32_t m_minDuration = 250;     // 250ms - smoother movement
    uint32_t m_maxDuration = 500;    // 500ms - natural saccade speed
    
    uint32_t m_saccadeDelayAfterTrack = 3000;  // 3 second delay before resuming idle movement
    uint32_t m_lastTrackTime = 0;             // Timestamp of last valid target
    bool m_idle = false;                       // True when waiting for saccade delay after losing target
};

#endif // EYE_MOVEMENT_H
