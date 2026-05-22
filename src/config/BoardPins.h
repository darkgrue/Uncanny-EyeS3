/**
 * @file BoardPins.h
 * @brief Board-specific pin definitions and I2C bus configuration.
 *
 * Maps the shared peripheral signals to the correct GPIO numbers for each
 * supported board (T-Display S3 AMOLED vs T-RGB). WiiChuck and other I2C
 * peripherals use BOARD_I2C_SDA / BOARD_I2C_SCL which differ between boards.
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/**
 * @brief Board-specific I2C and peripheral pin definitions.
 *
 * ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED: T-Display S3 AMOLED 1.43" (CO5300)
 *   I2C for extensions: SDA=IO7, SCL=IO6
 *
 * ARDUINO_LILYGO_T_RGB: T-RGB 1.75" (ST7701S)
 *   I2C for extensions: SDA=IO48, SCL=IO8
 */
#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)
#define BOARD_I2C_SDA 7
#define BOARD_I2C_SCL 6
#define BOARD_NAME "T-Display S3 AMOLED"
#elif defined(ARDUINO_LILYGO_T_RGB)
#define BOARD_I2C_SDA 48
#define BOARD_I2C_SCL 8
#define BOARD_NAME "T-RGB"
#else
#define BOARD_I2C_SDA 21
#define BOARD_I2C_SCL 22
#define BOARD_NAME "Unknown"
#endif

#endif // BOARD_PINS_H