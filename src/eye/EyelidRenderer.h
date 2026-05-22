#ifndef EYELID_RENDERER_H
#define EYELID_RENDERER_H

#include <stdint.h>
#include <Arduino.h>
#include "common/EyeState.h"
#include "common/DisplayGeometry.h"
#include "eyes.h"

class EyelidRenderer {
public:
    EyelidRenderer();

    void begin(int displaySize, uint16_t eyeRadius, const EyelidConfig& config);

    void setTrackingEnabled(bool enabled) { m_trackingEnabled = enabled; }
    void setSquint(bool squint) { m_squint = squint; }

    void render(float eyeX, float eyeY, float eyelidGap, uint16_t* frameBuffer);

    float getUpperLidFactor() const { return m_smoothedUpperFactor; }
    float getLowerLidFactor() const { return m_smoothedLowerFactor; }

private:
    void renderDefaultEyelids(int centerX, int centerY, float upperY, float lowerY,
                              uint16_t* buffer, int size, uint16_t color);
    void renderCustomEyelids(float blinkFactor, int centerX, int centerY,
                             uint16_t* buffer, int size, uint16_t color);

    float calculateUpperLidY(float eyeY, float gap);
    float calculateLowerLidY(float eyeY, float gap);

    int m_displaySize = 0;
    int m_eyeRadius = 0;

    const EyelidConfig* m_config = nullptr;

    bool m_hasCustomEyelids = false;
    bool m_trackingEnabled = true;
    bool m_squint = false;

    float m_smoothedUpperFactor = 1.0f;
    float m_smoothedLowerFactor = 1.0f;

    float m_prevUpperY = 0.5f;
    float m_prevLowerY = 0.5f;

    uint16_t m_eyelidColor = 0;
};

#endif // EYELID_RENDERER_H