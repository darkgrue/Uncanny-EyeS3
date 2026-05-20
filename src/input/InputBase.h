#ifndef INPUT_BASE_H
#define INPUT_BASE_H

#include "common/EyeState.h"

// Base class for all input devices
class InputBase {
public:
    virtual ~InputBase() = default;
    
    // Initialize the input device
    virtual bool begin() = 0;
    
    // Update input state, returns true if state changed
    virtual bool update() = 0;
    
    // Get current eye target position (-1.0 to +1.0)
    virtual float getTargetX() const = 0;
    virtual float getTargetY() const = 0;
    
    // Check if input wants exclusive control
    virtual bool hasExclusiveControl() const = 0;
    
    // Check for special commands
    virtual bool wantsBlink() const = 0;  // Edge-triggered
    virtual bool wantsBoop() const = 0;  // Edge-triggered
    virtual bool wantsClose() const = 0; // Hold command - eyes close while pressed
    virtual bool wantsWide() const = 0;   // Hold command - eyes wide while pressed

    // Clear edge-triggered flags (called after processing)
    virtual void clearBlinkFlag() { }
    virtual void clearBoopFlag() { }
};

// Input manager that combines multiple inputs
class InputManager {
public:
    void addInput(InputBase* input);
    void update();
    
    // Get combined/priority input values
    float getEyeX() const;
    float getEyeY() const;
    bool shouldBlink() const;
    bool shouldBoop() const;
    
private:
    InputBase* findPrimaryInput() const;
    
    InputBase* inputs[4];
    uint8_t inputCount = 0;
};

#endif // INPUT_BASE_H