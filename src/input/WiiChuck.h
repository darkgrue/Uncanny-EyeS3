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

/**
 * @brief Wii Nunchuck controller input source.
 *
 * Reads joystick position (X/Y) for eye targeting and C/Z buttons for expressions:
 * - Joystick: Direct eye position when active (hasExclusiveControl when tilted > deadzone)
 * - C button (hold): Eyes go wide
 * - Z button (press): Single blink
 * - C + Z simultaneously: Boop expression
 */
class WiiChuckInput : public InputBase {
public:
    /**
     * @brief Construct a WiiChuckInput with the specified I2C address.
     * @param address I2C address of the Nunchuck (default 0x52).
     */
    explicit WiiChuckInput(uint8_t address = 0x52);

    bool begin() override;
    bool update() override;

    float getTargetX() const override { return m_targetX; }
    float getTargetY() const override { return m_targetY; }

    /**
     * @brief Returns true when joystick is tilted beyond the deadzone.
     *
     * While true, the Nunchuck drives eye position directly, suppressing
     * autonomous movement.
     */
    bool hasExclusiveControl() const override { return m_hasStick; }

    /** @brief Returns true on the frame the Z button is first pressed. */
    bool wantsBlink() const override { return m_wantsBlink; }

    /** @brief Returns true on the frame the C button is first pressed. */
    bool wantsBoop() const override { return m_wantsBoop; }

    /** @brief Returns true while the Z button is held down. */
    bool wantsClose() const override { return m_zPressed; }

    /** @brief Returns true while the C button is held down. */
    bool wantsWide() const override { return m_cPressed; }

    /** @brief Clear the blink flag after consuming the event. */
    void clearBlinkFlag() override { m_wantsBlink = false; }

    /** @brief Clear the boop flag after consuming the event. */
    void clearBoopFlag() override { m_wantsBoop = false; }

private:
    /** @brief Read 6 bytes from the Nunchuck and decode joystick/buttons. */
    void readData();

    uint8_t m_address;          /**< I2C address (default 0x52). */
    float m_targetX = 0.0f;      /**< Normalized joystick X (-1.0 to +1.0). */
    float m_targetY = 0.0f;      /**< Normalized joystick Y (-1.0 to +1.0). */
    bool m_hasStick = false;     /**< True when joystick is outside deadzone. */
    bool m_wantsBlink = false;   /**< Edge-triggered blink request. */
    bool m_wantsBoop = false;   /**< Edge-triggered boop request. */
    bool m_cPressed = false;    /**< Current C button state. */
    bool m_zPressed = false;    /**< Current Z button state. */

    uint8_t m_status[6] = {0};  /**< Raw bytes from Nunchuck. */
    bool m_lastZPressed = false; /**< Previous frame Z state (for edge detection). */
    bool m_lastCPressed = false; /**< Previous frame C state (for edge detection). */
};

#endif // WIICHUCK_H