#ifndef TDISPLAY_S3_AMOLED_H
#define TDISPLAY_S3_AMOLED_H

#include "DisplayHAL.h"
#include "TFT_eSPI.h"

// Display implementation for Lilygo T-Display S3 AMOLED
// 1.43" or 1.75" AMOLED display, 466x466 or similar resolution
class TDisplayS3AMOLED : public DisplayHAL {
public:
    TDisplayS3AMOLED();
    
    bool begin() override;
    
    int getWidth() const override { return m_width; }
    int getHeight() const override { return m_height; }
    
    void setAddrWindow(int x, int y, int w, int h) override;
    void pushPixels(const uint16_t* pixels, size_t count) override;
    void pushPixels(uint16_t color, size_t count) override;
    void fillRect(int x, int y, int w, int h, uint16_t color) override;
    void clear(uint16_t color = 0x0000) override;
    void setRotation(uint8_t rotation) override;
    void setBacklight(bool on) override;
    void setBrightness(uint8_t level) override;
    
    bool needsFlush() const override { return m_dirty; }
    void flush() override;
    
    // Access to underlying TFT
    TFT_eSPI& getTFT() { return m_tft; }
    
private:
    TFT_eSPI m_tft;
    int m_width = 466;
    int m_height = 466;
    bool m_dirty = false;
    bool m_backlightOn = false;
};

#endif // TDISPLAY_S3_AMOLED_H
