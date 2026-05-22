/**
 * @file InputBase.h
 * @brief Base interface for all input devices that control eye movement and expressions.
 *
 * Input devices (e.g., Wii Chuck, LightSensor) inherit from InputBase to provide
 * eye position, blink commands, and special expressions to the EyeAnimator system.
 */
#ifndef INPUT_BASE_H
#define INPUT_BASE_H

#include "common/EyeState.h"

/**
 * @brief Base class for all input devices.
 *
 * All input sources (controllers, sensors) must implement this interface to provide
 * normalized eye target positions and expressional commands.
 */
class InputBase {
public:
    virtual ~InputBase() = default;

    /**
     * @brief Initialize the input device.
     * @return true if initialization succeeded, false otherwise.
     */
    virtual bool begin() = 0;

    /**
     * @brief Update input state from hardware.
     * @return true if state changed since last update, false otherwise.
     */
    virtual bool update() = 0;

    /**
     * @brief Get current eye target X position (-1.0 = far left, +1.0 = far right).
     */
    virtual float getTargetX() const = 0;

    /**
     * @brief Get current eye target Y position (-1.0 = far up, +1.0 = far down).
     */
    virtual float getTargetY() const = 0;

    /**
     * @brief Check if this input has exclusive control over eye movement.
     *
     * When true, the input drives eye position directly. When false, the system
     * may fall back to autonomous movement (random saccades).
     */
    virtual bool hasExclusiveControl() const = 0;

    /**
     * @brief Check if input requests a blink (edge-triggered, single activation).
     */
    virtual bool wantsBlink() const = 0;

    /**
     * @brief Check if input requests a boop (edge-triggered, single activation).
     */
    virtual bool wantsBoop() const = 0;

    /**
     * @brief Check if input requests eyes to close (hold-while-pressed).
     */
    virtual bool wantsClose() const = 0;

    /**
     * @brief Check if input requests eyes to go wide (hold-while-pressed).
     */
    virtual bool wantsWide() const = 0;

    /** @brief Clear edge-triggered blink flag after processing. */
    virtual void clearBlinkFlag() { }

    /** @brief Clear edge-triggered boop flag after processing. */
    virtual void clearBoopFlag() { }
};

/**
 * @brief Manages multiple input sources and resolves priority.
 *
 * Not currently used by main.cpp but available for future multi-input scenarios
 * where, e.g., a controller and sensor could coexist.
 */
class InputManager {
public:
    void addInput(InputBase* input);
    void update();

    /** @brief Get combined eye X position from primary input. */
    float getEyeX() const;
    /** @brief Get combined eye Y position from primary input. */
    float getEyeY() const;
    /** @brief Returns true if any input requests a blink. */
    bool shouldBlink() const;
    /** @brief Returns true if any input requests a boop. */
    bool shouldBoop() const;

private:
    InputBase* findPrimaryInput() const;

    InputBase* inputs[4];
    uint8_t inputCount = 0;
};

#endif // INPUT_BASE_H