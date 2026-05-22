#include "EyelidRenderer.h"
#include "animation/EyeMovement.h"
#include <cmath>

EyelidRenderer::EyelidRenderer()
    : m_displaySize(0)
    , m_eyeRadius(0)
    , m_config(nullptr)
    , m_hasCustomEyelids(false)
    , m_trackingEnabled(true)
    , m_squint(false)
    , m_smoothedUpperFactor(1.0f)
    , m_smoothedLowerFactor(1.0f)
    , m_prevUpperY(0.5f)
    , m_prevLowerY(0.5f)
    , m_eyelidColor(0) {
}

void EyelidRenderer::begin(int displaySize, uint16_t eyeRadius, const EyelidConfig& config) {
    m_displaySize = displaySize;
    m_eyeRadius = eyeRadius;
    m_config = &config;

    m_hasCustomEyelids = (config.upper != nullptr && config.lower != nullptr);
    m_eyelidColor = config.color;

    m_smoothedUpperFactor = EYELID_DEFAULT_GAP;
    m_smoothedLowerFactor = EYELID_DEFAULT_GAP;
    m_prevUpperY = 0.5f - (EYELID_DEFAULT_GAP * 0.5f);
    m_prevLowerY = 0.5f + (EYELID_DEFAULT_GAP * 0.5f);
}

float EyelidRenderer::calculateUpperLidY(float eyeY, float gap) {
    float baseUpperY = 0.5f - (gap * 0.5f);

    if (m_trackingEnabled) {
        float trackingOffset = -eyeY * EYELID_UPPER_TRACK_STRENGTH * 0.5f;
        baseUpperY += trackingOffset;
    }

    return baseUpperY;
}

float EyelidRenderer::calculateLowerLidY(float eyeY, float gap) {
    float baseLowerY = 0.5f + (gap * 0.5f);

    if (m_trackingEnabled) {
        float trackingOffset = -eyeY * EYELID_LOWER_TRACK_STRENGTH * 0.5f;
        baseLowerY += trackingOffset;
    }

    return baseLowerY;
}

void EyelidRenderer::render(float eyeX, float eyeY, float eyelidGap, uint16_t* frameBuffer) {
    if (m_squint) {
        eyelidGap *= EYELID_SQUINT_FACTOR;
    }

    float targetUpperY = calculateUpperLidY(eyeY, eyelidGap);
    float targetLowerY = calculateLowerLidY(eyeY, eyelidGap);

#if BLINK_USE_SMOOTHstep
    constexpr float smoothK = 3.0f;
    auto smoothstep = [](float t) {
        t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;
        return t * t * (3.0f - 2.0f * t);
    };
    targetUpperY = smoothstep(targetUpperY);
    targetLowerY = smoothstep(targetLowerY);
    (void)smoothK;
#else
    (void)0;
#endif

    float upperDelta = targetUpperY - m_prevUpperY;
    float lowerDelta = targetLowerY - m_prevLowerY;

    float upperStep = upperDelta * EYELID_SMOOTHING;
    float lowerStep = lowerDelta * EYELID_SMOOTHING;

    m_smoothedUpperFactor = m_prevUpperY + upperStep;
    m_smoothedLowerFactor = m_prevLowerY + lowerStep;

    m_prevUpperY = m_smoothedUpperFactor;
    m_prevLowerY = m_smoothedLowerFactor;

    int centerX = m_displaySize / 2;
    int centerY = m_displaySize / 2;

    int offsetX = (int)(eyeX * (m_displaySize / 4));
    int offsetY = (int)(eyeY * (m_displaySize / 4));

    int eyeCenterX = centerX + offsetX;
    int eyeCenterY = centerY + offsetY;

    if (m_hasCustomEyelids && m_config != nullptr) {
        renderCustomEyelids(eyelidGap, eyeCenterX, eyeCenterY, frameBuffer, m_displaySize, m_eyelidColor);
    } else {
        float upperYNorm = m_smoothedUpperFactor;
        float lowerYNorm = m_smoothedLowerFactor;
        renderDefaultEyelids(eyeCenterX, eyeCenterY, upperYNorm, lowerYNorm, frameBuffer, m_displaySize, m_eyelidColor);
    }
}

