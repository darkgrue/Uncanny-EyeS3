#ifndef TRGB_DISPLAY_H
#define TRGB_DISPLAY_H

#include "DisplayHAL.h"
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>

class TRGBDisplay : public DisplayHAL {
public:
    TRGBDisplay();

    bool begin() override;

    int getWidth() const override { return 480; }
    int getHeight() const override { return 480; }

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

private:
    void resetDisplay();
    void initST7701S();
    void initBus();
    void writeCommand(uint8_t cmd);
    void writeData(const uint8_t* data, size_t len);

    esp_lcd_panel_handle_t m_panelDrv;
    uint16_t* m_framebuffer;
    int m_width = 480;
    int m_height = 480;
    bool m_backlightOn = false;
    bool m_initialized = false;
};

#endif // TRGB_DISPLAY_H