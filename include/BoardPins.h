/**
 * @file BoardPins.h
 * @brief Board-specific pin definitions and I2C bus configuration.
 *
 * Maps the shared peripheral signals to the correct GPIO numbers for each
 * supported board (T-Display S3 AMOLED vs T-RGB).
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

// Battery ADC
#define BATTERY_VOLTAGE_ADC_DATA 4

// Display Configuration
#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)
#define BOARD_NAME "T-Display S3 AMOLED"
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define LCD_SDIO0 11
#define LCD_SDIO1 13
#define LCD_SDIO2 14
#define LCD_SDIO3 15
#define LCD_SCLK 12
#define LCD_CS 10
#define LCD_RST 17
#define LCD_EN 16
#define QSPI_FREQUENCY 90000000
#elif defined(ARDUINO_LILYGO_T_RGB)
#define BOARD_NAME "T-RGB"
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define LCD_SDIO0 11
#define LCD_SDIO1 13
#define LCD_SDIO2 14
#define LCD_SDIO3 15
#define LCD_SCLK 12
#define LCD_CS 10
#define LCD_RST 17
#define LCD_EN 16
#define QSPI_FREQUENCY 90000000
#else
#define BOARD_NAME "Unknown"
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#endif

// I2C Bus Configuration
#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)
#define IIC_SDA 7
#define IIC_SCL 6
#define QWIIC_SDA 43
#define QWIIC_SCL 44
#elif defined(ARDUINO_LILYGO_T_RGB)
#define IIC_SDA 48
#define IIC_SCL 8
#define QWIIC_SDA 43
#define QWIIC_SCL 44
#else
#define IIC_SDA 21
#define IIC_SCL 22
#endif

// Light Sensor
#define LIGHT_PIN 5

// RTC
#define PCF85063_INT 9

// SD Card
#define SD_CS 38
#define SD_MOSI 39
#define SD_MISO 40
#define SD_SCLK 41

// Touch
#define TP_INT 9

#endif // BOARD_PINS_H