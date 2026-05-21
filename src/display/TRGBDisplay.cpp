#include "TRGBDisplay.h"

namespace {
constexpr int GFX_BL = 46;
constexpr int GFX_SDA = 8;
constexpr int GFX_SCL = 48;
constexpr int GFX_PWD = 2;
constexpr int GFX_CS = 3;
constexpr int GFX_SCK = 5;
constexpr int GFX_MOSI = 4;
}

TRGBDisplay::TRGBDisplay()
    : bus(nullptr)
    , rgbpanel(nullptr)
    , gfx(nullptr)
    , m_initialized(false) {
}

bool TRGBDisplay::begin() {
    if (m_initialized) {
        return true;
    }

    Wire.begin(GFX_SDA, GFX_SCL, 400000);

    bus = new Arduino_XL9535SWSPI(GFX_SDA, GFX_SCL, GFX_PWD, GFX_CS, GFX_SCK, GFX_MOSI);

    rgbpanel = new Arduino_ESP32RGBPanel(
        45, 41, 47, 42,
        21, 18, 17, 16, 15,
        14, 13, 12, 11, 10, 9,
        7, 6, 5, 3, 2,
        1, 50, 1, 30,
        1, 20, 1, 30,
        1);

    gfx = new Arduino_RGB_Display(
        480, 480, rgbpanel, 0, true,
        bus, GFX_NOT_DEFINED, st7701_type4_init_operations, sizeof(st7701_type4_init_operations));

    if (!gfx->begin()) {
        Serial.println("TRGB display begin() failed!");
        return false;
    }

    gfx->fillScreen(0x0000);
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
    m_initialized = true;

    return true;
}

void TRGBDisplay::setAddrWindow(int x, int y, int w, int h) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

void TRGBDisplay::pushPixels(const uint16_t* pixels, size_t count) {
    if (gfx) {
        uint16_t* fb = gfx->getFramebuffer();
        if (fb) {
            memcpy(fb, pixels, count * sizeof(uint16_t));
        }
    }
}

void TRGBDisplay::pushPixels(uint16_t color, size_t count) {
    if (gfx) {
        gfx->fillScreen(color);
    }
}

void TRGBDisplay::fillRect(int x, int y, int w, int h, uint16_t color) {
    if (gfx) {
        gfx->fillRect(x, y, w, h, color);
    }
}

void TRGBDisplay::clear(uint16_t color) {
    if (gfx) {
        gfx->fillScreen(color);
    }
}

void TRGBDisplay::setRotation(uint8_t rotation) {
    if (gfx) {
        gfx->setRotation(rotation);
    }
}

void TRGBDisplay::setBacklight(bool on) {
    digitalWrite(GFX_BL, on ? HIGH : LOW);
}

void TRGBDisplay::setBrightness(uint8_t level) {
    (void)level;
}

void TRGBDisplay::startWrite() {
    // RGB panel doesn't need explicit bus management
}

void TRGBDisplay::endWrite() {
    // RGB panel doesn't need explicit bus management
}

void TRGBDisplay::flush() {
    // RGB display auto-flushes via its own mechanism
}

void TRGBDisplay::drawString(int16_t x, int16_t y, const char* str, uint16_t color) {
    if (gfx) {
        gfx->setCursor(x, y);
        gfx->setTextColor(color);
        gfx->print(str);
    }
}

void TRGBDisplay::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) {
    if (gfx) {
        gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
    }
}

void TRGBDisplay::drawSubRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h,
                                    int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH) {
    // RGB panel doesn't support partial updates efficiently
    // Fall back to full bitmap draw
    if (gfx) {
        gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
    }
}

bool TRGBDisplay::drawRGBBitmapAsync(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) {
    // RGB panel uses different mechanism - for now use sync version
    if (gfx) {
        gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
    }
    return true;  // Completed
}

bool TRGBDisplay::isDMATransferBusy() {
    // RGB panel handles its own DMA - for now assume not busy
    return false;
}