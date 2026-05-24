/**
 * @file WiiChuck.h
 * @brief Nintendo Wii Nunchuck controller input implementation.
 *
 * Provides eye position from the analog joystick and expression commands from the
 * C and Z buttons. Joystick movement takes exclusive control while held, otherwise
 * the system uses autonomous eye movement.
 */
#ifndef WIICHUCK_H
#define WIICHUCK_H

#include "InputBase.h"
#include <Wire.h>

/** @brief Identifies the type of Wii Extension Controller attached via I2C. */
typedef enum _controllertype {
	UnknownChuck,              /**< Controller type could not be determined. */
	NUNCHUCK,                  /**< Standard Wii Nunchuck. */
	WIICLASSIC,                /**< Wii Classic Controller. */
	GuitarHeroController,      /**< Guitar Hero Controller. */
	GuitarHeroWorldTourDrums,  /**< Guitar Hero World Tour drum kit. */
	DrumController,            /**< Taiko no Tatsujin TaTaCon drum controller. */
	DrawsomeTablet,            /**< Drawsome drawing tablet. */
	Turntable                  /**< DJ Hero turntable controller. */
} ControllerType;

/**
 * @brief Wii Nunchuck controller input source.
 *
 * Reads joystick position (X/Y) for eye targeting and C/Z buttons for expressions:
 * - Joystick: Direct eye position when active (hasExclusiveControl when tilted > deadzone)
 * - Z button (hold): Eyes close; releases when Z is released
 * - C button (hold): Eyes go wide; releases when C is released
 * - C + Z simultaneously: Boop expression (C wins wide vs close priority)
 */
class WiiChuckInput : public InputBase
{
public:
  /**
   * @brief Construct a WiiChuckInput with the specified I2C address and bus.
   * @param address I2C address of the Nunchuck (default 0x52).
   * @param wire    I2C bus instance to use (default Wire).
   */
  explicit WiiChuckInput(uint8_t address = 0x52, TwoWire &wire = Wire);

  /**
   * @brief Initialize I2C bus and send the Nunchuck handshake sequence.
   * @return true if the controller responded and was identified as a NUNCHUCK.
   */
  bool begin() override;

  /**
   * @brief Poll the Nunchuck at ~60 Hz and update internal state.
   * @return true on each frame that fresh data was read from the controller.
   */
  bool update() override;

  /**
   * @brief Returns the normalized joystick X position.
   * @return Value in [-1.0, +1.0]; 0.0 when inside the deadzone.
   */
  float getTargetX() const override { return m_targetX; }

  /**
   * @brief Returns the normalized joystick Y position.
   * @return Value in [-1.0, +1.0]; 0.0 when inside the deadzone.
   */
  float getTargetY() const override { return m_targetY; }

  /**
   * @brief Returns true when joystick is tilted beyond the deadzone.
   *
   * While true, the Nunchuck drives eye position directly, suppressing
   * autonomous movement.
   */
  bool hasExclusiveControl() const override { return m_hasStick; }

  /** @brief Always false — blink is automatic; no button triggers it. */
  bool wantsBlink() const override { return false; }

  /** @brief Returns true on the frame the C+Z chord first fires. */
  bool wantsBoop() const override { return m_wantsBoop; }

  /** @brief Returns true while Z is held (eyes closed, released when Z released). */
  bool wantsClose() const override { return m_zPressed; }

  /** @brief Returns true while C is held (eyes wide, released when C released). */
  bool wantsWide() const override { return m_cPressed; }

  /** @brief Clear the blink flag after consuming the event. */
  void clearBlinkFlag() override { m_wantsBlink = false; }

  /** @brief Clear the boop flag after consuming the event. */
  void clearBoopFlag() override { m_wantsBoop = false; }

private:
  /** @brief Probe the controller type via the 0xFA identification register. */
  ControllerType identifyController();

  /** @brief Read 6 bytes from the Nunchuck and decode joystick/buttons. */
  void readData();

  TwoWire &m_wire;           /**< I2C bus instance. */
  uint8_t m_address;         /**< I2C address (default 0x52). */
  float m_targetX = 0.0f;    /**< Normalized joystick X (-1.0 to +1.0). */
  float m_targetY = 0.0f;    /**< Normalized joystick Y (-1.0 to +1.0). */
  bool m_hasStick = false;   /**< True when joystick is outside deadzone. */
  bool m_wantsBlink = false; /**< Edge-triggered blink request. */
  bool m_wantsBoop = false;  /**< Edge-triggered boop request. */
  bool m_cPressed = false;   /**< Current C button state. */
  bool m_zPressed = false;   /**< Current Z button state. */

  uint8_t m_status[6] = {0};   /**< Raw bytes from Nunchuck. */
  bool m_lastZPressed = false; /**< Previous frame Z state (for edge detection). */
  bool m_lastCPressed = false; /**< Previous frame C state (for edge detection). */
  uint32_t m_lastRead = 0;     /**< micros() timestamp of last I2C poll. */
};

#endif // WIICHUCK_H