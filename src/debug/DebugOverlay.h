#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <stdint.h>
#include "common/DisplayHAL.h"

class DebugOverlay {
public:
    DebugOverlay();

    void begin(DisplayHAL* display);

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void setBatteryPin(int pin, uint16_t minRaw, uint16_t maxRaw);

    void update();

    void render();

    float getFps() const { return m_fps; }

private:
    void updateFps();
    void updateBattery();

    DisplayHAL* m_display = nullptr;
    bool m_enabled = false;
    bool m_initialized = false;

    int m_batteryPin = -1;
    uint16_t m_batteryMinRaw = 0;
    uint16_t m_batteryMaxRaw = 4095;
    int m_batteryPercent = 0;

    uint32_t m_lastFrameTime = 0;
    uint32_t m_frameCount = 0;
    uint32_t m_fpsUpdateTime = 0;
    float m_fps = 0.0f;
};

#endif // DEBUG_OVERLAY_H