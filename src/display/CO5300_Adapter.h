/**
 * @file CO5300_Adapter.h
 * @brief Adapter for CO5300 display to provide v1.3.7 API on v1.6.5 library.
 *
 * In v1.6.5, Arduino_CO5300 inherits from Arduino_OLED which changed the
 * API: Display_Brightness() -> setBrightness(), SetContrast() -> setContrast()
 *
 * This adapter provides the v1.3.7 method names as aliases/wrappers.
 */
#pragma once

#include <Arduino_GFX.h>

class Arduino_CO5300;

class CO5300_Adapter : public Arduino_CO5300
{
public:
    Arduino_CO5300_Adapter(Arduino_DataBus *bus, int8_t rst = GFX_NOT_DEFINED, uint8_t r = 0,
                           int16_t w = 480, int16_t h = 480,
                           uint8_t col_offset1 = 0, uint8_t row_offset1 = 0,
                           uint8_t col_offset2 = 0, uint8_t row_offset2 = 0)
        : Arduino_CO5300(bus, rst, r, w, h, col_offset1, row_offset1, col_offset2, row_offset2)
    {
    }

    void Display_Brightness(uint8_t brightness)
    {
        setBrightness(brightness);
    }

    void SetContrast(uint8_t contrast)
    {
        setContrast(contrast);
    }
};