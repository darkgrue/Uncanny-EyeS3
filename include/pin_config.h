#pragma once

//  #define DO0143FAT01 //DO0143FMST02//1.43 inches (SH8601 FT3168)
#define H0175Y003AM // 1.75 inches (CO5300 CST9217)
// #define DO0143FMST10 //1.43 inches (CO5300 FT3168)

//  #define SPI_FREQUENCY 10000000
// #define SPI_FREQUENCY 20000000
// #define SPI_FREQUENCY 40000000
#define SPI_FREQUENCY 80000000
// #define SPI_FREQUENCY 80000000
//  SPI Mode, SPI_MODE0, set by Arduino_CO5300::begin().
//  SPI Host, SPI2_HOST, set by QSPI_SPI_HOST.

#define LCD_WIDTH 466  // physical display horizontal resolution
#define LCD_HEIGHT 466 // physical display vertical resolution

#define LCD_SDIO0 11
#define LCD_SDIO1 13
#define LCD_SDIO2 14
#define LCD_SDIO3 15
#define LCD_SCLK 12
#define LCD_CS 10
#define LCD_RST 17
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

#define LCD_EN 16

// IIC
#define IIC_SDA 7
#define IIC_SCL 6

// TOUCH (CST9217)
#define TP_INT 9

// Battery Voltage ADC
#define BATTERY_VOLTAGE_ADC_DATA 4

// SD
#define SD_CS 38
#define SD_MOSI 39
#define SD_MISO 40
#define SD_SCLK 41

// PCF8563
#define PCF8563_INT 9

// Light Sensor
#define LIGHT_PIN 5