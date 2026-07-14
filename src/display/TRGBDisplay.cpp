/**
 * @file TRGBDisplay.cpp
 * @brief Display driver implementation for the ST7701S RGB panel via DPI.
 *
 * Uses 24-bit parallel DPI at 80MHz for pixel data. The RGB panel operates
 * synchronously (writes complete before the function returns), so async
 * methods are stubbed. The directTransfer() path copies directly to the
 * PSRAM framebuffer for the fastest bulk update available.
 */
#include "TRGBDisplay.h"
#include <databus/Arduino_XL9535SWSPI.h>
#include <databus/Arduino_ESP32RGBPanel.h>
#include <display/Arduino_RGB_Display.h>

// Same ROM function Arduino_RGB_Display's own draw calls use to flush the CPU
// data cache back to PSRAM after writing pixels. The DPI panel's DMA reads the
// framebuffer directly from PSRAM, bypassing the cache, so writes that skip
// this call can stay invisible to the panel indefinitely.
extern "C" int Cache_WriteBack_Addr(uint32_t addr, uint32_t size);

namespace
{
  constexpr int GFX_BL = 46;
  constexpr int GFX_SDA = 8;
  constexpr int GFX_SCL = 48;
  constexpr int GFX_PWD = 2;
  constexpr int GFX_CS = 3;
  constexpr int GFX_SCK = 5;
  constexpr int GFX_MOSI = 4;

  // Backlight PWM: 5kHz is above the flicker/audible range; 8-bit resolution
  // matches setBrightness()'s uint8_t range.
  constexpr uint32_t BL_PWM_FREQ = 5000;
  constexpr uint8_t BL_PWM_RESOLUTION = 8;
}

TRGBDisplay::TRGBDisplay()
    : bus(nullptr), rgbpanel(nullptr), gfx(nullptr), m_initialized(false)
{
}

/**
 * @brief Initialize the ST7701S RGB panel.
 *
 * Starts I2C for the GPIO expander, creates the XL9535 SPI bus proxy,
 * instantiates the ESP32 RGB panel bus, and creates the Arduino_RGB_Display.
 * Backlight is enabled after initialization.
 */
bool TRGBDisplay::begin()
{
  if (m_initialized)
  {
    return true;
  }

  Wire.begin(GFX_SDA, GFX_SCL, 400000);

  bus = new Arduino_XL9535SWSPI(GFX_SDA, GFX_SCL, GFX_PWD, GFX_CS, GFX_SCK, GFX_MOSI);

  // Pin group order matches Arduino_ESP32RGBPanel's non-bigEndian data_gpio_nums
  // assembly ([b-args, g-args, r-args]) against LilyGo's confirmed-working raw
  // array in LilyGo_RGBPanel.cpp ([DATA13-17, DATA6-11, DATA1-5]) — the "r"/"b"
  // argument names denote array position, not which pins are physically wired
  // to red/blue on the panel.
  // prefer_speed pinned to 8MHz per LilyGo's official RGB_MAX_PIXEL_CLOCK_HZ
  // (LilyGo_RGBPanel.cpp) — Arduino_GFX's own auto-selection picks 12MHz
  // whenever CONFIG_SPIRAM_MODE_QUAD isn't defined (true here: this board
  // uses octal PSRAM).
  rgbpanel = new Arduino_ESP32RGBPanel(
      45, 41, 47, 42,
      21, 18, 17, 16, 15,
      14, 13, 12, 11, 10, 9,
      7, 6, 5, 3, 2,
      1, 50, 1, 30,
      1, 20, 1, 30,
      1, 8000000);

  gfx = new Arduino_RGB_Display(
      480, 480, rgbpanel, 0, true,
      bus, GFX_NOT_DEFINED, st7701_type4_init_operations, sizeof(st7701_type4_init_operations));

  if (!gfx->begin())
  {
    Serial.println("TRGB display begin() failed!");
    return false;
  }

  gfx->fillScreen(0x0000);
  ledcAttach(GFX_BL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
  ledcWrite(GFX_BL, m_brightness);
  m_initialized = true;

  return true;
}

/** @brief Set address window (stubbed — DPI panel uses fixed addressing). */
void TRGBDisplay::setAddrWindow(int x, int y, int w, int h)
{
  (void)x;
  (void)y;
  (void)w;
  (void)h;
}

/** @brief Copy pixels directly to the PSRAM framebuffer. */
void TRGBDisplay::pushPixels(const uint16_t *pixels, size_t count)
{
  if (gfx)
  {
    // Cast to Arduino_RGB_Display to access getFramebuffer()
    Arduino_RGB_Display *rgbDisplay = static_cast<Arduino_RGB_Display *>(gfx);
    uint16_t *fb = rgbDisplay->getFramebuffer();
    if (fb)
    {
      memcpy(fb, pixels, count * sizeof(uint16_t));
    }
  }
}

/** @brief Fill the screen with a single color. */
void TRGBDisplay::pushPixels(uint16_t color, size_t count)
{
  if (gfx)
  {
    gfx->fillScreen(color);
  }
}

/** @brief Fill a rectangle with a solid color. */
void TRGBDisplay::fillRect(int x, int y, int w, int h, uint16_t color)
{
  if (gfx)
  {
    gfx->fillRect(x, y, w, h, color);
  }
}

/** @brief Clear the entire screen. */
void TRGBDisplay::clear(uint16_t color)
{
  if (gfx)
  {
    gfx->fillScreen(color);
  }
}

/** @brief Set display rotation (0-3); also drives the directTransfer() flip for 180°. */
void TRGBDisplay::setRotation(uint8_t rotation)
{
  m_rotation180 = (rotation == 2);
  if (gfx)
  {
    gfx->setRotation(rotation);
  }
}

/** @brief Set backlight on/off, preserving the last brightness level for on. */
void TRGBDisplay::setBacklight(bool on)
{
  ledcWrite(GFX_BL, on ? m_brightness : 0);
}

/** @brief Set backlight brightness via PWM (0-255). */
void TRGBDisplay::setBrightness(uint8_t level)
{
  m_brightness = level;
  ledcWrite(GFX_BL, level);
}

/** @brief Begin a bus transaction (no-op for DPI). */
void TRGBDisplay::startWrite()
{
}

/** @brief End a bus transaction (no-op for DPI). */
void TRGBDisplay::endWrite()
{
}

/** @brief Flush pending writes (auto-flushes). */
void TRGBDisplay::flush()
{
}

/** @brief Draw a null-terminated string. */
void TRGBDisplay::drawString(int16_t x, int16_t y, const char *str, uint16_t color)
{
  if (gfx)
  {
    gfx->setCursor(x, y);
    gfx->setTextColor(color);
    gfx->print(str);
  }
}

/** @brief Draw a full 16-bit RGB bitmap. */
void TRGBDisplay::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
  if (gfx)
  {
    gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  }
}

