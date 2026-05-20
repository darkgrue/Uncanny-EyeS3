#ifndef AMOLED_DISPLAY_H
#define AMOLED_DISPLAY_H

#include "common/DisplayHAL.h"
#include "Arduino_GFX_Library.h"

class AMOLEDDisplay : public DisplayHAL {
public:
    AMOLEDDisplay();

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

    bool needsFlush() const override { return true; }
    void flush() override;

    void drawString(int16_t x, int16_t y, const char* str, uint16_t color = 0xFFFF) override;

private:
    Arduino_CO5300* m_gfx = nullptr;
    int m_width = 466;
    int m_height = 466;
    bool m_initialized = false;
};

#endif // AMOLED_DISPLAY_H