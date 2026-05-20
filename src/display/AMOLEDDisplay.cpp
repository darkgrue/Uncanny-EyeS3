#include "AMOLEDDisplay.h"
#include "pin_config.h"

AMOLEDDisplay::AMOLEDDisplay()
    : m_gfx(nullptr), m_initialized(false)
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
  Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

  // T-Display S3 AMOLED 1.75" - CO5300 with LilyGO library signature
  // Constructor: (bus, rst, rotation, ips, width, height, col_offset1, row_offset1, col_offset2, row_offset2)
  m_gfx = new Arduino_CO5300(bus, LCD_RST, 0, false, 466, 466, 6, 0, 0, 0);

  if (!m_gfx->begin())
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
    m_gfx->setAddrWindow(x, y, w, h);
  }
}

void AMOLEDDisplay::pushPixels(const uint16_t *pixels, size_t count)
{
  if (m_gfx && pixels && count > 0)
  {
    m_gfx->startWrite();
    m_gfx->writePixels(const_cast<uint16_t *>(pixels), count);
    m_gfx->endWrite();
  }
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

void AMOLEDDisplay::flush()
{
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