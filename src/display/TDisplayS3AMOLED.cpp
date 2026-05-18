#include "TDisplayS3AMOLED.h"
#include <Arduino.h>

TDisplayS3AMOLED::TDisplayS3AMOLED() 
    : m_width(466)
    , m_height(466)
    , m_dirty(false)
    , m_backlightOn(false) {
}

bool TDisplayS3AMOLED::begin() {
    m_tft.init();
    m_tft.setRotation(3);  // Portrait mode for watch-style display
    
    m_width = m_tft.width();
    m_height = m_tft.height();
    
    clear(0x0000);
    setBacklight(true);
    setBrightness(255);
    
    return true;
}

void TDisplayS3AMOLED::setAddrWindow(int x, int y, int w, int h) {
    m_tft.setAddrWindow(x, y, x + w - 1, y + h - 1);
}

void TDisplayS3AMOLED::pushPixels(const uint16_t* pixels, size_t count) {
    m_tft.pushPixels(pixels, count);
}

void TDisplayS3AMOLED::pushPixels(uint16_t color, size_t count) {
    m_tft.pushColor(color, count);
}

void TDisplayS3AMOLED::fillRect(int x, int y, int w, int h, uint16_t color) {
    m_tft.fillRect(x, y, w, h, color);
}

void TDisplayS3AMOLED::clear(uint16_t color) {
    m_tft.fillScreen(color);
}

void TDisplayS3AMOLED::setRotation(uint8_t rotation) {
    m_tft.setRotation(rotation);
}

void TDisplayS3AMOLED::setBacklight(bool on) {
    m_backlightOn = on;
    // T-Display S3 AMOLED uses GPIO for backlight control
    // TODO: Set actual backlight pin
}

void TDisplayS3AMOLED::setBrightness(uint8_t level) {
    // If PWM control is available, use it
    // TODO: Configure PWM channel for backlight
}

void TDisplayS3AMOLED::flush() {
    // For framebuffer-based displays, this would swap buffers
    m_dirty = false;
}
