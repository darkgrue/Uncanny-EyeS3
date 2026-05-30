/**
 * @file EyeMovement.h
 * @brief Saccadic eye movement generator with lognormal distribution and centering bias.
 *
 * Human eye movements naturally follow a lognormal distribution — most are small
 * microsaccades (~0.5-2°) with occasional larger saccades. This module generates
 * realistic autonomous eye movement using that distribution, applies a centering
 * bias to pull the eye back toward center after peripheral movements, and uses
 * sigmoid easing to approximate the characteristic velocity profile of real saccades.
 *
 * Tuning guide: Each parameter section includes a description of its effect and
 * recommended range. Increasing a parameter moves behavior in that direction.
 *
 * ============================================================================
 * SACCADE / EYE MOVEMENT PARAMETERS
 * ============================================================================
 *
 * Lognormal Distribution Parameters
 * --------------------------------
 * These control the amplitude (size) distribution of random eye movements.
 * Lognormal is used because real saccades follow this distribution — most
 * movements are small, with a long tail of larger movements.
 *
 * EYE_MOVE_LOGNORMAL_SIGMA: Spread/shape of the amplitude distribution.
 *   Higher = more variation, more large saccades. Range: 0.2-0.6. Default: 0.35.
 *
 * EYE_MOVE_LOGNORMAL_OFFSET: Shifts the distribution (negative = smaller avg).
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
 * EYE_MOVE_BOUNDS_RADIUS: Movement boundary radius (fraction of display half-width).
 *   Controls how far from center the pupil can travel. Range: 0.3-0.8. Default: 0.6.
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
 * EYE_MOVE_FIXATION_PAUSE_MIN/MAX: Pause between random saccades (ms). Default: 12000-50000.
 *   Long pauses with wide random spread create the natural "dwell then dart" rhythm.
 *   Increase MIN for a lazier gaze; decrease MAX to keep the eye more active.
 *
 * EYE_MOVE_SACCADE_DELAY: Delay before resuming autonomous saccades after an external
 *   target (joystick or face) is lost (ms). Default: 4000.
 *
 * Saccade Easing Parameters
 * -------------------------
 * Real saccades have rapid acceleration/deceleration; sigmoid easing approximates this.
 *
 * EYE_MOVE_EASING_STEEPNESS: Sharpness of the easing transition. Range: 1.0-5.0. Default: 3.0.
 *
 * Joystick Smooth-Follow Parameters
 * ----------------------------------
 * Applied per frame at ~120 FPS via adaptive exponential smoothing.
 * Alpha = BASE + distance * DIST, clamped to [0, 1].
 *   JOYSTICK_BASE_ALPHA: minimum blend per frame (small-movement smoothness).
 *   JOYSTICK_DIST_ALPHA: extra blend per unit of distance (large-move responsiveness).
 * At dist=0.0: alpha=0.12 → ~140 ms settling; at dist=0.6: alpha=0.30 → ~80 ms.
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
 * EYELID_NORMAL_CLOSURE_DEFAULT: Resting eyelid coverage (0.0=open, 1.0=closed). Default: 0.15.
 * EYELID_WIDE_CLOSURE_DEFAULT: Wide/surprised eyelid coverage. Default: 0.0.
 * EYELID_SMOOTHING: Smoothing factor for eyelid position changes. Default: 0.1.
 * BLINK_USE_SMOOTHSTEP: Enable smoothstep easing (0/1). Default: 1.
 *
 * Forced Expression Animation Durations
 * --------------------------------------
 * When the Z or C button is pressed or released the eyelid animates smoothly
 * to the target position using smoothstep easing instead of snapping instantly.
 *
 * EYELID_CLOSE_DURATION: ms to animate to fully closed on Z-button hold.
 *   Faster feels reflexive; slower feels deliberate. Range: 80-300. Default: 150.
 *
 * EYELID_WIDE_DURATION: ms to animate to wide-open on C-button hold.
 *   Should feel like mild surprise — quick but not snappy. Range: 80-250. Default: 130.
 *
 * EYELID_NORMAL_DURATION: ms to animate back to the resting gap on button release.
 *   A slower return feels more relaxed and natural. Range: 150-500. Default: 230.
 *
 * PUPIL_WIDE_DURATION: ms to constrict pupil on C-button hold. Default: 100.
 * PUPIL_RELEASE_DURATION: ms to return pupil to normal on C-button release. Default: 150.
 *
 * ============================================================================
 * PUPIL / IRIS AUTONOMOUS ANIMATION (HIPPUS) PARAMETERS
 * ============================================================================
 * Human pupils exhibit slow spontaneous oscillation at rest (hippus). This models
 * that behavior: the pupil drifts toward a new target over a slow transition, holds
 * briefly, then drifts again. All timing values are in milliseconds.
 *
 * IRIS_AMPLITUDE_SCALE: Normal-distribution sigma for the drift amplitude (normalized
 *   0-1 fraction of the pupil range). Smaller = subtler oscillation. Default: 0.07.
 *
 * IRIS_AMPLITUDE_MAX: Hard clip on drift amplitude (normalized). Default: 0.15.
 *
 * IRIS_HOLD_MIN/MAX: Random hold time at each target before the next drift (ms).
 *   Default: 5000-13000 (5-13 s). Increase to make the pupil drift less frequently.
 *
 * IRIS_TRANSITION_MIN/MAX: Random duration of each drift transition (ms).
 *   Default: 1500-3500. Slower transitions look more organic.
 *
 * ============================================================================
 * BOOP EXPRESSION PARAMETERS
 * ============================================================================
 * Boop: brief squint + dilated pupils triggered by a button or external event.
 *
 * BOOP_DURATION_MS: How long the boop expression holds (ms). Default: 1500.
 * BOOP_SQUINT_FACTOR: Eyelid closure during boop (0.0=open, 1.0=closed). Default: 0.6.
 */
