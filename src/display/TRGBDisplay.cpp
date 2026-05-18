#include "TRGBDisplay.h"
#include <Arduino.h>

namespace {
constexpr gpio_num_t PIN_HSYNC = GPIO_NUM_47;
constexpr gpio_num_t PIN_VSYNC = GPIO_NUM_41;
constexpr gpio_num_t PIN_DE = GPIO_NUM_45;
constexpr gpio_num_t PIN_PCLK = GPIO_NUM_42;
constexpr gpio_num_t PIN_BL = GPIO_NUM_46;

constexpr gpio_num_t PIN_DATA[16] = {
    GPIO_NUM_44, GPIO_NUM_21, GPIO_NUM_18, GPIO_NUM_17,
    GPIO_NUM_16, GPIO_NUM_15, GPIO_NUM_14, GPIO_NUM_13,
    GPIO_NUM_12, GPIO_NUM_11, GPIO_NUM_10, GPIO_NUM_9,
    GPIO_NUM_7, GPIO_NUM_6, GPIO_NUM_5, GPIO_NUM_3
};

struct LcdInitCmd {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes;
};

DRAM_ATTR const LcdInitCmd st7701s_init[] = {
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x10}, 5},
    {0xC0, {0x3b, 0x00}, 2},
    {0xC1, {0x06, 0x05}, 2},
    {0xC2, {0x37, 0x02}, 2},
    {0xC6, {0x21}, 1},
    {0xC3, {0x02}, 1},
    {0xCC, {0x30}, 1},
    {0xB0, {0xC0, 0x54, 0x5c, 0x0d, 0x51, 0x06, 0x09, 0x08, 0x07, 0x24, 0x03, 0x11, 0x0f, 0xac, 0xb5, 0x7f}, 16},
    {0xB1, {0xC0, 0x54, 0x5c, 0x0e, 0x11, 0x07, 0x0a, 0x09, 0x08, 0x24, 0x04, 0x51, 0x10, 0xad, 0x75, 0x7f}, 16},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x11}, 5},
    {0xB0, {0x7d}, 1},
    {0xB1, {0x3b}, 1},
    {0xB2, {0x07}, 1},
    {0xB3, {0x80}, 1},
    {0xB5, {0x45}, 1},
    {0xB7, {0x87}, 1},
    {0xB8, {0x33}, 1},
    {0xB9, {0x10}, 1},
    {0xBB, {0x03}, 1},
    {0xC0, {0x03}, 1},
    {0xC1, {0x70}, 1},
    {0xC2, {0x70}, 1},
    {0xD0, {0x88}, 1},
    {0xE0, {0x00, 0x18, 0x00, 0x00, 0x00, 0x20}, 6},
    {0xE1, {0x02, 0x00, 0x04, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x22, 0x22}, 11},
    {0xE2, {0x10, 0x10, 0x20, 0x20, 0xe7, 0x00, 0x00, 0x00, 0xe6, 0x00, 0x00, 0x00, 0x00}, 13},
    {0xE3, {0x00, 0x00, 0x11, 0x11}, 4},
    {0xE4, {0x44, 0x44}, 2},
    {0xE5, {0x03, 0xE0, 0x00, 0xF5, 0x05, 0xe2, 0x00, 0xf5, 0x07, 0xe4, 0x00, 0xf5, 0x09, 0xe6, 0x00, 0xf5}, 16},
    {0xE6, {0x00, 0x00, 0x11, 0x11}, 4},
    {0xE7, {0x44, 0x44}, 2},
    {0xE8, {0x02, 0xDF, 0x00, 0xf5, 0x04, 0xe1, 0x00, 0xf5, 0x06, 0xe3, 0x00, 0xf5, 0x08, 0xe5, 0x00, 0xf5}, 16},
    {0xEB, {0x00, 0x02, 0xe4, 0xe4, 0x88, 0x00, 0x10}, 7},
    {0xEC, {0x3D, 0x02, 0x00}, 3},
    {0xED, {0x20, 0x76, 0x54, 0x98, 0xBA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xab, 0x89, 0x45, 0x67, 0x02}, 16},
    {0xEF, {0x00, 0x00, 0x04, 0x00, 0x3f, 0x1f}, 6},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x13}, 5},
    {0xE8, {0x00, 0x0E}, 2},
    {0xE8, {0x00, 0x0C}, 2},
    {0xE8, {0x00, 0x00}, 2},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x00}, 5},
    {0x36, {0x08}, 1},
    {0x11, {0x00}, 0x80},
    {0x29, {0x00}, 0x80},
    {0x20, {0x00}, 1},
    {0, {0}, 0xff}
};

constexpr gpio_num_t PIN_CS = GPIO_NUM_3;
constexpr gpio_num_t PIN_MOSI = GPIO_NUM_4;
constexpr gpio_num_t PIN_SCLK = GPIO_NUM_5;
constexpr gpio_num_t PIN_RST = GPIO_NUM_6;

