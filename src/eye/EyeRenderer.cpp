#include "EyeRenderer.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

EyeRenderer::EyeRenderer()
    : m_display(nullptr)
    , m_displaySize(0)
    , m_mapRadius(0)
    , m_mapDiameter(0)
    , m_eyeDef(nullptr)
    , m_frameBuf1(nullptr)
    , m_frameBuf2(nullptr)
    , m_renderBuf(nullptr)
    , m_displayBuf(nullptr)
    , m_dirtyMinX(0), m_dirtyMinY(0), m_dirtyMaxX(0), m_dirtyMaxY(0)
    , m_prevDirtyMinX(0), m_prevDirtyMinY(0), m_prevDirtyMaxX(0), m_prevDirtyMaxY(0)
    , m_transferInProgress(false)
    , m_lastTransferTime(0)
{
}

EyeRenderer::~EyeRenderer() {
    if (m_frameBuf1) {
        heap_caps_free(m_frameBuf1);
        m_frameBuf1 = nullptr;
    }
    if (m_frameBuf2) {
        heap_caps_free(m_frameBuf2);
        m_frameBuf2 = nullptr;
    }
}

bool EyeRenderer::begin(DisplayHAL* display, const EyeDefinition& eyeDef) {
    m_display = display;
    m_eyeDef = &eyeDef;
    m_displaySize = display->getWidth();
    if (display->getHeight() < m_displaySize) {
        m_displaySize = display->getHeight();
    }
    m_mapRadius = eyeDef.polarMap.radius;
    m_mapDiameter = m_mapRadius * 2;

    // Allocate double buffers in PSRAM with 4-byte alignment (addr_align requirement)
    size_t bufSize = m_displaySize * m_displaySize * sizeof(uint16_t);
    // Use MALLOC_CAP_32BIT for proper alignment (addr_align=4 for CO5300)
    m_frameBuf1 = (uint16_t*)heap_caps_malloc(bufSize, MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
    m_frameBuf2 = (uint16_t*)heap_caps_malloc(bufSize, MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);

    if (!m_frameBuf1 || !m_frameBuf2) {
        Serial.println("[EyeRenderer] Failed to allocate double buffers!");
        if (m_frameBuf1) heap_caps_free(m_frameBuf1);
        if (m_frameBuf2) heap_caps_free(m_frameBuf2);
        m_frameBuf1 = m_frameBuf2 = nullptr;
        return false;
    }

    // Start with buf1 as render target, buf2 as display target
    m_renderBuf = m_frameBuf1;
    m_displayBuf = m_frameBuf2;

    // Initialize dirty region to full screen
    m_dirtyMinX = 0;
    m_dirtyMinY = 0;
    m_dirtyMaxX = m_displaySize;
    m_dirtyMaxY = m_displaySize;
    m_prevDirtyMinX = 0;
    m_prevDirtyMinY = 0;
    m_prevDirtyMaxX = m_displaySize;
    m_prevDirtyMaxY = m_displaySize;
    m_transferInProgress = false;
    m_lastTransferTime = 0;

    Serial.printf("[EyeRenderer] Double buffer allocated: %d bytes each\n", bufSize);

    return true;
}

void EyeRenderer::renderFrame(float eyeX, float eyeY, float pupilFactor,
                              float upperLidFactor, float lowerLidFactor, float blinkFactor,
                              uint16_t irisAngle, uint16_t scleraAngle) {
    if (!m_display || !m_eyeDef || !m_renderBuf) {
        Serial.println("[EyeRenderer] ERROR: null pointer!");
        return;
    }

    const EyeDefinition& eye = *m_eyeDef;

    // Calculate radii from eye definition
    uint16_t eyeRadius = eyeRadiusPixels(eye);
    uint16_t irisRadius = irisRadiusPixels(eye);
    uint16_t pupilRadius = (uint16_t)(irisRadius * eye.pupil.minFraction);

    int centerX = m_displaySize / 2;
    int centerY = m_displaySize / 2;

    // Apply eye position offset (eyeX, eyeY are normalized -1 to +1)
    int offsetX = (int)(eyeX * (m_displaySize / 4));
    int offsetY = (int)(eyeY * (m_displaySize / 4));

    // Compute bounding box to skip unnecessary pixels
    int minX = centerX + offsetX - eyeRadius;
    int maxX = centerX + offsetX + eyeRadius;
    int minY = centerY + offsetY - eyeRadius;
    int maxY = centerY + offsetY + eyeRadius;

    // Clamp to display bounds
    minX = (minX < 0) ? 0 : minX;
    maxX = (maxX > m_displaySize) ? m_displaySize : maxX;
    minY = (minY < 0) ? 0 : minY;
    maxY = (maxY > m_displaySize) ? m_displaySize : maxY;

    // CO5300 requires EVEN coordinates and dimensions
    // Round down odd start positions, round up odd sizes
    if (minX % 2 != 0) minX--;
    if (minY % 2 != 0) minY--;
    if (maxX % 2 != 0) maxX++;
    if (maxY % 2 != 0) maxY++;

    // Clamp again after even adjustment
    minX = (minX < 0) ? 0 : minX;
    maxX = (maxX > m_displaySize) ? m_displaySize : maxX;
    minY = (minY < 0) ? 0 : minY;
    maxY = (maxY > m_displaySize) ? m_displaySize : maxY;

    // Precompute squared radii
    int eyeRadiusSq = (int)eyeRadius * (int)eyeRadius;
    int irisRadiusSq = (int)irisRadius * (int)irisRadius;
    int pupilRadiusSq = (int)pupilRadius * (int)pupilRadius;

    // First fill entire buffer with background color (fast memset)
    uint16_t bgColor = eye.backColor;
    m_backgroundColor = bgColor;
    int totalPixels = m_displaySize * m_displaySize;
    memset(m_renderBuf, bgColor, totalPixels * sizeof(uint16_t));

    // Only render pixels within bounding box
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int dx = x - centerX - offsetX;
            int dy = y - centerY - offsetY;
            int distSq = dx * dx + dy * dy;

            uint16_t color;
            if (distSq <= eyeRadiusSq) {
                if (distSq <= irisRadiusSq) {
                    if (distSq <= pupilRadiusSq) {
                        color = eye.pupil.color;
                    } else {
                        color = eye.iris.color;
                    }
                } else {
                    color = eye.sclera.color;
                }
                m_renderBuf[y * m_displaySize + x] = color;
            }
        }
    }

    // Software sync using time-based tracking
    if (m_transferInProgress) {
        int64_t elapsed = esp_timer_get_time() - m_lastTransferTime;
        if (elapsed < 43000) {  // 43ms transfer time at 80MHz
            vTaskDelay(1);
        }
        m_transferInProgress = false;
    }

    // Swap buffers
    uint16_t* temp = m_renderBuf;
    m_renderBuf = m_displayBuf;
    m_displayBuf = temp;

    // Synchronous transfer - blocks for ~20ms
    int64_t transferStart = esp_timer_get_time();
    m_display->beginDisplayTransfer();
    m_display->drawRGBBitmap(0, 0, m_displayBuf, m_displaySize, m_displaySize);
    m_display->endDisplayTransfer();
    int64_t transferEnd = esp_timer_get_time();

    m_transferInProgress = true;
    m_lastTransferTime = esp_timer_get_time();

    // Timing stats every 60 frames
    static int64_t minXfer = INT64_MAX;
    static int64_t maxXfer = 0;
    static uint32_t samples = 0;

    int64_t xferTime = transferEnd - transferStart;
    if (xferTime < minXfer) minXfer = xferTime;
    if (xferTime > maxXfer) maxXfer = xferTime;
    samples++;

    if (samples % 60 == 0) {
        Serial.printf("[XFER] %lld us (%lld-%lld) FPS=%.1f\n",
            xferTime, minXfer, maxXfer, 1000000.0 / xferTime);
    }
}