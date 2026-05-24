/**
 * @file BlinkFSM.h
 * @brief Finite state machine for realistic eyelid blink animation.
 *
 * Implements the characteristic human blink pattern: quick closing phase,
 * brief pause at full closure, then slower opening. Handles both automatic
 * timed blinks and triggered/expression blinks (close, wide).
 */
#ifndef BLINK_FSM_H
#define BLINK_FSM_H

#include "common/EyeState.h"

/**
 * @brief Manages eyelid blink animation states and timing.
 *
 * Operates as a state machine cycling through NOBLINK → ENBLINK → DEBLINK.
 * Tracks both automatic periodic blinks (2-8 second intervals) and
 * triggered expressions (blink, close, wide) with pause-at-closure behavior.
 */
class BlinkFSM
{
public:
  BlinkFSM();

  /** @brief Reset to idle state and randomize next automatic blink timer. */
  void reset();

  /**
   * @brief Trigger a single reflexive blink.
   *
   * Resets the next automatic blink timer to a multiple of the blink
   * duration, preventing rapid double-blinks from overlapping.
   */
  void trigger();

  /** @brief Force eyelids fully closed (hold expression). */
  void close();

  /** @brief Force eyelids fully open (surprise expression). */
  void wide();

  /**
   * @brief Return to normal automatic blinking.
   */
  void normal();

  /**
   * @brief Force eyelids to a specific gap value (wide expression).
   * @param gap Target eyelid gap (0.0 = closed, 1.0 = fully open).
   */
  void wideTo(float gap);

  /**
   * @brief Set the target gap for normal state.
   * @param gap Normal eyelid gap (0.0 = closed, 1.0 = fully open).
   */
  void setNormalGap(float gap);

  /**
   * @brief Advance the state machine.
   * @param elapsed Microseconds since the last update (use micros()).
   * @return true if state or factor changed.
   */
  bool update(uint32_t elapsed);

  /** @brief Current eyelid closure factor (0.0 = open, 1.0 = closed). */
  float getFactor() const { return m_factor; }

  /** @brief Current blink state. */
  EyeBlinkState getState() const { return m_state; }

  /** @brief True when eyelids are in a forced expression (close/wide). */
  bool isForced() const { return m_forced; }

private:
  EyeBlinkState m_state = NOBLINK; // Current state in the blink cycle
  float m_factor = 0.0f;           // Current closure factor (0.0=open, 1.0=closed)
  bool m_forced = false;           // True when a forced expression is active or animating
  float m_targetGap = 0.0f;        // Resting closure value set by setNormalGap()

  float m_forceStart = 0.0f;  // Closure factor at the start of a forced transition
  float m_forceTarget = 0.0f; // Closure factor the forced transition is moving toward

  uint32_t m_stateStart = 0;    // Timestamp (micros) when current state began
  uint32_t m_blinkDuration = 0; // Duration of the current phase in microseconds
  uint32_t m_nextBlinkTime = 0; // Interval until the next automatic blink (microseconds)
};

#endif // BLINK_FSM_H