/**
 * @brief Direct bulk transfer to the PSRAM framebuffer.
 *
 * Copies row-by-row from the source buffer to the display's PSRAM framebuffer.
 * Assumes a 480-wide framebuffer pitch. This is the fastest bulk update path
 * for the RGB panel since writes are synchronous.
 *
 * When `m_rotation180` is set (display mounted upside down), rows are copied
 * back-to-front and each row's pixels are written in reverse order, mirroring
 * the destination through the center of the whole physical panel instead of a
 * straight memcpy.
 */
void TRGBDisplay::directTransfer(uint16_t *buffer, int destX, int destY,
                                 int srcX, int srcY, int srcW, int srcH)
{
  if (!gfx || !buffer)
    return;

  // Cast to Arduino_RGB_Display to access getFramebuffer()
  Arduino_RGB_Display *rgbDisplay = static_cast<Arduino_RGB_Display *>(gfx);
  uint16_t *fb = rgbDisplay->getFramebuffer();
  if (!fb)
    return;

  int fbWidth = gfx->width();

  if (!m_rotation180)
  {
    for (int row = 0; row < srcH; row++)
    {
      uint16_t *srcRow = buffer + (srcY + row) * fbWidth + srcX;
      uint16_t *dstRow = fb + (destY + row) * fbWidth + destX;
      memcpy(dstRow, srcRow, srcW * sizeof(uint16_t));
      Cache_WriteBack_Addr((uint32_t)dstRow, srcW * sizeof(uint16_t));
    }
    return;
  }

  // 180° mount compensation: the destination position is mirrored through the
  // center of the *whole* physical panel (m_width x m_height), not just the
  // sub-rect being transferred, since the orientation is a property of how the
  // panel itself is mounted.
  for (int row = 0; row < srcH; row++)
  {
    uint16_t *srcRow = buffer + (srcY + row) * fbWidth + srcX;
    int dstRowIdx = (m_height - 1) - (destY + row);
    uint16_t *dstRow = fb + dstRowIdx * fbWidth;
    int dstColIdx = (m_width - 1) - (destX + srcW - 1);
    for (int col = 0; col < srcW; col++)
    {
      dstRow[(m_width - 1) - (destX + col)] = srcRow[col];
    }
    Cache_WriteBack_Addr((uint32_t)(dstRow + dstColIdx), srcW * sizeof(uint16_t));
  }
}

/** @brief Draw a sub-region of a bitmap (falls back to full draw). */
void TRGBDisplay::drawSubRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h,
                                   int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH)
{
  if (gfx)
  {
    gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  }
}

/** @brief Draw a bitmap asynchronously (synchronous fallback). */
bool TRGBDisplay::drawRGBBitmapAsync(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
  if (gfx)
  {
    gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  }
  return true;
}

/** @brief Returns false (RGB panel handles its own DMA). */
bool TRGBDisplay::isDMATransferBusy()
{
  return false;
}