/**
 * @file EyeMovement.h
 * @brief Saccadic eye movement generator with lognormal distribution and centering bias.
 *
 * Human eye movements naturally follow a lognormal distribution â€” most are small
 * microsaccades (~0.5-2Â°) with occasional larger saccades. This module generates
 * realistic autonomous eye movement using that distribution, applies a centering
 * bias to pull the eye back toward center after peripheral movements, and uses
 * sigmoid easing to approximate the characteristic velocity profile of real saccades.
 *
 * Tuning guide: Each parameter section includes a description of its effect and
 * recommended range. Increasing a parameter moves behavior in that direction.
 *
 * ============================================================================
 * MOVEMENT PARAMETERS
 * ============================================================================
 *
 * Lognormal Distribution Parameters
 * --------------------------------
 * These control the amplitude (size) distribution of random eye movements.
 * Lognormal is used because real saccades follow this distribution â€” most
 * movements are small, with a long tail of larger movements.
 *
 * EYE_MOVE_LOGNORMAL_SIGMA: Spread/shape of the amplitude distribution.
 *   Higher = more variation, more large saccades. Range: 0.2-0.6. Default: 0.35.
 *
 * EYE_MOVE_LOGNORMAL_OFFSET: Shifts the distribution (negative = smaller).
 *   Range: -0.4 to 0.1. Default: -0.2.
 *
 * EYE_MOVE_AMPLITUDE_SCALE: Scales lognormal output relative to bounds radius.
 *   Range: 0.0-1.0. Default: 0.5.
 *
 * EYE_MOVE_MIN_AMPLITUDE: Minimum movement amplitude (prevents zero/stuck).
 *   Range: 0.005-0.02. Default: 0.01.
 *
 * EYE_MOVE_MAX_AMPLITUDE_SCALE: Max amplitude as fraction of bounds radius.
 *   Range: 0.0-1.0. Default: 0.8.
 *
 * Centering Bias Parameters
 * ------------------------
 * These control how strongly the eye is pulled back toward center after a movement.
 * This mimics natural behavior where peripheral movements trigger corrective glances.
 *
 * EYE_MOVE_CENTER_BIAS_FACTOR: Strength of centering pull (0.0-1.0). Default: 0.6.
 * EYE_MOVE_CENTER_BIAS_MAX: Cap on centering effect (0.0-0.5). Default: 0.5.
 *
 * Movement Duration Parameters
 * --------------------------
 * Real saccades range from ~100ms (small) to ~300ms (large).
 *
 * EYE_MOVE_BASE_DURATION_MIN/MAX: Base duration for small movements (ms). Default: 200-350.
 * EYE_MOVE_DURATION_DISTANCE_SCALE: Extra duration scaled by distance (ms). Default: 150.
 * EYE_MOVE_DURATION_MIN/MAX: Hard limits on duration (ms). Default: 150-500.
 *
 * Fixation / Idle Timing Parameters
 * ---------------------------------
 * EYE_MOVE_FIXATION_PAUSE_MIN/MAX: Pause between random saccades (ms). Default: 4000-8000.
 * EYE_MOVE_SACCADE_DELAY: Delay before resuming idle movement after losing target (ms). Default: 3000.
 *
 * Saccade Easing Parameters
 * -------------------------
 * Real saccades have rapid acceleration/deceleration; sigmoid easing approximates this.
 *
 * EYE_MOVE_EASING_STEEPNESS: Sharpness of the easing transition. Range: 1.0-5.0. Default: 3.0.
 *
 * ============================================================================
 * EYELID / BLINK PARAMETERS
 * ============================================================================
 * Human blinks: quick closing, brief pause at closure, slower opening.
 *
 * BLINK_DURATION_CLOSE_MIN/MAX: Closing phase duration (ms). Default: 60-100.
 * BLINK_DURATION_OPEN_MIN/MAX: Opening phase duration (ms). Default: 120-200.
 * BLINK_PAUSE_AT_CLOSURE: Pause at full closure (ms). Default: 20.
 *
 * BLINK_INTERVAL_MIN/MAX: Random interval between automatic blinks (ms). Default: 2000-8000.
 * BLINK_PROBABILITY_BURST: Chance of double-blink (0.0-1.0). Default: 0.15.
 * BLINK_CHANCE_AFTER_SACCADE: Chance of blink after eye movement. Default: 0.08.
 *
 * EYELID_UPPER_TRACK_STRENGTH: How much upper eyelid tracks pupil (0.0-1.0). Default: 0.3.
 * EYELID_LOWER_TRACK_STRENGTH: How much lower eyelid tracks pupil. Default: 0.15.
 * EYELID_SQUINT_FACTOR: Eyelid closure during squint (0.0-1.0). Default: 0.7.
 *
 * EYELID_SMOOTHING: Smoothing factor for eyelid position changes. Default: 0.1.
 * BLINK_USE_SMOOTHSTEP: Enable smoothstep easing (0/1). Default: 1.
 */
#ifndef EYE_MOVEMENT_H
#define EYE_MOVEMENT_H

#include "common/EyeState.h"

// ============================================================================
// EYE MOVEMENT PARAMETERS
// ============================================================================
// See file-level Doxygen documentation above for full parameter descriptions.
// ============================================================================

#define EYE_MOVE_LOGNORMAL_SIGMA 0.35f
#define EYE_MOVE_LOGNORMAL_OFFSET -0.2f
#define EYE_MOVE_AMPLITUDE_SCALE 0.5f
#define EYE_MOVE_MIN_AMPLITUDE 0.01f
#define EYE_MOVE_MAX_AMPLITUDE_SCALE 0.8f

