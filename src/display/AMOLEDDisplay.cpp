/**
 * @file AMOLEDDisplay.cpp
 * @brief Display driver implementation for the CO5300 AMOLED panel via QSPI.
 *
 * Initializes the ESP32 QSPI bus and CO5300 display controller with a 80MHz
 * SPI clock. Provides both the GFX-based pixel path and a directTransfer()
 * path that bypasses per-pixel library overhead for maximum throughput.
 *
 * The backlight is ramped from 0 to full brightness over ~750ms at startup.
 * Even dimensions are enforced throughout (CO5300 requires 16-bit aligned writes).
 */
#include "AMOLEDDisplay.h"
#include "BoardPins.h"

// GFX library headers - use subdirectory paths relative to include path .pio/libdeps/amoled/GFX Library for Arduino/src
#include "databus/Arduino_ESP32QSPI.h"
#include "display/Arduino_CO5300.h"
#include "Arduino_TFT.h"

AMOLEDDisplay::AMOLEDDisplay()
    : m_gfx(nullptr), m_qspiBus(nullptr), m_initialized(false), m_transferPending(false)
{
}

/**
 * @brief Initialize the CO5300 display and QSPI bus.
 *
 * Enables backlight power, creates the QSPI bus with LCD_CS/SCLK/SDIO0-3,
 * instantiates the CO5300 GFX object, ramps brightness from 0→255 over
 * ~750ms, and clears to black.
 */
bool AMOLEDDisplay::begin()
{
  if (m_initialized)
  {
    return true;
  }

  pinMode(LCD_EN, OUTPUT);
  digitalWrite(LCD_EN, HIGH);
  delay(100);

  m_qspiBus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

  m_gfx = new Arduino_CO5300(m_qspiBus, LCD_RST, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);

  if (!m_gfx->begin(QSPI_FREQUENCY))
  {
    Serial.println("ERROR: CO5300 begin() failed!");
    return false;
  }
  Serial.println("CO5300 begin() successful.");

  m_gfx->fillScreen(RGB565_WHITE);

  for (int i = 0; i <= 255; i++)
  {
    m_gfx->setBrightness(i);
    delay(3);
  }

  m_gfx->fillScreen(0x0000);
  m_initialized = true;

  Serial.println("AMOLED init OK.");
  return true;
}

/** @brief Set the pixel address window for subsequent writePixels calls. */
void AMOLEDDisplay::setAddrWindow(int x, int y, int w, int h)
{
  if (m_gfx)
  {
    m_gfx->writeAddrWindow(x, y, w, h);
  }
}

/** @brief Write an array of RGB565 pixels to the display. */
void AMOLEDDisplay::pushPixels(const uint16_t *pixels, size_t count)
{
  if (m_gfx && pixels && count > 0)
  {
    m_gfx->writePixels(const_cast<uint16_t *>(pixels), count);
  }
}

/**
 * @brief Draw a 16-bit RGB bitmap.
 *
 * CO5300 requires even width/height — odd dimensions are rounded up.
 */
void AMOLEDDisplay::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
  if (m_gfx && bitmap)
  {
    if (w % 2 != 0)
      w++;
    if (h % 2 != 0)
      h++;
    m_gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  }
}

/**
 * @brief Draw a sub-region of a 16-bit RGB bitmap.
 *
 * Copies the sub-region to a contiguous buffer (CO5300 requires even stride)
 * before drawing. Memory is allocated on first use and reused subsequently.
 */
