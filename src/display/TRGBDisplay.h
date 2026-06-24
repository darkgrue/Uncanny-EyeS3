/**
 * @file TRGBDisplay.h
 * @brief Display hardware abstraction for LilyGo T-RGB (ST7701S).
 *
 * Implements DisplayHAL for the ST7701S RGB panel using DPI (24-bit parallel)
 * at 80MHz. Most async methods are stubbed since the RGB panel operates
 * synchronously (writes complete before the function returns). Includes
 * directTransfer() implementation for bulk frame updates.
 */
#ifndef TRGB_DISPLAY_H
#define TRGB_DISPLAY_H

#include "common/DisplayHAL.h"
#include <Arduino_GFX.h>
#include <Wire.h>

// Forward declarations - full includes are in TRGBDisplay.cpp
// to avoid GFX library dependency for environments that don't use this display
class Arduino_XL9535SWSPI;
class Arduino_ESP32RGBPanel;
class Arduino_RGB_Display;

/**
 * @brief Display driver for the LilyGo T-RGB (ST7701S RGB panel).
 *
 * Uses 24-bit DPI (parallel) at 80MHz. Async transfer methods are stubbed
 * because the panel writes are synchronous. The directTransfer() path
 * provides the fastest bulk update mechanism available.
 */
class TRGBDisplay : public DisplayHAL
{
public:
  TRGBDisplay();

  bool begin() override;

  int getWidth() const override { return 480; }
  int getHeight() const override { return 480; }

  void setAddrWindow(int x, int y, int w, int h) override;
  void pushPixels(const uint16_t *pixels, size_t count) override;
  void pushPixels(uint16_t color, size_t count) override;
  void fillRect(int x, int y, int w, int h, uint16_t color) override;
  void clear(uint16_t color = 0x0000) override;
  void setRotation(uint8_t rotation) override;
  void setBacklight(bool on) override;
  void setBrightness(uint8_t level) override;

  void startWrite() override;
  void endWrite() override;

  bool needsFlush() const override { return true; }
  void flush() override;

  void drawString(int16_t x, int16_t y, const char *str, uint16_t color = 0xFFFF) override;
  void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override;
  void drawSubRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h,
                        int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH) override;
  bool drawRGBBitmapAsync(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override;
  bool isDMATransferBusy() override;

  bool beginAsyncTransfer() override { return true; }
  bool writePixelsAsync(uint16_t *, size_t) override { return true; }
  bool endAsyncTransfer() override { return true; }
  bool isAsyncTransferComplete() override { return true; }
  bool waitForAsyncTransfer(uint32_t timeoutMs) override
  {
    (void)timeoutMs;
    return true;
  }

  bool beginDisplayTransfer() override { return true; }
  void endDisplayTransfer() override {}
  bool isTransferComplete() override { return true; }

  void directTransfer(uint16_t *buffer, int destX, int destY,
                      int srcX, int srcY, int srcW, int srcH) override;

private:
  Arduino_DataBus *bus = nullptr;
  Arduino_ESP32RGBPanel *rgbpanel = nullptr;
  Arduino_GFX *gfx = nullptr;
  int m_width = 480;
  int m_height = 480;
  bool m_initialized = false;
  bool m_rotation180 = false;
};

#endif // TRGB_DISPLAY_H