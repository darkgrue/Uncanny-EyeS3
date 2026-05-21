#include "AMOLEDDisplay.h"
#include "pin_config.h"
#include "Arduino_TFT.h"  // For getDataBus()

#include "../lib/Arduino_GFX-1.3.7/src/databus/Arduino_ESP32QSPI.h"

AMOLEDDisplay::AMOLEDDisplay()
    : m_gfx(nullptr), m_qspiBus(nullptr), m_initialized(false)
    , m_transferPending(false)
{
}

bool AMOLEDDisplay::begin()
{
  if (m_initialized)
  {
    return true;
  }

  // Enable backlight power.
  pinMode(LCD_EN, OUTPUT);
  digitalWrite(LCD_EN, HIGH);
  delay(100);

  // Initialize QSPI bus
  m_qspiBus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

  // T-Display S3 AMOLED 1.75" - CO5300 with LilyGO library signature
  // Constructor: (bus, rst, rotation, ips, width, height, col_offset1, row_offset1, col_offset2, row_offset2)
  m_gfx = new Arduino_CO5300(m_qspiBus, LCD_RST, 0, false, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);

  if (!m_gfx->begin(SPI_FREQUENCY))
  {
    Serial.println("ERROR: CO5300 begin() failed!");
    return false;
  }
  Serial.println("CO5300 begin() successful.");

  m_gfx->fillScreen(WHITE);

  // Ramp brightness from 0 to maximum.
  for (int i = 0; i <= 255; i++)
  {
    m_gfx->Display_Brightness(i);
    delay(3);
  }

  // Clear to black for app use
  m_gfx->fillScreen(0x0000);
  m_initialized = true;

  Serial.println("AMOLED init OK.");
  return true;
}

void AMOLEDDisplay::setAddrWindow(int x, int y, int w, int h)
{
  if (m_gfx)
  {
    // Call writeAddrWindow directly WITHOUT startWrite/endWrite wrappers.
    // The caller (EyeRenderer::renderFrame) already holds the QSPI transaction open.
    // Nested startWrite/endWrite in setAddrWindow would toggle CS incorrectly.
    m_gfx->writeAddrWindow(x, y, w, h);
  }
}

void AMOLEDDisplay::pushPixels(const uint16_t *pixels, size_t count)
{
  if (m_gfx && pixels && count > 0)
  {
    m_gfx->writePixels(const_cast<uint16_t *>(pixels), count);
  }
}

void AMOLEDDisplay::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
  if (m_gfx && bitmap)
  {
    // CO5300 requires even dimensions - round up if odd
    if (w % 2 != 0) w++;
    if (h % 2 != 0) h++;
    m_gfx->draw16bitBeRGBBitmap(x, y, bitmap, w, h);
  }
}

