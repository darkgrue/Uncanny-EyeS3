/**
 * @file Arduino_GFX_Adapter.h
 * @brief Adapter layer to bridge Arduino_GFX v1.3.7 API to v1.6.5.
 *
 * This adapter provides backward compatibility for code that was written
 * against v1.3.7 API while using the v1.6.5 library. It uses overrides
 * and wrapper methods rather than patching the library.
 */
#pragma once

#include <Arduino_GFX.h>

class Arduino_GFX_AdapterBase : public Arduino_GFX
{
public:
    using Arduino_GFX::Arduino_GFX;

    virtual void flush(bool force_flush = false) override
    {
        flush();
    }

    void flush()
    {
    }
};