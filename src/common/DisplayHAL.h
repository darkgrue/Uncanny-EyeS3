/**
 * @file DisplayHAL.h
 * @brief Hardware abstraction layer for display peripherals.
 *
 * Defines a common interface (abstract base class) for display drivers, allowing
 * EyeRenderer and other components to operate regardless of whether the target
 * is an AMOLED (CO5300 via QSPI) or an RGB panel (ST7701S via DPI).
 *
 * Implementations must provide pixel push, rectangle fill, rectangle clear,
 * async transfer support, and direct bulk transfer for maximum throughput.
 */
#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Abstract base class for display hardware.
 *
 * Provides a unified interface for pixel-level operations, async transfer
 * management, and display-specific features like brightness and rotation.
 * Implementations: AMOLEDDisplay, TRGBDisplay.
 */
class DisplayHAL {
public:
    virtual ~DisplayHAL() = default;

    /** @brief Initialize the display hardware. */
    virtual bool begin() = 0;

    /** @brief Horizontal resolution in pixels. */
    virtual int getWidth() const = 0;

    /** @brief Vertical resolution in pixels. */
    virtual int getHeight() const = 0;

    /** @brief Set the active pixel window for subsequent writes. */
    virtual void setAddrWindow(int x, int y, int w, int h) = 0;

    /** @brief Write an array of RGB565 pixels. */
    virtual void pushPixels(const uint16_t* pixels, size_t count) = 0;

    /** @brief Write a single repeated RGB565 color. */
    virtual void pushPixels(const uint16_t color, size_t count) = 0;

    /** @brief Fill a rectangle with a solid color. */
    virtual void fillRect(int x, int y, int w, int h, uint16_t color) = 0;

    /** @brief Clear the entire screen to a color. */
    virtual void clear(uint16_t color = 0x0000) = 0;

    /** @brief Set display rotation (0-3). */
    virtual void setRotation(uint8_t rotation) = 0;

    /** @brief Turn the backlight on or off. */
    virtual void setBacklight(bool on) = 0;

    /** @brief Set backlight brightness (0-255). */
    virtual void setBrightness(uint8_t level) = 0;

    /** @brief Returns true when a flush is needed. */
    virtual bool needsFlush() const = 0;

    /** @brief Flush pending writes to the display. */
    virtual void flush() = 0;

    /** @brief Begin a bus transaction (QSPI/SPI). */
    virtual void startWrite() = 0;

    /** @brief End a bus transaction. */
    virtual void endWrite() = 0;

    // --- Async transfer support ---

    /** @brief Begin an asynchronous pixel transfer. */
    virtual bool beginAsyncTransfer() { return true; }

    /** @brief Write pixels asynchronously. */
    virtual bool writePixelsAsync(uint16_t*, size_t) { return true; }

    /** @brief Complete the asynchronous transfer. */
    virtual bool endAsyncTransfer() { return true; }

    /** @brief Check if async transfer is complete. */
    virtual bool isAsyncTransferComplete() { return true; }

    /** @brief Block until async transfer finishes. */
    virtual bool waitForAsyncTransfer(uint32_t) { return true; }

    // --- Text and bitmap drawing ---

    /** @brief Draw a null-terminated string at the given position. */
    virtual void drawString(int16_t x, int16_t y, const char* str, uint16_t color = 0xFFFF) = 0;

    /** @brief Draw a full RGB565 bitmap at the given position. */
    virtual void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) = 0;

    /** @brief Draw a sub-region of an RGB565 bitmap. */
    virtual void drawSubRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h,
                                  int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH) = 0;

    /** @brief Draw a bitmap asynchronously (returns immediately if DMA is busy). */
    virtual bool drawRGBBitmapAsync(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) = 0;

    /** @brief Returns true if DMA transfer is still in progress. */
    virtual bool isDMATransferBusy() = 0;

    // --- Software-sync render/display overlap ---

    /** @brief Begin a render-to-display transfer sequence. */
    virtual bool beginDisplayTransfer() = 0;

    /** @brief End and finalize the transfer. */
    virtual void endDisplayTransfer() = 0;

    /** @brief Returns true if the transfer is complete. */
    virtual bool isTransferComplete() = 0;

    // --- Direct bulk transfer ---

    /**
     * @brief Direct buffer transfer bypassing the GFX library.
     *
     * Achieves maximum throughput by copying directly from a PSRAM buffer
     * to the display using QSPI/DPI without per-pixel GFX overhead.
     */
    virtual void directTransfer(uint16_t* buffer, int destX, int destY,
                                 int srcX, int srcY, int srcW, int srcH) = 0;

    /** @brief Async version of directTransfer. */
    virtual void directTransferAsync(uint16_t* buffer, int destX, int destY,
                                     int srcX, int srcY, int srcW, int srcH) {
        directTransfer(buffer, destX, destY, srcX, srcY, srcW, srcH);
    }

    /** @brief Chunked async transfer (returns false by default). */
    virtual bool directTransferChunkedAsync(uint16_t* buffer, int destX, int destY,
                                             int srcX, int srcY, int srcW, int srcH) {
        (void)buffer; (void)destX; (void)destY; (void)srcX; (void)srcY; (void)srcW; (void)srcH;
        return false;
    }
};

#endif // DISPLAY_HAL_H