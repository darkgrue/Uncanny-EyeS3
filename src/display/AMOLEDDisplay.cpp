#include "AMOLEDDisplay.h"
#include "pin_config.h"
#include "Arduino_TFT.h"  // For getDataBus()

AMOLEDDisplay::AMOLEDDisplay()
    : m_gfx(nullptr), m_qspiBus(nullptr), m_initialized(false)
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

  // Diagnostic: Draw horizontal color bars to verify data reaches display
  Serial.println("Drawing diagnostic bars...");
  m_gfx->fillRect(0, 0, 466, 116, 0xF800); // Red top quarter
  delay(100);
  m_gfx->fillRect(0, 116, 466, 116, 0x07E0); // Green
  delay(100);
  m_gfx->fillRect(0, 232, 466, 117, 0x001F); // Blue
  delay(100);
  m_gfx->fillRect(0, 349, 466, 117, 0xFFFF); // White bottom
  delay(500);

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

  // Set address window to target region
  m_gfx->startWrite();
  m_gfx->writeAddrWindow(x, y, w, h);

  // Extract sub-region and write in chunks
  // bitmap is the full buffer, srcX/srcY is offset into it, srcW/srcH is size to copy
  uint16_t *rowPtr = bitmap + srcY * w + srcX;

  for (int16_t row = 0; row < srcH; row++) {
    m_gfx->writePixels(rowPtr, srcW);
    rowPtr += w;  // Advance to next row in full buffer
  }

  m_gfx->endWrite();
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

void AMOLEDDisplay::drawTestPattern()
{
  if (!m_gfx)
    return;

  // Test pattern dimensions
  constexpr int TEST_W = 100;
  constexpr int TEST_H = 100;

  // Create checkerboard pattern in static buffer to avoid stack overflow
  static uint16_t testBmp[TEST_W * TEST_H];

  // Fill with red/white checkerboard pattern
  for (int y = 0; y < TEST_H; y++)
  {
    for (int x = 0; x < TEST_W; x++)
    {
      bool isRed = ((x / 10) + (y / 10)) % 2 == 0;
      testBmp[y * TEST_W + x] = isRed ? 0xF800 : 0xFFFF; // Red or White
    }
  }

  // Draw test pattern in center of screen
  int16_t startX = (466 - TEST_W) / 2;
  int16_t startY = (466 - TEST_H) / 2;

  Serial.printf("[AMOLEDDisplay] Drawing %dx%d test pattern at (%d,%d)\n",
                TEST_W, TEST_H, startX, startY);

  // Like LVGL example: call draw16bitBeRGBBitmap directly without startWrite/endWrite
  m_gfx->draw16bitBeRGBBitmap(startX, startY, testBmp, TEST_W, TEST_H);

  Serial.println("[AMOLEDDisplay] Test pattern drawn.");
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