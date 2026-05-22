#ifndef EYE_MOVEMENT_H
#define EYE_MOVEMENT_H

#include "common/EyeState.h"

// ============================================================================
// EYE MOVEMENT PARAMETERS
// ============================================================================
// These parameters control the characteristics of saccadic eye movements.
// Human eye movements naturally follow a lognormal distribution - most are
// small microsaccades (~0.5-2 degrees) with occasional larger saccades.
//
// TUNING GUIDE:
// - Increasing a parameter moves behavior in that direction
// - Decreasing moves in opposite direction
// ============================================================================

// ---------------------------------------------------------------------------
// LOGNORMAL DISTRIBUTION PARAMETERS
// ---------------------------------------------------------------------------
// These control the amplitude (size) distribution of random eye movements.
// Lognormal is used because real saccades follow this distribution - most
// movements are small, with a long tail of larger movements.

#define EYE_MOVE_LOGNORMAL_SIGMA  0.35f
// Controls the spread/shape of the amplitude distribution.
// Higher = more variation in movement sizes, more large saccades
// Lower = tighter clustering around small movements
// Typical range: 0.2 (very small bias) to 0.6 (more large saccades)
// Recommended: 0.3-0.4 for natural-appearing movement

#define EYE_MOVE_LOGNORMAL_OFFSET  -0.2f
// Shifts the entire distribution (negative = smaller movements).
// More negative = bias toward tiny microsaccades
// Zero or positive = allows larger saccades more frequently
// Typical range: -0.4 (extreme small bias) to 0.1 (larger movements)
// Recommended: -0.3 to -0.1 for natural behavior

#define EYE_MOVE_AMPLITUDE_SCALE   0.5f
// Scales the lognormal output relative to bounds radius.
// Higher = larger maximum amplitudes
// Lower = smaller maximum amplitudes
// Range: 0.0 to 1.0 (relative to m_boundsRadius)
// Recommended: 0.4-0.6

#define EYE_MOVE_MIN_AMPLITUDE     0.01f
// Minimum movement amplitude (prevents zero/stuck movements).
// Higher = fewer tiny movements, more "purposeful" looking
// Lower = allow more tiny microsaccades
// Recommended: 0.005-0.02

#define EYE_MOVE_MAX_AMPLITUDE_SCALE 0.8f
// Maximum amplitude as fraction of bounds radius.
// Higher = allow movements closer to edge
// Lower = keep movements more central
// Range: 0.0 to 1.0
// Recommended: 0.6-0.9

// ---------------------------------------------------------------------------
// CENTERING BIAS PARAMETERS
// ---------------------------------------------------------------------------
// These control how strongly the eye is pulled back toward center after
// a movement. This mimics natural eye behavior where peripheral movements
// often trigger corrective glances back toward center.

#define EYE_MOVE_CENTER_BIAS_FACTOR  0.6f
// How strongly the eye is pulled toward center based on distance from center.
// 0.0 = no centering bias (movements stay wherever they go)
// 1.0 = strong centering (eye strongly prefers to return to center)
// Higher = more conservative, centered gaze
// Lower = more exploratory, varied positions
// Recommended: 0.4-0.7

#define EYE_MOVE_CENTER_BIAS_MAX     0.5f
// Cap on centering bias effect (0.0 to 0.5).
// Prevents extreme pull-back when eye is very far from center.
// Higher = allow more extreme positions before centering kicks in
// Lower = stronger centering at moderate distances
// Recommended: 0.3-0.6

// ---------------------------------------------------------------------------
// MOVEMENT DURATION PARAMETERS
// ---------------------------------------------------------------------------
// These control how long saccades take to complete. Real human saccades
// range from ~100ms for small to ~300ms for large movements.

