#ifndef BOARD_PINS_H
#define BOARD_PINS_H

// Board-specific pin definitions
// These are used by peripherals like WiiChuck that need different I2C pins per board

#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)
    // T-Display S3 AMOLED 1.43" - SH8601 display
    // I2C for WiiChuck/extensions: SDA=IO07, SCL=IO06
    #define BOARD_I2C_SDA 7
    #define BOARD_I2C_SCL 6
    #define BOARD_NAME "T-Display S3 AMOLED"
#elif defined(ARDUINO_LILYGO_T_RGB)
    // T-RGB - ST7701S display
    // I2C for WiiChuck/extensions: SDA=IO48, SCL=IO08
    #define BOARD_I2C_SDA 48
    #define BOARD_I2C_SCL 8
    #define BOARD_NAME "T-RGB"
#else
    // Default fallback
    #define BOARD_I2C_SDA 21
    #define BOARD_I2C_SCL 22
    #define BOARD_NAME "Unknown"
#endif

#endif // BOARD_PINS_H