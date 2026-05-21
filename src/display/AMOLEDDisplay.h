#ifndef AMOLED_DISPLAY_H
#define AMOLED_DISPLAY_H

#include "common/DisplayHAL.h"
#include "Arduino_GFX_Library.h"

class AMOLEDDisplay : public DisplayHAL {
public:
    AMOLEDDisplay();

    bool begin() override;

    int getWidth() const override { return m_width; }
    int getHeight() const override { return m_height; }

    void setAddrWindow(int x, int y, int w, int h) override;
    void pushPixels(const uint16_t* pixels, size_t count) override;
    void pushPixels(uint16_t color, size_t count) override;
    void fillRect(int x, int y, int w, int h, uint16_t color) override;
    void clear(uint16_t color = 0x0000) override;
    void setRotation(uint8_t rotation) override;
    void setBacklight(bool on) override;
    void setBrightness(uint8_t level) override;

    void startWrite() override;
    void endWrite() override;

    bool needsFlush() const override { return true; }
    void flush() override;

    void drawString(int16_t x, int16_t y, const char* str, uint16_t color = 0xFFFF) override;
    void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override;
    void drawSubRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h,
                          int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH) override;
bool drawRGBBitmapAsync(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override;
    bool isDMATransferBusy() override;

    // Poll-based wait for transfer completion (non-blocking check)
    bool waitForTransferComplete(uint32_t timeoutMs);

    // Wait for async transfer to complete
    bool waitForAsyncTransfer(uint32_t timeoutMs) override;

    // Software sync for render/display overlap
    bool beginDisplayTransfer() override;
    void endDisplayTransfer() override;
    bool isTransferComplete() override;

    // Direct bulk transfer - bypasses GFX library for maximum throughput
    // Transfers pixels directly from buffer to display using QSPI
    void directTransfer(uint16_t* buffer, int destX, int destY,
                        int srcX, int srcY, int srcW, int srcH);

private:
    Arduino_CO5300* m_gfx = nullptr;
    Arduino_DataBus* m_qspiBus = nullptr;  // Direct QSPI access for async DMA
    int m_width = 466;
    int m_height = 466;
    bool m_initialized = false;
    bool m_transferPending = false;
};

#endif // AMOLED_DISPLAY_H