inline void send9bit(uint16_t data) {
    digitalWrite(PIN_CS, LOW);
    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_MOSI, (data >> i) & 1);
        digitalWrite(PIN_SCLK, HIGH);
        digitalWrite(PIN_SCLK, LOW);
    }
    digitalWrite(PIN_CS, HIGH);
}

inline void sendCmd(uint8_t cmd) {
    uint16_t d = cmd;
    send9bit(d);
}

inline void sendData(uint8_t data) {
    uint16_t d = data | 0x100;
    send9bit(d);
}
}

TRGBDisplay::TRGBDisplay()
    : m_panelDrv(nullptr)
    , m_framebuffer(nullptr)
    , m_backlightOn(false)
    , m_initialized(false) {
}

bool TRGBDisplay::begin() {
    if (m_initialized) {
        return true;
    }

    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, LOW);

    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_SCLK, OUTPUT);
    digitalWrite(PIN_SCLK, LOW);

    initST7701S();

    initBus();

    m_framebuffer = (uint16_t*)heap_caps_malloc(480 * 480 * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!m_framebuffer) {
        m_framebuffer = (uint16_t*)malloc(480 * 480 * 2);
    }

    clear(0x0000);
    setBacklight(true);
    m_initialized = true;

    return true;
}

void TRGBDisplay::initST7701S() {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(20);
    digitalWrite(PIN_RST, HIGH);
    delay(10);

    size_t i = 0;
    while (st7701s_init[i].databytes != 0xff) {
        uint8_t cmd = st7701s_init[i].cmd;
        uint8_t len = st7701s_init[i].databytes & 0x1F;
        sendCmd(cmd);
        for (size_t j = 0; j < len; j++) {
            sendData(st7701s_init[i].data[j]);
        }
        if (st7701s_init[i].databytes & 0x80) {
            delay(100);
        }
        i++;
    }
}

void TRGBDisplay::initBus() {
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = 8000000UL,
            .h_res = 480,
            .v_res = 480,
            .hsync_pulse_width = 1,
            .hsync_back_porch = 30,
            .hsync_front_porch = 50,
            .vsync_pulse_width = 1,
            .vsync_back_porch = 30,
            .vsync_front_porch = 20,
            .flags = {
                .pclk_active_neg = 1,
            },
        },
        .data_width = 16,
        .psram_trans_align = 64,
        .hsync_gpio_num = PIN_HSYNC,
        .vsync_gpio_num = PIN_VSYNC,
        .de_gpio_num = PIN_DE,
        .pclk_gpio_num = PIN_PCLK,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums = {
            PIN_DATA[0], PIN_DATA[1], PIN_DATA[2], PIN_DATA[3],
            PIN_DATA[4], PIN_DATA[5], PIN_DATA[6], PIN_DATA[7],
            PIN_DATA[8], PIN_DATA[9], PIN_DATA[10], PIN_DATA[11],
            PIN_DATA[12], PIN_DATA[13], PIN_DATA[14], PIN_DATA[15],
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &m_panelDrv));
    ESP_ERROR_CHECK(esp_lcd_panel_init(m_panelDrv));
}

void TRGBDisplay::resetDisplay() {
    digitalWrite(PIN_RST, LOW);
    delay(20);
    digitalWrite(PIN_RST, HIGH);
    delay(10);
}

void TRGBDisplay::writeCommand(uint8_t cmd) {
    sendCmd(cmd);
}

void TRGBDisplay::writeData(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        sendData(data[i]);
    }
}

void TRGBDisplay::setAddrWindow(int x, int y, int w, int h) {
}

void TRGBDisplay::pushPixels(const uint16_t* pixels, size_t count) {
    memcpy(m_framebuffer, pixels, count * 2);
}

void TRGBDisplay::pushPixels(uint16_t color, size_t count) {
    uint16_t* p = m_framebuffer;
    for (size_t i = 0; i < count; i++) {
        *p++ = color;
    }
}

void TRGBDisplay::fillRect(int x, int y, int w, int h, uint16_t color) {
    uint16_t* p = m_framebuffer + y * 480 + x;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            p[col] = color;
        }
        p += 480;
    }
}

void TRGBDisplay::clear(uint16_t color) {
    uint16_t* p = m_framebuffer;
    size_t total = 480 * 480;
    for (size_t i = 0; i < total; i++) {
        *p++ = color;
    }
}

void TRGBDisplay::setRotation(uint8_t rotation) {
}

void TRGBDisplay::setBacklight(bool on) {
    m_backlightOn = on;
    digitalWrite(PIN_BL, on ? HIGH : LOW);
}

void TRGBDisplay::setBrightness(uint8_t level) {
    if (level > 0 && !m_backlightOn) {
        setBacklight(true);
    } else if (level == 0 && m_backlightOn) {
        setBacklight(false);
    }
}

void TRGBDisplay::flush() {
    if (m_initialized && m_panelDrv && m_framebuffer) {
        esp_lcd_panel_draw_bitmap(m_panelDrv, 0, 0, 480, 480, m_framebuffer);
    }
}