#ifndef EYE_MOVEMENT_H
#define EYE_MOVEMENT_H

#include "common/EyeState.h"

// ============================================================================
// SACCADE / EYE MOVEMENT PARAMETERS
// ============================================================================

// Lognormal amplitude distribution
#define EYE_MOVE_LOGNORMAL_SIGMA 0.35f
#define EYE_MOVE_LOGNORMAL_OFFSET -0.2f
#define EYE_MOVE_AMPLITUDE_SCALE 0.5f
#define EYE_MOVE_MIN_AMPLITUDE 0.01f
#define EYE_MOVE_MAX_AMPLITUDE_SCALE 0.8f

// Movement boundary (fraction of display half-width)
#define EYE_MOVE_BOUNDS_RADIUS 0.6f

// Centering bias
#define EYE_MOVE_CENTER_BIAS_FACTOR 0.6f
#define EYE_MOVE_CENTER_BIAS_MAX 0.5f

// Per-saccade duration (ms)
#define EYE_MOVE_BASE_DURATION_MIN 200
#define EYE_MOVE_BASE_DURATION_MAX 350
#define EYE_MOVE_DURATION_DISTANCE_SCALE 150
#define EYE_MOVE_DURATION_MIN 150
#define EYE_MOVE_DURATION_MAX 500

// Fixation pause between saccades (ms)
#define EYE_MOVE_FIXATION_PAUSE_MIN 3000
#define EYE_MOVE_FIXATION_PAUSE_MAX 12000

// Delay before resuming saccades after losing an external target (ms)
#define EYE_MOVE_SACCADE_DELAY 4000

// Sigmoid easing steepness for saccade velocity profile (range 1.0-5.0)
#define EYE_MOVE_EASING_STEEPNESS 3.0f

// Joystick smooth-follow (adaptive exponential smoothing per frame at ~120 FPS)
#define JOYSTICK_BASE_ALPHA 0.12f
#define JOYSTICK_DIST_ALPHA 0.30f

// ============================================================================
// EYELID / BLINK PARAMETERS
// ============================================================================

// Blink phase durations (ms)
#define BLINK_DURATION_CLOSE_MIN 60
#define BLINK_DURATION_CLOSE_MAX 100
#define BLINK_DURATION_OPEN_MIN 120
#define BLINK_DURATION_OPEN_MAX 200
#define BLINK_PAUSE_AT_CLOSURE 20

// Automatic blink interval (ms) and burst / post-saccade probabilities
#define BLINK_INTERVAL_MIN 2000
#define BLINK_INTERVAL_MAX 8000
#define BLINK_PROBABILITY_BURST 0.15f
#define BLINK_CHANCE_AFTER_SACCADE 0.08f