#define EYE_MOVE_CENTER_BIAS_FACTOR 0.6f
#define EYE_MOVE_CENTER_BIAS_MAX 0.5f

#define EYE_MOVE_BASE_DURATION_MIN 200
#define EYE_MOVE_BASE_DURATION_MAX 350
#define EYE_MOVE_DURATION_DISTANCE_SCALE 150
#define EYE_MOVE_DURATION_MIN 150
#define EYE_MOVE_DURATION_MAX 500

#define EYE_MOVE_FIXATION_PAUSE_MIN 4000
#define EYE_MOVE_FIXATION_PAUSE_MAX 8000
#define EYE_MOVE_SACCADE_DELAY 3000

#define EYE_MOVE_EASING_STEEPNESS 3.0f

// ============================================================================
// EYELID / BLINK PARAMETERS
// ============================================================================

#define BLINK_DURATION_CLOSE_MIN 60
#define BLINK_DURATION_CLOSE_MAX 100
#define BLINK_DURATION_OPEN_MIN 120
#define BLINK_DURATION_OPEN_MAX 200
#define BLINK_PAUSE_AT_CLOSURE 20

#define BLINK_INTERVAL_MIN 2000
#define BLINK_INTERVAL_MAX 8000
#define BLINK_PROBABILITY_BURST 0.15f
#define BLINK_CHANCE_AFTER_SACCADE 0.08f

#define EYELID_UPPER_TRACK_STRENGTH 0.3f
#define EYELID_LOWER_TRACK_STRENGTH 0.15f
#define EYELID_SQUINT_FACTOR 0.7f
#define EYELID_NORMAL_CLOSURE_DEFAULT 0.15f  // Default eyelid coverage when open (0.0-1.0)
#define EYELID_WIDE_CLOSURE_DEFAULT 0.0f       // Default eyelid coverage when wide (0.0-1.0)
#define EYELID_SMOOTHING 0.1f
#define BLINK_USE_SMOOTHSTEP 1

// ============================================================================
// End of parameters
// ============================================================================

/**
 * @brief Generates smooth, realistic saccadic eye movement.
 *
 * Produces natural-looking eye motion using a lognormal amplitude distribution,
 * centering bias to simulate corrective glances, and sigmoid easing for realistic
 * velocity profiles. Operates in two modes: autonomous (random saccades with fixation
 * pauses) and directed (moving toward a target position provided by an input device).
 */
class EyeMovement
{
public:
  EyeMovement();

  /**
   * @brief Set the target eye position.
   * @param x Target X (-1.0 = far left, +1.0 = far right).
   * @param y Target Y (-1.0 = far up, +1.0 = far down).
   */
  void setTarget(float x, float y);

  /** @brief Set the movement bounds radius (0.0-1.0). */
  void setBounds(float radius) { m_boundsRadius = radius; }

  /** @brief Set the duration range for random movements. */
  void setRandomDuration(uint32_t minMs, uint32_t maxMs);

  /** @brief Enable/disable autonomous random movement mode. */
  void setRandomMode(bool enabled);

  /** @brief Set delay before resuming random movement after losing target (ms). */
  void setSaccadeDelay(uint32_t ms) { m_saccadeDelayAfterTrack = ms; }

  /** @brief Mark that an external target is active (resets idle timer). */
  void setTargetAcquired();

  /** @brief Mark that external target is lost (starts idle delay countdown). */
  void setTargetLost();

  /** @brief Returns true when idle and waiting for saccade delay. */
  bool isIdle() const { return m_idle; }

  /**
   * @brief Advance movement by one tick.
   * @param dt Milliseconds since last update (used for time-based duration).
   * @return true if the eye moved this tick.
   */
  bool update(uint32_t dt);

  /** @brief Current interpolated eye X position. */
  float getX() const { return m_currentX; }

  /** @brief Current interpolated eye Y position. */
  float getY() const { return m_currentY; }

  /** @brief Raw target X (before bounds clamping). */
  float getTargetX() const { return m_targetX; }

  /** @brief Raw target Y (before bounds clamping). */
  float getTargetY() const { return m_targetY; }

  /** @brief Returns true while a movement is in progress. */
  bool isMoving() const { return m_moving; }

  /** @brief Generate and start a new random saccade. */
  void startRandomMove();

  /**
   * @brief Start a directed movement to a specific position.
   * @param x Target X.
   * @param y Target Y.
   * @param durationMs Movement duration in milliseconds.
   */
  void moveTo(float x, float y, uint32_t durationMs);

private:
  void updatePosition();

  float m_currentX = 0.5f; // Current X position (0.5 = center)
  float m_currentY = 0.5f; // Current Y position
  float m_startX = 0.5f;   // Start position for current movement
  float m_startY = 0.5f;
  float m_targetX = 0.5f; // Target position
  float m_targetY = 0.5f;
  float m_boundsRadius = 0.5f; // Movement boundary radius

  bool m_moving = false;    // True during active movement
  bool m_randomMode = true; // True when generating autonomous saccades

  uint32_t m_moveStartTime = 0; // Timestamp when current movement started
  uint32_t m_moveDuration = 0;  // Total planned duration of current movement

  uint32_t m_minDuration = EYE_MOVE_BASE_DURATION_MIN;
  uint32_t m_maxDuration = EYE_MOVE_BASE_DURATION_MAX;

  uint32_t m_saccadeDelayAfterTrack = EYE_MOVE_SACCADE_DELAY;
  uint32_t m_lastTrackTime = 0; // Timestamp of last target status change
  bool m_idle = false;          // True when idle between saccades
};

#endif // EYE_MOVEMENT_H
