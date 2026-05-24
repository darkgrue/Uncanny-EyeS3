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
  float m_factor = 0.0f;           // Current closure factor (0.0-1.0)
  bool m_forced = false;           // True when in forced expression
  float m_targetGap = 0.0f;        // Target gap for normal state

  uint32_t m_stateStart = 0;    // Timestamp when current state started
  uint32_t m_blinkDuration = 0; // Duration of the closing or opening phase
  uint32_t m_nextBlinkTime = 0; // Timestamp for next automatic blink
};

#endif // BLINK_FSM_H