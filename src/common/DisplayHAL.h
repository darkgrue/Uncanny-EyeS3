#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H

#include <stdint.h>
#include <stddef.h>

// Display Hardware Abstraction Layer
// Provides a common interface for different display types
class DisplayHAL {
public:
    virtual ~DisplayHAL() = default;
    
    // Initialize the display
    virtual bool begin() = 0;
    
    // Get display dimensions
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    
    // Set the current draw window
    virtual void setAddrWindow(int x, int y, int w, int h) = 0;
    
    // Push pixel data to display
    virtual void pushPixels(const uint16_t* pixels, size_t count) = 0;
    virtual void pushPixels(const uint16_t color, size_t count) = 0;
    
    // Fill rectangle
    virtual void fillRect(int x, int y, int w, int h, uint16_t color) = 0;
    
    // Clear screen with color
    virtual void clear(uint16_t color = 0x0000) = 0;
    
    // Set rotation
    virtual void setRotation(uint8_t rotation) = 0;
    
    // Turn display on/off
    virtual void setBacklight(bool on) = 0;
    virtual void setBrightness(uint8_t level) = 0;  // 0-255
    
// Check if display needs DMA/complete
    virtual bool needsFlush() const = 0;
    virtual void flush() = 0;

    // Bus transaction management (for QSPI/SPI displays)
    virtual void startWrite() = 0;
    virtual void endWrite() = 0;

    // Async transfer support - split transfer into phases for render/transfer overlap
    virtual bool beginAsyncTransfer() = 0;
    virtual bool writePixelsAsync(uint16_t *pixels, size_t count) = 0;
    virtual bool endAsyncTransfer() = 0;
    virtual bool isAsyncTransferComplete() = 0;

    // Draw text at position
    virtual void drawString(int16_t x, int16_t y, const char* str, uint16_t color = 0xFFFF) = 0;

    // Draw 16-bit RGB bitmap
    virtual void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) = 0;

    // Draw sub-region of 16-bit RGB bitmap (for partial updates)
    virtual void drawSubRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h,
                                  int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH) = 0;

    // Async version - returns immediately, dmaCompleted signals when transfer finishes
    virtual bool drawRGBBitmapAsync(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) = 0;
    virtual bool isDMATransferBusy() = 0;

    // Software sync render/display overlap
    virtual bool beginDisplayTransfer() = 0;
    virtual void endDisplayTransfer() = 0;
    virtual bool isTransferComplete() = 0;
};

#endif // DISPLAY_HAL_H