#define EYE_MOVE_BASE_DURATION_MIN   200  // milliseconds
#define EYE_MOVE_BASE_DURATION_MAX   350  // milliseconds
// Random base duration for small movements.
// Higher = slower, more deliberate looking
// Lower = quicker, more energetic movements
// Recommended: 150-250ms for base minimum

#define EYE_MOVE_DURATION_DISTANCE_SCALE  150  // milliseconds
// Additional duration added based on movement distance.
// Scales with distanceRatio (actual distance / half bounds).
// Higher = more speed difference between small and large saccades
// Lower = more uniform timing regardless of size
// Recommended: 100-200ms

#define EYE_MOVE_DURATION_MIN       150  // milliseconds
#define EYE_MOVE_DURATION_MAX       500  // milliseconds
// Hard limits on movement duration.
// Lower min = snappier small movements
// Higher max = allow slow large movements
// Recommended: 100-200ms min, 300-600ms max

// ---------------------------------------------------------------------------
// FIXATION / IDLE TIMING PARAMETERS
// ---------------------------------------------------------------------------
// These control the pause between saccades (fixation period) and the
// delay before resuming idle movement after losing a tracking target.

#define EYE_MOVE_FIXATION_PAUSE_MIN  4000  // milliseconds
#define EYE_MOVE_FIXATION_PAUSE_MAX  8000  // milliseconds
// How long the eye holds still between random saccades.
// Higher = longer, more deliberate pauses, calmer appearance
// Lower = more active, curious scanning behavior
// Human fixation typically: 200-600ms for reading, 1-3s for scene viewing
// Recommended: 2000-10000ms depending on desired activity level

#define EYE_MOVE_SACCADE_DELAY       3000  // milliseconds
// Delay before starting random movement after losing a target.
// Higher = more hesitation when tracking lost
// Lower = quicker return to idle scanning
// Recommended: 2000-5000ms

// ---------------------------------------------------------------------------
// EASING PARAMETERS
// ---------------------------------------------------------------------------
// These control the velocity profile of saccades using sigmoid easing.
// Real saccades have rapid acceleration and deceleration with peak
// velocity in the middle - this sigmoid approximates that profile.

#define EYE_MOVE_EASING_STEEPNESS    3.0f
// Controls how sharp/step-like the easing transition is.
// Higher = more abrupt start/stop, more robotic
// Lower = smoother, more organic acceleration/deceleration
// Range: 1.0 (very smooth) to 5.0 (very sharp)
// Recommended: 2.0-4.0

// ============================================================================
// EYELID / BLINK PARAMETERS
// ============================================================================
// These parameters control realistic eyelid animation and blinks.
// Human blinks follow a characteristic pattern: quick closing phase,
// brief pause at closure, then slower opening. Random blinks occur
// at intervals that follow natural human behavior patterns.
//
// BLINK TIMING PARAMETERS
// ---------------------------------------------------------------------------

#define BLINK_DURATION_CLOSE_MIN     60    // milliseconds
#define BLINK_DURATION_CLOSE_MAX     100   // milliseconds
// Duration of the closing phase of a blink.
// Higher = slower, lazier-looking blink
// Lower = snappier, more alert blink
// Human range: 60-120ms typical
// Recommended: 60-100ms

#define BLINK_DURATION_OPEN_MIN      120   // milliseconds
#define BLINK_DURATION_OPEN_MAX      200   // milliseconds
// Duration of the opening phase (should be slower than closing).
// Higher = lingering, sultry look
// Lower = quick, alert
// Opening is naturally slower than closing in humans
// Recommended: 120-200ms (roughly 1.5-2x closing duration)

#define BLINK_PAUSE_AT_CLOSURE       20    // milliseconds
// Brief pause when eyelids are fully closed during a blink.
// Mimics natural behavior where eyes linger briefly when closed.
// Set to 0 to disable pause.
// Recommended: 10-30ms

// ---------------------------------------------------------------------------
// AUTOMATIC / RANDOM BLINK PARAMETERS
// ---------------------------------------------------------------------------

