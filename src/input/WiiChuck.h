#ifndef WIICHUCK_H
#define WIICHUCK_H

#include "InputBase.h"
#include <Wire.h>

// Wii Chuck controller input implementation
class WiiChuckInput : public InputBase {
public:
    WiiChuckInput(uint8_t address = 0x52);
    
    bool begin() override;
    bool update() override;
    
    float getTargetX() const override { return m_targetX; }
    float getTargetY() const override { return m_targetY; }
    bool hasExclusiveControl() const override { return m_hasStick; }
    
    // Edge-triggered commands
    bool wantsBlink() const override { return m_wantsBlink; }
    bool wantsBoop() const override { return m_wantsBoop; }
    
    // Hold commands (C = wide, Z = close)
    bool wantsClose() const override { return m_zPressed; }  // Z button held
    bool wantsWide() const override { return m_cPressed; }   // C button held
    
    // Clear transient flags after they are consumed
    void clearBlinkFlag() { m_wantsBlink = false; }
    void clearBoopFlag() { m_wantsBoop = false; }

private:
    void readData();
    
    uint8_t m_address;
    float   m_targetX = 0.0f;
    float   m_targetY = 0.0f;
    bool    m_hasStick = false;
    bool    m_wantsBlink = false;
    bool    m_wantsBoop = false;
    bool    m_cPressed = false;
    bool    m_zPressed = false;
    
    // Cached values from controller
    uint8_t m_status[6] = {0};
    bool    m_lastZPressed = false;
    bool    m_lastCPressed = false;
};

#endif // WIICHUCK_H