void AMOLEDDisplay::drawSubRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h,
                                     int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH)
{
  if (!m_gfx || !bitmap)
    return;

  int16_t dstW = srcW;
  int16_t dstH = srcH;
  if (dstW % 2 != 0)
    dstW++;
  if (dstH % 2 != 0)
    dstH++;

  static uint16_t *subBuf = nullptr;
  static int subBufSize = 0;

  int neededSize = dstW * dstH;
  if (neededSize > subBufSize)
  {
    if (subBuf)
    {
      heap_caps_free(subBuf);
    }
    subBuf = (uint16_t *)heap_caps_malloc(neededSize * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
    subBufSize = neededSize;
  }

  if (!subBuf)
    return;

  uint16_t *dst = subBuf;
  for (int16_t row = 0; row < srcH; row++)
  {
    uint16_t *srcRow = bitmap + (srcY + row) * w + srcX;
    memcpy(dst, srcRow, srcW * sizeof(uint16_t));
    dst += dstW;
  }

  m_gfx->draw16bitRGBBitmap(x, y, subBuf, dstW, dstH);
}

/** @brief Draw a bitmap asynchronously (delegates to synchronous draw). */
bool AMOLEDDisplay::drawRGBBitmapAsync(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
  if (!m_gfx || !bitmap)
    return true;
  m_gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  return true;
}

/**
 * @brief Begin a display transfer sequence.
 *
 * Opens a QSPI write transaction and sets the full-screen address window.
 * Use before a directTransfer() call.
 */
bool AMOLEDDisplay::beginDisplayTransfer()
{
  if (!m_gfx)
    return false;
  m_gfx->startWrite();
  m_gfx->writeAddrWindow(0, 0, m_width, m_height);
  m_transferPending = true;
  return true;
}

/**
 * @brief Block until all QSPI chunks are transferred.
 * @param timeoutMs Maximum time to wait.
 * @return true if transfer completed within timeout.
 */
bool AMOLEDDisplay::waitForTransferComplete(uint32_t timeoutMs)
{
  if (!m_transferPending)
    return true;

  // In v1.6.5, transfers are synchronous/blocking, so no need to wait
  (void)timeoutMs;
  m_transferPending = false;
  m_gfx->endWrite();
  return true;
}

/** @brief Wait for an async transfer to complete. */
bool AMOLEDDisplay::waitForAsyncTransfer(uint32_t timeoutMs)
{
  return waitForTransferComplete(timeoutMs);
}

/** @brief End the display transfer sequence and close the QSPI transaction. */
void AMOLEDDisplay::endDisplayTransfer()
{
  if (!m_gfx)
    return;
  m_gfx->endWrite();
  m_transferPending = false;
}

/** @brief Returns true when no transfer is in progress. */
bool AMOLEDDisplay::isTransferComplete()
{
  return !m_transferPending;
}

/** @brief Returns true if a DMA transfer is still pending. */
bool AMOLEDDisplay::isDMATransferBusy()
{
  return m_transferPending;
}

/** @brief Fill the screen with a single repeated color. */
void AMOLEDDisplay::pushPixels(uint16_t color, size_t count)
{
  if (m_gfx)
  {
    m_gfx->fillScreen(color);
  }
}

/** @brief Fill a rectangle with a solid color. */
void AMOLEDDisplay::fillRect(int x, int y, int w, int h, uint16_t color)
{
  if (m_gfx)
  {
    m_gfx->fillRect(x, y, w, h, color);
  }
}

/** @brief Clear the entire screen to a color. */
void AMOLEDDisplay::clear(uint16_t color)
{
  if (m_gfx)
  {
    m_gfx->fillScreen(color);
  }
}

/** @brief Set display rotation (0-3). */
void AMOLEDDisplay::setRotation(uint8_t rotation)
{
  if (m_gfx)
  {
    m_gfx->setRotation(rotation);
  }
}

/** @brief Set backlight state (active-low enable). */
void AMOLEDDisplay::setBacklight(bool on)
{
  digitalWrite(LCD_EN, on ? LOW : HIGH);
}

/** @brief Set display brightness (0-255). */
void AMOLEDDisplay::setBrightness(uint8_t level)
{
  if (m_gfx)
  {
    m_gfx->setBrightness(level);
  }
}

/** @brief Begin a QSPI bus transaction. */
void AMOLEDDisplay::startWrite()
{
  if (m_gfx)
  {
    m_gfx->startWrite();
  }
}

/** @brief End a QSPI bus transaction. */
void AMOLEDDisplay::endWrite()
{
  if (m_gfx)
  {
    m_gfx->endWrite();
  }
}

/** @brief Flush pending writes (delegates to endWrite completion). */
void AMOLEDDisplay::flush()
{
  // QSPI transfers are synchronous; flush is a no-op handled by endWrite.
}

/** @brief Draw a null-terminated string at the given position. */
void AMOLEDDisplay::drawString(int16_t x, int16_t y, const char *str, uint16_t color)
{
  if (m_gfx)
  {
    m_gfx->setCursor(x, y);
    m_gfx->setTextColor(color);
    m_gfx->print(str);
  }
}

/**
 * @brief Direct bulk transfer bypassing the GFX library.
 *
 * Uses QSPI writeBytes directly for maximum throughput. Enforces even
 * coordinates and dimensions (CO5300 requirement). Handles both contiguous
 * and non-contiguous source buffers by copying row-by-row when needed.
 */
void AMOLEDDisplay::directTransfer(uint16_t *buffer, int destX, int destY,
                                   int srcX, int srcY, int srcW, int srcH)
{
  if (!m_gfx || !buffer)
    return;

  if (destX % 2 != 0)
    destX--;
  if (destY % 2 != 0)
    destY--;
  if (srcW % 2 != 0)
    srcW++;
  if (srcH % 2 != 0)
    srcH++;

  if (!m_qspiBus)
    return;

  m_gfx->startWrite();
  m_gfx->writeAddrWindow(destX, destY, srcW, srcH);

  int totalPixels = srcW * srcH;
  uint32_t totalBytes = totalPixels * 2;

  bool isContiguous = (srcX == 0) && (srcW == m_width);

  if (isContiguous)
  {
    uint16_t *srcPtr = buffer + srcY * m_width;
    m_qspiBus->writeBytes((uint8_t *)srcPtr, totalBytes);
  }
  else
  {
    constexpr int STACK_BUF_PIXELS = 8192;
    static uint16_t *s_copyBuf = nullptr;
    static size_t s_copyBufSize = 0;

    uint16_t *copyBuf = nullptr;

    if (totalPixels <= STACK_BUF_PIXELS)
    {
      static uint16_t stackBuf[STACK_BUF_PIXELS];
      copyBuf = stackBuf;
    }
    else
    {
      if (totalPixels > (int)s_copyBufSize)
      {
        if (s_copyBuf)
          heap_caps_free(s_copyBuf);
        s_copyBuf = (uint16_t *)heap_caps_malloc(totalPixels * sizeof(uint16_t),
                                                 MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
        s_copyBufSize = totalPixels;
      }
      copyBuf = s_copyBuf;
    }

    if (!copyBuf)
    {
      m_gfx->endWrite();
      return;
    }

    for (int row = 0; row < srcH; row++)
    {
      uint16_t *srcRow = buffer + (srcY + row) * m_width + srcX;
      memcpy(copyBuf + row * srcW, srcRow, srcW * sizeof(uint16_t));
    }

    m_qspiBus->writeBytes((uint8_t *)copyBuf, totalBytes);
  }

  m_gfx->endWrite();
}