#define BLINK_INTERVAL_MIN           2000  // milliseconds (2 seconds)
#define BLINK_INTERVAL_MAX           8000  // milliseconds (8 seconds)
// Random interval between automatic blinks when idle.
// Blinks occur at random intervals within this range to seem natural.
// Higher values = less frequent blinking, more focused appearance
// Lower values = more frequent blinking, nervous/alert appearance
// Human average: ~4-6 seconds between blinks during rest
// Recommended: 2000-8000ms depending on desired activity level

#define BLINK_PROBABILITY_BURST      0.15f
// Probability of a double-blink (two quick blinks in succession).
// 0.0 = no double blinks, 1.0 = always double blink
// Higher = more excitable/nervous character
// Lower = calmer, more deliberate character
// Recommended: 0.1-0.2 (10-20% chance)

#define BLINK_CHANCE_AFTER_SACCADE    0.08f
// Probability of an automatic blink occurring after a saccade (eye movement).
// When the eye makes a large movement, it may trigger a blink.
// 0.0 = blinks are purely time-based, 1.0 = every saccade triggers blink
// Higher = more reactive, appears more human
// Lower = more mechanical, predetermined blinking
// Recommended: 0.05-0.15

// ---------------------------------------------------------------------------
// EYELID SHAPE PARAMETERS
// ---------------------------------------------------------------------------

#define EYELID_UPPER_TRACK_STRENGTH  0.3f
// How much the upper eyelid tracks the pupil position (0.0 to 1.0).
// 0.0 = eyelid stays at fixed position regardless of gaze
// 1.0 = eyelid follows pupil, creating natural "looking up/down" effect
// This creates the effect of eyelids adjusting based on where you're looking.
// Recommended: 0.2-0.5 for subtle tracking, 0.0 for static eyelids

#define EYELID_LOWER_TRACK_STRENGTH   0.15f
// How much the lower eyelid tracks the pupil position.
// Lower eyelids have less movement range than upper.
// Recommended: 0.1-0.25

#define EYELID_DEFAULT_GAP            0.85f
// Default eyelid gap when fully open (0.0 = closed, 1.0 = wide open).
// Adjust this to control how "open" the eyes appear at rest.
// Higher = more exposed eye, alert appearance
// Lower = more demure, drowsy appearance
// Recommended: 0.7-0.95

#define EYELID_SQUINT_FACTOR          0.7f
// How much the eyelids close during a "squint" expression.
// Applied as a multiplier to the current eyelid gap.
// 0.0 = complete squint (nearly closed), 1.0 = no squint effect
// Recommended: 0.5-0.8

// ---------------------------------------------------------------------------
// ANIMATION SMOOTHING PARAMETERS
// ---------------------------------------------------------------------------

#define EYELID_SMOOTHING              0.1f
// Smoothing factor for eyelid position changes (0.0 to 1.0).
// Higher = smoother but slower responding animation
// Lower = snappier but potentially jerkier animation
// Uses exponential smoothing: newVal = lerp(prevVal, targetVal, smoothing)
// Recommended: 0.08-0.15 for 60-120 FPS displays

#define BLINK_USE_SMOOTHstep           1    // 1 = use smoothstep easing, 0 = linear
// Use smoothstep easing for more natural acceleration/deceleration.
// When enabled, blinks use ease-in-out instead of linear motion.
// Recommended: 1 (enabled) for more natural appearance

// ============================================================================
// End of parameters
// ============================================================================

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

    uint32_t m_minDuration = EYE_MOVE_BASE_DURATION_MIN;
    uint32_t m_maxDuration = EYE_MOVE_BASE_DURATION_MAX;

    uint32_t m_saccadeDelayAfterTrack = EYE_MOVE_SACCADE_DELAY;
    uint32_t m_lastTrackTime = 0;
    bool m_idle = false;
};

#endif // EYE_MOVEMENT_H