void EyelidRenderer::renderDefaultEyelids(int centerX, int centerY, float upperY, float lowerY,
                                          uint16_t* buffer, int size, uint16_t color) {
    int upperYPos = (int)(upperY * size);
    int lowerYPos = (int)(lowerY * size);

    int eyeRadius = m_eyeRadius;

    // Only iterate over the eye's bounding box (circular region)
    int minY = centerY - eyeRadius;
    int maxY = centerY + eyeRadius;
    int minX = centerX - eyeRadius;
    int maxX = centerX + eyeRadius;

    // Clamp to display bounds
    if (minY < 0) minY = 0;
    if (maxY >= size) maxY = size - 1;
    if (minX < 0) minX = 0;
    if (maxX >= size) maxX = size - 1;

    int eyeRadiusSq = eyeRadius * eyeRadius;

    // Precompute corner pinch factor
    float cornerPinch = 0.35f;
    float cornerY = upperY + (lowerY - upperY) * cornerPinch;
    (void)cornerPinch;
    int cornerYPos = (int)(cornerY * size);
    (void)cornerYPos;

    // Precompute inverse radius for normalization
    float invRadius = 1.0f / (float)eyeRadius;
    (void)cornerY;

    for (int y = minY; y <= maxY; y++) {
        int dy = y - centerY;
        int dySq = dy * dy;

        // Check if row intersects upper eyelid region
        bool aboveUpper = (y < upperYPos);
        bool belowLower = (y > lowerYPos);

        if (!aboveUpper && !belowLower) continue;

        for (int x = minX; x <= maxX; x++) {
            int dx = x - centerX;
            int distSq = dx * dx + dySq;

            // Only process pixels within circular eye boundary
            if (distSq > eyeRadiusSq) continue;

            bool occluded = false;

            if (aboveUpper) {
                // Upper eyelid: check if inside the curved eyelid shape
                float centerToY = (float)dy * invRadius;
                float lidBoundary = (upperY - 0.5f) * 2.0f;
                occluded = centerToY < lidBoundary;
            } else if (belowLower) {
                // Lower eyelid: check if inside the curved eyelid shape
                float centerToY = (float)dy * invRadius;
                float lidBoundary = (lowerY - 0.5f) * 2.0f;
                occluded = centerToY > lidBoundary;
            }

            if (occluded) {
                buffer[y * size + x] = color;
            }
        }
    }
}

void EyelidRenderer::renderCustomEyelids(float blinkFactor, int centerX, int centerY,
                                          uint16_t* buffer, int size, uint16_t color) {
    if (m_config == nullptr) return;

    int effectiveUpper = (int)(blinkFactor * size * 0.5f);
    int effectiveLower = size - effectiveUpper;

    int mapWidth = size;

    for (int x = 0; x < mapWidth; x++) {
        int tableIdx = x * 2;

        uint8_t upperStart = 0;
        uint8_t upperEnd = 0;
        uint8_t lowerStart = 0;
        uint8_t lowerEnd = 0;

        if (m_config->upper != nullptr) {
            upperStart = m_config->upper[tableIdx];
            upperEnd = m_config->upper[tableIdx + 1];
        }

        if (m_config->lower != nullptr) {
            lowerStart = m_config->lower[tableIdx];
            lowerEnd = m_config->lower[tableIdx + 1];
        }

        if (upperStart == 255 || upperEnd == 255) {
            upperStart = 0;
            upperEnd = (uint8_t)(blinkFactor * size * 0.5f);
        }

        if (lowerStart == 255 || lowerEnd == 255) {
            lowerStart = (uint8_t)(size - blinkFactor * size * 0.5f);
            lowerEnd = size;
        }

        int upperLidY = (int)centerY + (int)upperStart - (int)(effectiveUpper * (1.0f - (float)upperEnd / (float)size));
        int lowerLidY = (int)centerY + (int)lowerStart - (int)((size - effectiveLower) * (1.0f - (float)lowerEnd / (float)size));

        upperLidY = (upperLidY < 0) ? 0 : (upperLidY >= size) ? size - 1 : upperLidY;
        lowerLidY = (lowerLidY < 0) ? 0 : (lowerLidY >= size) ? size - 1 : lowerLidY;

        for (int y = upperLidY; y <= lowerLidY; y++) {
            if (y >= 0 && y < size) {
                int idx = y * size + x;
                buffer[idx] = color;
            }
        }
    }
}