void AMOLEDDisplay::drawSubRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h,
                                       int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH)
{
  if (!m_gfx || !bitmap) return;

  // CO5300 requires even dimensions - enforce
  int16_t dstW = srcW;
  int16_t dstH = srcH;
  if (dstW % 2 != 0) dstW++;
  if (dstH % 2 != 0) dstH++;

  // Use draw16bitBeRGBBitmap for the sub-region
  // This is the same as the working full-frame path which uses writeBytes internally
  // Create a temporary buffer for the sub-region
  static uint16_t* subBuf = nullptr;
  static int subBufSize = 0;

  int neededSize = dstW * dstH;
  if (neededSize > subBufSize) {
    if (subBuf) {
      heap_caps_free(subBuf);
    }
    subBuf = (uint16_t*)heap_caps_malloc(neededSize * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
    subBufSize = neededSize;
  }

  if (!subBuf) return;

  // Copy sub-region to contiguous buffer (no pixel reordering, justmemcpy)
  uint16_t* dst = subBuf;
  for (int16_t row = 0; row < srcH; row++) {
    uint16_t* srcRow = bitmap + (srcY + row) * w + srcX;
    memcpy(dst, srcRow, srcW * sizeof(uint16_t));
    dst += dstW;
  }

  // Use the same draw path as full-frame (which works correctly)
  m_gfx->draw16bitBeRGBBitmap(x, y, subBuf, dstW, dstH);
}

bool AMOLEDDisplay::drawRGBBitmapAsync(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
  if (!m_gfx || !bitmap) return true;  // Error

  // Use same flow as draw16bitBeRGBBitmap
  m_gfx->draw16bitBeRGBBitmap(x, y, bitmap, w, h);
  return true;
}

bool AMOLEDDisplay::beginDisplayTransfer()
{
  if (!m_gfx) return false;

  // Start write transaction and set address window
  m_gfx->startWrite();
  m_gfx->writeAddrWindow(0, 0, m_width, m_height);
  m_transferPending = true;
  return true;
}

bool AMOLEDDisplay::waitForTransferComplete(uint32_t timeoutMs)
{
  if (!m_transferPending) return true;

  Arduino_DataBus* bus = m_gfx->getDataBus();
  if (!bus) return false;

  Arduino_ESP32QSPI* qspi = (Arduino_ESP32QSPI*)bus;

  if (qspi->waitAllChunks(timeoutMs)) {
    m_transferPending = false;
    m_gfx->endWrite();
    return true;
  }
  return false;
}

bool AMOLEDDisplay::waitForAsyncTransfer(uint32_t timeoutMs)
{
  return waitForTransferComplete(timeoutMs);
}

void AMOLEDDisplay::endDisplayTransfer()
{
  if (!m_gfx) return;

  // End the write transaction
  m_gfx->endWrite();
  m_transferPending = false;
}

bool AMOLEDDisplay::isTransferComplete()
{
  // For sync implementation, transfer is complete immediately after endWrite
  // The actual transfer happens synchronously in drawRGBBitmap
  // This flag tracks whether a transfer is in progress
  return !m_transferPending;
}

bool AMOLEDDisplay::isDMATransferBusy()
{
  return m_transferPending;
}

void AMOLEDDisplay::pushPixels(uint16_t color, size_t count)
{
  if (m_gfx)
  {
    m_gfx->fillScreen(color);
  }
}

void AMOLEDDisplay::fillRect(int x, int y, int w, int h, uint16_t color)
{
  if (m_gfx)
  {
    m_gfx->fillRect(x, y, w, h, color);
  }
}

void AMOLEDDisplay::clear(uint16_t color)
{
  if (m_gfx)
  {
    m_gfx->fillScreen(color);
  }
}

void AMOLEDDisplay::setRotation(uint8_t rotation)
{
  if (m_gfx)
  {
    m_gfx->setRotation(rotation);
  }
}

void AMOLEDDisplay::setBacklight(bool on)
{
  digitalWrite(LCD_EN, on ? LOW : HIGH); // Active low enable
}

void AMOLEDDisplay::setBrightness(uint8_t level)
{
  if (m_gfx)
  {
    m_gfx->Display_Brightness(level);
  }
}

void AMOLEDDisplay::startWrite()
{
  if (m_gfx)
  {
    m_gfx->startWrite();
  }
}

void AMOLEDDisplay::endWrite()
{
  if (m_gfx)
  {
    m_gfx->endWrite();
  }
}

void AMOLEDDisplay::flush()
{
  // For QSPI displays, flush ensures all pending DMA transfers complete
  // The base Arduino_GFX::flush() handles framebuffer flush for canvas types
  // For direct display access, we rely on endWrite() completing the transfer
}

void AMOLEDDisplay::drawString(int16_t x, int16_t y, const char *str, uint16_t color)
{
  if (m_gfx)
  {
    m_gfx->setCursor(x, y);
    m_gfx->setTextColor(color);
    m_gfx->print(str);
  }
}

// Direct bulk transfer - bypasses GFX library for maximum throughput
// Uses QSPI directly to push pixel data to CO5300 display
void AMOLEDDisplay::directTransfer(uint16_t* buffer, int destX, int destY,
                                     int srcX, int srcY, int srcW, int srcH)
{
  if (!m_gfx || !buffer) return;

  // CO5300 requires even coordinates
  if (destX % 2 != 0) destX--;
  if (destY % 2 != 0) destY--;
  if (srcW % 2 != 0) srcW++;
  if (srcH % 2 != 0) srcH++;

  // Get QSPI bus for direct access
  Arduino_DataBus* bus = m_gfx->getDataBus();
  if (!bus) return;

  Arduino_ESP32QSPI* qspi = (Arduino_ESP32QSPI*)bus;

  // Set up address window for the destination region
  m_gfx->startWrite();
  m_gfx->writeAddrWindow(destX, destY, srcW, srcH);

  int totalPixels = srcW * srcH;
  uint32_t totalBytes = totalPixels * 2;

  // Check if source region is already contiguous (no copy needed)
  // Contiguous when: srcX == 0 AND srcW == m_width (full row segments)
  bool isContiguous = (srcX == 0) && (srcW == m_width);

  if (isContiguous) {
      // Source is contiguous - pass directly to QSPI
      uint16_t* srcPtr = buffer + srcY * m_width;
      qspi->writeBytes((uint8_t*)srcPtr, totalBytes);
  } else {
      // Non-contiguous source - need to copy row-by-row to temp buffer
      constexpr int STACK_BUF_PIXELS = 8192;
      static uint16_t* s_copyBuf = nullptr;
      static size_t s_copyBufSize = 0;

      uint16_t* copyBuf = nullptr;

      if (totalPixels <= STACK_BUF_PIXELS) {
          static uint16_t stackBuf[STACK_BUF_PIXELS];
          copyBuf = stackBuf;
      } else {
          if (totalPixels > (int)s_copyBufSize) {
              if (s_copyBuf) heap_caps_free(s_copyBuf);
              s_copyBuf = (uint16_t*)heap_caps_malloc(totalPixels * sizeof(uint16_t),
                                                      MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
              s_copyBufSize = totalPixels;
          }
          copyBuf = s_copyBuf;
      }

      if (!copyBuf) {
          m_gfx->endWrite();
          return;
      }

      // Copy row-by-row
      for (int row = 0; row < srcH; row++) {
          uint16_t* srcRow = buffer + (srcY + row) * m_width + srcX;
          memcpy(copyBuf + row * srcW, srcRow, srcW * sizeof(uint16_t));
      }

      qspi->writeBytes((uint8_t*)copyBuf, totalBytes);
  }

  m_gfx->endWrite();
}