// Eyelid tracking and shape
#define EYELID_UPPER_TRACK_STRENGTH 0.3f
#define EYELID_LOWER_TRACK_STRENGTH 0.15f
#define EYELID_SQUINT_FACTOR 0.7f
#define EYELID_NORMAL_CLOSURE_DEFAULT 0.15f // Resting coverage (0.0=open, 1.0=closed)
#define EYELID_WIDE_CLOSURE_DEFAULT 0.0f    // Wide/surprised coverage
#define EYELID_SMOOTHING 0.1f
#define BLINK_USE_SMOOTHSTEP 1

// Forced expression animation durations (ms)
#define EYELID_CLOSE_DURATION 150  // Z-button hold → fully closed
#define EYELID_WIDE_DURATION 130   // C-button hold → wide open
#define EYELID_NORMAL_DURATION 230 // Button release → resting gap
#define PUPIL_WIDE_DURATION 100    // C-button hold → constricted pupil
#define PUPIL_RELEASE_DURATION 150 // C-button release → normal pupil

// ============================================================================
// PUPIL / IRIS AUTONOMOUS ANIMATION (HIPPUS) PARAMETERS
// ============================================================================

// Drift amplitude: normal-distribution sigma and hard clip (normalized 0-1)
#define IRIS_AMPLITUDE_SCALE 0.07f
#define IRIS_AMPLITUDE_MAX 0.15f

// Pupil smoothAlpha for light-sensor-driven transitions (exponential smoothing coefficient)
// Lower = slower/more gradual, Higher = faster/more snappy (0.01 to 0.2 typical)
#define PUPIL_SMOOTH_ALPHA 0.5f

// Hold time at each drift target (ms)
#define IRIS_HOLD_MIN 5000
#define IRIS_HOLD_MAX 13000

// Transition duration for each drift (ms)
#define IRIS_TRANSITION_MIN 1500
#define IRIS_TRANSITION_MAX 3500

// ============================================================================
// BOOP EXPRESSION PARAMETERS
// ============================================================================

#define BOOP_DURATION_MS 1500   // How long the boop expression holds (ms)
#define BOOP_SQUINT_FACTOR 0.6f // Eyelid closure during boop (0.0=open, 1.0=closed)
#define BOOP_PUPIL_DURATION 120 // Transition duration for pupil dilate/constrict during boop (ms)

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

  /** @brief Override the fixation pause for the next completed move only (ms). */
  void setPostMoveIdle(uint32_t ms) { m_postMoveIdleMs = ms; }

  /** @brief Mark that external target is lost (starts idle delay countdown). */
  void setTargetLost();

  /** @brief Returns true when idle and waiting for saccade delay. */
  bool isIdle() const { return m_idle; }

  /**
   * @brief Advance movement by one tick.
   * @return true if the eye moved this tick.
   */
  bool update();

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

  /**
   * @brief Directly set the current eye position, cancelling any active movement.
   *
   * Used by joystick smooth-follow to bypass the saccade system: the caller
   * maintains its own smoothed position and writes it here each frame.
   * Bounds-clamps the input the same way setTarget() does.
   */
  void setCurrentPosition(float x, float y);

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

  float m_currentX = 0.0f; // Current X position (0.0 = center)
  float m_currentY = 0.0f; // Current Y position
  float m_startX = 0.0f;   // Start position for current movement
  float m_startY = 0.0f;
  float m_targetX = 0.0f; // Target position
  float m_targetY = 0.0f;
  float m_boundsRadius = EYE_MOVE_BOUNDS_RADIUS; // Movement boundary radius

  bool m_moving = false;    // True during active movement
  bool m_randomMode = true; // True when generating autonomous saccades

  uint32_t m_moveStartTime = 0; // Timestamp when current movement started
  uint32_t m_moveDuration = 0;  // Total planned duration of current movement

  uint32_t m_minDuration = EYE_MOVE_BASE_DURATION_MIN;
  uint32_t m_maxDuration = EYE_MOVE_BASE_DURATION_MAX;

  uint32_t m_saccadeDelayAfterTrack = EYE_MOVE_SACCADE_DELAY;
  uint32_t m_postMoveIdleMs = 0; // One-shot override for the next fixation pause
  uint32_t m_lastTrackTime = 0;  // Timestamp of last target status change
  bool m_idle = false;           // True when idle between saccades
};

#endif // EYE_MOVEMENT_H
