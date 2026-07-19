/**
 * @file AMOLEDDisplay.h
 * @brief Display hardware abstraction for LilyGo T-Display S3 AMOLED.
 *
 * Implements DisplayHAL for the CO5300 display driver using QSPI at up to 80MHz.
 * Supports async DMA transfers for render/display overlap, direct bulk transfers
 * bypassing the GFX library, and software-sync methods to coordinate rendering
 * and display updates on separate tasks.
 */
#ifndef AMOLED_DISPLAY_H
#define AMOLED_DISPLAY_H

#include "common/DisplayHAL.h"
#include <Arduino_GFX.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Forward declarations - full includes are in AMOLEDDisplay.cpp
class Arduino_ESP32QSPI;
class Arduino_CO5300;

/**
 * @brief Display driver for the LilyGo T-Display S3 AMOLED (CO5300).
 *
 * Uses QSPI at 80MHz for maximum pixel throughput. Provides both the standard
 * GFX-based interface and a directTransfer() path that bypasses per-pixel
 * library overhead for bulk frame updates.
 */
class AMOLEDDisplay : public DisplayHAL
{
public:
  AMOLEDDisplay();

  bool begin() override;

  int getWidth() const override { return m_width; }
  int getHeight() const override { return m_height; }

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

  /** @brief Non-blocking poll for DMA completion. */
  bool waitForTransferComplete(uint32_t timeoutMs);

  bool waitForAsyncTransfer(uint32_t timeoutMs) override;

  bool beginDisplayTransfer() override;
  void endDisplayTransfer() override;
  bool isTransferComplete() override;

  /**
   * @brief Direct bulk transfer from a PSRAM buffer via QSPI.
   *
   * Bypasses Arduino_GFX entirely for maximum throughput. Copies directly
   * from the source buffer to the display using QSPI DMA.
   */
  void directTransfer(uint16_t *buffer, int destX, int destY,
                      int srcX, int srcY, int srcW, int srcH) override;

  /** @brief directTransfer() sends raw bytes over QSPI — needs big-endian pixels. */
  bool needsByteSwappedPixels() const override { return true; }

private:
  Arduino_CO5300 *m_gfx = nullptr;
  Arduino_ESP32QSPI *m_qspiBus = nullptr;
  int m_width = 466;
  int m_height = 466;
  bool m_initialized = false;
  bool m_transferPending = false;

  /**
   * @brief Guards the QSPI bus against concurrent access.
   *
   * directTransfer() runs on the async xfer task (Core 0, ~120 Hz) while
   * setRotation() can be called from the Arduino loop() on a serial command
   * (also Core 0, lower priority). Without this, the two can interleave their
   * beginWrite()/endWrite() sequences on the same bus handle, desyncing the
   * ESP-IDF SPI driver's chip-select tracking and tripping its
   * "host->cur_cs == handle->id" assertion.
   */
  SemaphoreHandle_t m_busMutex = nullptr;
};

#endif // AMOLED_DISPLAY_H