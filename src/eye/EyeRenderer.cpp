/**
 * @file EyeRenderer.cpp
 * @brief Core eye rendering engine using precomputed polar maps.
 *
 * Allocates two PSRAM-backed RGB565 frame buffers for double-buffered rendering.
 * Renders the eye column-by-column into the render buffer, then flips to the
 * display buffer for transfer. Handles pupil/iris/sclera coloring per pixel
 * based on squared distance from the eye center, and clips pixels within a
 * circular boundary. Supports both custom eyelid tables from the eye definition
 * and a math-based curved fallback. Enforces even coordinates for CO5300.
 */
#include "EyeRenderer.h"
#include <Arduino.h>
#include <algorithm>
#include <esp_heap_caps.h>

// Include the precomputed polar angle table for the active display.
// These large PROGMEM arrays must be defined in exactly one translation unit;
// keeping the include here (not in a shared header) achieves that.
#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)
#include "display/display_466.h"
static const uint8_t *const s_angleMap   = polarAngle_233;
static constexpr bool        s_hasAngleMap = true;
#elif defined(ARDUINO_LILYGO_T_RGB)
#include "display/display_480.h"
static const uint8_t *const s_angleMap   = polarAngle_240;
static constexpr bool        s_hasAngleMap = true;
#else
static const uint8_t *const s_angleMap   = nullptr;
static constexpr bool        s_hasAngleMap = false;
#endif

// Debug: force eyelids to a fixed gap (0=closed, 100=open, 50=half).
#define DEBUG_EYELID_GAP_OVERRIDE -1 // -1 = disabled, 0-100 = forced percentage (50=half)

EyeRenderer::EyeRenderer()
    : m_display(nullptr), m_displaySize(0), m_mapRadius(0), m_mapDiameter(0), m_eyeDef(nullptr),
      m_frameBuf1(nullptr), m_frameBuf2(nullptr), m_renderBuf(nullptr), m_displayBuf(nullptr),
      m_dirtyMinX(0), m_dirtyMinY(0), m_dirtyMaxX(0), m_dirtyMaxY(0),
      m_eyelidRenderer()
{
}

EyeRenderer::~EyeRenderer()
{
  if (m_frameBuf1)
  {
    heap_caps_free(m_frameBuf1);
    m_frameBuf1 = nullptr;
  }
  if (m_frameBuf2)
  {
    heap_caps_free(m_frameBuf2);
    m_frameBuf2 = nullptr;
  }
  if (m_irisTexCache)
  {
    heap_caps_free(m_irisTexCache);
    m_irisTexCache = nullptr;
  }
  if (m_scleraTexCache)
  {
    heap_caps_free(m_scleraTexCache);
    m_scleraTexCache = nullptr;
  }
  if (m_angleMapCache)
  {
    heap_caps_free(m_angleMapCache);
    m_angleMapCache = nullptr;
  }
  if (m_radiusMapCache)
  {
    heap_caps_free(m_radiusMapCache);
    m_radiusMapCache = nullptr;
  }
}

/**
 * @brief Initialize the renderer with display and eye definition.
 *
 * Allocates two PSRAM buffers (double-buffering) and a scratch buffer for
 * display transfers. Computes display size as min(width, height) and sets up
 * the circular clip bounds. Initializes the EyelidRenderer with tracking if
 * configured in the eye definition.
 */
bool EyeRenderer::begin(DisplayHAL *display, const EyeDefinition &eyeDef)
{
  m_display = display;
  m_eyeDef = &eyeDef;
  m_displaySize = display->getWidth();
  if (display->getHeight() < m_displaySize)
  {
    m_displaySize = display->getHeight();
  }
  m_mapRadius = eyeDef.polarMap.radius;
  m_mapDiameter = m_mapRadius * 2;

  m_eyelidRenderer.begin(m_displaySize, eyeRadiusPixels(eyeDef), eyeDef.eyelid);
  m_eyelidRenderer.setTrackingEnabled(eyeDef.eyelid.tracking);

  size_t bufSize = m_displaySize * m_displaySize * sizeof(uint16_t);
  if (!m_frameBuf1)
    m_frameBuf1 = (uint16_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!m_frameBuf2)
    m_frameBuf2 = (uint16_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!m_frameBuf1 || !m_frameBuf2)
  {
    Serial.println("[EyeRenderer] Failed to allocate double buffers!");
    if (m_frameBuf1)
      heap_caps_free(m_frameBuf1);
    if (m_frameBuf2)
      heap_caps_free(m_frameBuf2);
    m_frameBuf1 = m_frameBuf2 = nullptr;
    return false;
  }

  // Zero both frame buffers so background pixels outside the eye circle are always black.
  memset(m_frameBuf1, 0, bufSize);
  memset(m_frameBuf2, 0, bufSize);

  m_renderBuf = m_frameBuf1;
  m_displayBuf = m_frameBuf2;

  m_dirtyMinX = 0;
  m_dirtyMinY = 0;
  m_dirtyMaxX = m_displaySize;
  m_dirtyMaxY = m_displaySize;
  // Both buffers were just zeroed by memset, so no stale content exists.
  // Initialize each buffer's prevDirty to an empty rect (min == max).
  int mid = m_displaySize / 2;
  m_prevDirtyMinX[0] = m_prevDirtyMinX[1] = mid;
  m_prevDirtyMinY[0] = m_prevDirtyMinY[1] = mid;
  m_prevDirtyMaxX[0] = m_prevDirtyMaxX[1] = mid;
  m_prevDirtyMaxY[0] = m_prevDirtyMaxY[1] = mid;

  Serial.printf("[EyeRenderer] Double buffers allocated: %zu bytes each (PSRAM)\n", bufSize);

  // Cache PROGMEM textures in PSRAM to eliminate flash cache-miss latency in the hot render loop.
  // Flash random-access penalty is ~1500 ns/miss vs ~80 ns/miss for PSRAM — up to 20× speedup.

  // Release any previous caches (called on eye switch).
  if (m_irisTexCache)   { heap_caps_free(m_irisTexCache);   m_irisTexCache   = nullptr; }
  if (m_scleraTexCache) { heap_caps_free(m_scleraTexCache); m_scleraTexCache = nullptr; }
  if (m_angleMapCache)  { heap_caps_free(m_angleMapCache);  m_angleMapCache  = nullptr; }
  if (m_radiusMapCache) { heap_caps_free(m_radiusMapCache); m_radiusMapCache = nullptr; }

  // Allocate in priority order: angle map and radius map first (hot inner-loop lookups),
  // then iris texture, then sclera (largest, most cache-unfriendly — can tolerate PSRAM).
  if (s_hasAngleMap && m_mapRadius > 0)
  {
    size_t sz = (size_t)m_mapRadius * m_mapRadius;

    m_angleMapCache = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const char *angleLoc = "DRAM";
    if (!m_angleMapCache) { m_angleMapCache = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); angleLoc = "PSRAM"; }
    if (m_angleMapCache) { memcpy(m_angleMapCache, s_angleMap, sz); Serial.printf("[EyeRenderer] Angle map cached in %s: %zu bytes\n", angleLoc, sz); }
    else Serial.println("[EyeRenderer] Warning: failed to cache angle map");

    // Radius map: same indexing as angle map. radiusMap[qy*r + qx] = (uint8_t)sqrt(qx²+qy²).
    // Computed at startup (~1ms) so no PROGMEM table is needed.
    m_radiusMapCache = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const char *radiusLoc = "DRAM";
    if (!m_radiusMapCache) { m_radiusMapCache = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); radiusLoc = "PSRAM"; }
    if (m_radiusMapCache)
    {
      for (int qy = 0; qy < m_mapRadius; qy++)
        for (int qx = 0; qx < m_mapRadius; qx++)
        {
          int r = (int)sqrtf((float)(qx * qx + qy * qy));
          m_radiusMapCache[qy * m_mapRadius + qx] = (uint8_t)(r > 255 ? 255 : r);
        }
      Serial.printf("[EyeRenderer] Radius map computed in %s: %zu bytes\n", radiusLoc, sz);
    }
    else Serial.println("[EyeRenderer] Warning: failed to allocate radius map");
  }

  if (eyeDef.iris.texture.data && eyeDef.iris.texture.width > 0 && eyeDef.iris.texture.height > 0)
  {
    size_t sz = (size_t)eyeDef.iris.texture.width * eyeDef.iris.texture.height * sizeof(uint16_t);
    m_irisTexCache = (uint16_t *)heap_caps_malloc(sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const char *irisLoc = "DRAM";
    if (!m_irisTexCache) { m_irisTexCache = (uint16_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); irisLoc = "PSRAM"; }
    if (m_irisTexCache)
    {
      memcpy(m_irisTexCache, eyeDef.iris.texture.data, sz);
      size_t count = sz / sizeof(uint16_t);
      for (size_t i = 0; i < count; i++) m_irisTexCache[i] = __builtin_bswap16(m_irisTexCache[i]);
      Serial.printf("[EyeRenderer] Iris texture cached in %s: %zu bytes\n", irisLoc, sz);
    }
    else Serial.println("[EyeRenderer] Warning: failed to cache iris texture");
  }

  if (eyeDef.sclera.texture.data && eyeDef.sclera.texture.width > 0 && eyeDef.sclera.texture.height > 0)
  {
    size_t sz = (size_t)eyeDef.sclera.texture.width * eyeDef.sclera.texture.height * sizeof(uint16_t);
    m_scleraTexCache = (uint16_t *)heap_caps_malloc(sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const char *scleraLoc = "DRAM";
    if (!m_scleraTexCache) { m_scleraTexCache = (uint16_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); scleraLoc = "PSRAM"; }
    if (m_scleraTexCache)
    {
      memcpy(m_scleraTexCache, eyeDef.sclera.texture.data, sz);
      size_t count = sz / sizeof(uint16_t);
      for (size_t i = 0; i < count; i++) m_scleraTexCache[i] = __builtin_bswap16(m_scleraTexCache[i]);
      Serial.printf("[EyeRenderer] Sclera texture cached in %s: %zu bytes\n", scleraLoc, sz);
    }
    else Serial.println("[EyeRenderer] Warning: failed to cache sclera texture");
  }

  // Scan all eyelid table columns once so renderFrame() avoids the per-frame loop.
  m_hasCustomLids = false;
  if (eyeDef.eyelid.upper != nullptr && eyeDef.eyelid.lower != nullptr)
  {
    for (int col = 0; col < m_displaySize; col++)
    {
      uint8_t upperEnd   = eyeDef.eyelid.upper[col * 2 + 1];
      uint8_t lowerStart = eyeDef.eyelid.lower[col * 2];
      if ((upperEnd != 0 && upperEnd != 255) || (lowerStart != 0 && lowerStart != 255))
      {
        m_hasCustomLids = true;
        break;
      }
    }
  }

  return true;
}

/**
 * @brief Render the complete eye frame into the render buffer.
 *
 * Clears the render buffer with the eye's background color, then iterates
 * each column within the eye's bounding box. For each pixel, determines
 * pupil/iris/sclera color by squared-distance comparison against precomputed
 * radii. Eyelid occlusion is applied either via custom tables or math-based
 * curved boundary. After rendering, swaps render/display buffers and
 * initiates a full-frame display transfer.
 *
 * @param eyeX Normalized eye X position (-1.0 to +1.0).
 * @param eyeY Normalized eye Y position (-1.0 to +1.0).
 * @param pupilFactor Pupil scale (1.0 = full size, smaller = contracted).
 * @param upperLidFactor Upper eyelid open fraction (0.0-1.0).
 * @param lowerLidFactor Lower eyelid open fraction (0.0-1.0).
 * @param blinkFactor Overall blink factor (0.0 = open, 1.0 = closed).
 * @param irisAngle Iris texture rotation (0-1023, mapped to 0-255 angle space).
 * @param scleraAngle Sclera texture rotation (0-1023, mapped to 0-255 angle space).
 */
void EyeRenderer::renderFrame(float eyeX, float eyeY, float pupilFactor,
                              float upperLidFactor, float lowerLidFactor, float blinkFactor,
                              uint16_t irisAngle, uint16_t scleraAngle)
{
  if (!m_display || !m_eyeDef || !m_renderBuf)
  {
    Serial.println("[EyeRenderer] ERROR: null pointer!");
    return;
  }

  const EyeDefinition &eye = *m_eyeDef;

  uint16_t eyeRadius = eyeRadiusPixels(eye);
  uint16_t irisRadius = irisRadiusPixels(eye);
  uint16_t pupilRadius = (uint16_t)(irisRadius * pupilFactor);

  int centerX = m_displaySize / 2;
  int centerY = m_displaySize / 2;

  int offsetX = (int)(eyeX * (m_displaySize / 4));
  int offsetY = (int)(eyeY * (m_displaySize / 4));

  int margin = eyeRadius;
  int minX = centerX + offsetX - margin;
  int maxX = centerX + offsetX + margin;
  int minY = centerY + offsetY - margin;
  int maxY = centerY + offsetY + margin;

  minX = (minX < 0) ? 0 : minX;
  maxX = (maxX > m_displaySize) ? m_displaySize : maxX;
  minY = (minY < 0) ? 0 : minY;
  maxY = (maxY > m_displaySize) ? m_displaySize : maxY;

  if (minX % 2 != 0)
    minX--;
  if (minY % 2 != 0)
    minY--;
  if (maxX % 2 != 0)
    maxX++;
  if (maxY % 2 != 0)
    maxY++;
  if (maxX > m_displaySize)
    maxX = m_displaySize;
  if (maxY > m_displaySize)
    maxY = m_displaySize;

  // Each buffer alternates every other frame, so its last content is from 2 frames ago.
  // Select prevDirty for this specific buffer so we expand the render region correctly.
  int bufIdx = (m_renderBuf == m_frameBuf1) ? 0 : 1;
  int eyeMinX = minX, eyeMinY = minY, eyeMaxX = maxX, eyeMaxY = maxY;
  if (m_prevDirtyMinX[bufIdx] < m_prevDirtyMaxX[bufIdx])
  {
    if (m_prevDirtyMinX[bufIdx] < minX) minX = m_prevDirtyMinX[bufIdx];
    if (m_prevDirtyMinY[bufIdx] < minY) minY = m_prevDirtyMinY[bufIdx];
    if (m_prevDirtyMaxX[bufIdx] > maxX) maxX = m_prevDirtyMaxX[bufIdx];
    if (m_prevDirtyMaxY[bufIdx] > maxY) maxY = m_prevDirtyMaxY[bufIdx];
  }
  m_prevDirtyMinX[bufIdx] = eyeMinX;
  m_prevDirtyMinY[bufIdx] = eyeMinY;
  m_prevDirtyMaxX[bufIdx] = eyeMaxX;
  m_prevDirtyMaxY[bufIdx] = eyeMaxY;

  int eyeRadiusSq = (int)eyeRadius * (int)eyeRadius;
  int irisRadiusSq = (int)irisRadius * (int)irisRadius;
  int pupilRadiusSq = (int)pupilRadius * (int)pupilRadius;

  // Slit pupil: precompute per-frame boundary constants.
  // The boundary at the current pupil opening is a circular arc that passes
  // through (x2, 0) horizontally and (0, y1) vertically, with center on the
  // x-axis. A pixel at (|dx|, dy) is inside the pupil when it falls inside
  // that arc. slitXc and slitRcSq are hoisted out of the column/row loops.
  const bool hasSlit = (eye.pupil.slitRadius > 0.0f && pupilRadius > 0);
  float slitXc = 0.0f, slitRcSq = 0.0f;
  if (hasSlit) {
    float slitPx = eye.pupil.slitRadius * (float)irisRadius; // vertical half-height
    float x2 = (float)pupilRadius;                           // horizontal half-width
    float y1 = slitPx + ((float)irisRadius - slitPx) * x2 / (float)irisRadius;
    slitXc = (x2 * x2 - y1 * y1) / (2.0f * x2);
    float Rc = x2 - slitXc;
    slitRcSq = Rc * Rc;
  }

  // Texture pointers — prefer DRAM cache to avoid flash/PSRAM latency.
  const uint16_t *irisTexData   = m_irisTexCache   ? m_irisTexCache   : eye.iris.texture.data;
  const uint16_t *scleraTexData = m_scleraTexCache ? m_scleraTexCache : eye.sclera.texture.data;
  const int irisTexW  = eye.iris.texture.width;
  const int irisTexH  = eye.iris.texture.height;
  const int scleraTexW = eye.sclera.texture.width;
  const int scleraTexH = eye.sclera.texture.height;
  const bool hasIrisTex  = (s_hasAngleMap && m_mapRadius > 0
                            && eye.iris.texture.data  && irisTexW  > 0 && irisTexH  > 0);
  const bool hasScleraTex = (s_hasAngleMap && m_mapRadius > 0
                             && eye.sclera.texture.data && scleraTexW > 0 && scleraTexH > 0);
  // Angle map and radius map: prefer DRAM cache.
  const uint8_t *angleBase  = m_angleMapCache  ? m_angleMapCache  : s_angleMap;
  const uint8_t *radiusBase = m_radiusMapCache ? m_radiusMapCache : nullptr;

  // Per-frame fixed-point reciprocals — replace integer division in the inner loop.
  // Q16 format: texV = (r * mul) >> 16  gives  r * texH / radius without hardware divide.
  const int scleraWidth = (int)eyeRadius - (int)irisRadius;
  const uint32_t irisTexVMul   = (hasIrisTex  && irisRadius  > 0) ? (((uint32_t)irisTexH  << 16) / irisRadius)                        : 0;
  const uint32_t scleraTexVMul = (hasScleraTex && scleraWidth > 0) ? (((uint32_t)scleraTexH << 16) / (uint32_t)scleraWidth) : 0;
  // irisAngle / scleraAngle are 0-1023; map to 0-255 for 8-bit angle space.
  const uint8_t irisRot   = (uint8_t)(irisAngle  >> 2);
  const uint8_t scleraRot = (uint8_t)(scleraAngle >> 2);

  uint16_t bgColor = eye.backColor;

  // Pre-swap all solid colors to big-endian so the frame buffer is ready for
  // directTransfer → writeBytes, which sends raw bytes directly to the display
  // via PSRAM DMA without a DRAM copy.
  const uint16_t bgColorBE     = __builtin_bswap16(bgColor);
  const uint16_t pupilColorBE  = __builtin_bswap16(eye.pupil.color);
  const uint16_t irisColorBE   = __builtin_bswap16(eye.iris.color);
  const uint16_t scleraColorBE = __builtin_bswap16(eye.sclera.color);

  float eyelidGap = 1.0f - blinkFactor;

#if DEBUG_EYELID_GAP_OVERRIDE >= 0
  eyelidGap = DEBUG_EYELID_GAP_OVERRIDE / 100.0f;
#endif

  int eyeCenterX = centerX + offsetX;
  int eyeCenterY = centerY + offsetY;

  // Prepare eyelid factors before the main loop so row bounds are available for skipping.
  m_eyelidRenderer.prepareFactors(eyeX, eyeY, eyelidGap);
  int upperRow = m_eyelidRenderer.getUpperRow(m_displaySize);
  int lowerRow = m_eyelidRenderer.getLowerRow(m_displaySize);

  static uint32_t s_t0 = 0;
  s_t0 = micros();

  // Row-major loop: fills background pixels and renders eye pixels in a single pass,
  // eliminating the separate std::fill pass that previously double-wrote ~163k pixels
  // to PSRAM (once from fill, once from render). Saves ~40% of total PSRAM writes.
  for (int y = minY; y < maxY; y++)
  {
    uint16_t *rowBuf = m_renderBuf + y * m_displaySize;

    int dy = y - eyeCenterY;
    int dySq = dy * dy;
    int dxMaxSq = eyeRadiusSq - dySq;

    // Compute circle bounds for this row. Default to empty range when outside circle.
    int xCircStart = eyeCenterX;
    int xCircEnd   = eyeCenterX;
    if (dxMaxSq > 0)
    {
      int dxMax = (int)sqrtf((float)dxMaxSq);
      if ((dxMax + 1) * (dxMax + 1) <= dxMaxSq)
        dxMax++;
      xCircStart = eyeCenterX - dxMax;
      xCircEnd   = eyeCenterX + dxMax;
      if (xCircStart < minX) xCircStart = minX;
      if (xCircEnd   > maxX) xCircEnd   = maxX;
    }

    // Fill left gap (outside circle) with background color.
    for (int x = minX; x < xCircStart; x++) rowBuf[x] = bgColorBE;

    // Fill right gap (outside circle) with background color.
    for (int x = xCircEnd; x < maxX; x++) rowBuf[x] = bgColorBE;

    // Eyelid rows: fill circle portion with bgColor so drawEyelids() paints on top.
    if (!m_hasCustomLids && (y <= upperRow || y >= lowerRow))
    {
      for (int x = xCircStart; x < xCircEnd; x++) rowBuf[x] = bgColorBE;
      continue;
    }

    if (xCircStart >= xCircEnd)
      continue;

    // Hoist per-row values for texture lookups.
    int qy = dy < 0 ? -dy : dy;
    if (qy >= m_mapRadius) qy = m_mapRadius - 1;
    const uint8_t *angleRow  = (hasIrisTex || hasScleraTex)
                               ? (angleBase  + (size_t)qy * m_mapRadius) : nullptr;
    const uint8_t *radiusRow = (radiusBase && (hasIrisTex || hasScleraTex))
                               ? (radiusBase + (size_t)qy * m_mapRadius) : nullptr;

    // dy² is constant for the row; hoist for slit-pupil test.
    float slitDySq = hasSlit ? (float)dySq : 0.0f;

    for (int x = xCircStart; x < xCircEnd; x++)
    {
      int dx = x - eyeCenterX;
      int distSq = dx * dx + dySq;

      uint16_t color;

      float slitDdxSq = 0.0f;
      if (hasSlit)
      {
        float px = (float)(dx < 0 ? -dx : dx);
        float ddx = px - slitXc;
        slitDdxSq = ddx * ddx;
      }

      const bool inPupil = hasSlit
          ? (distSq <= irisRadiusSq && slitDdxSq + slitDySq <= slitRcSq)
          : (distSq <= pupilRadiusSq);

      if (inPupil)
      {
        color = pupilColorBE;
      }
      else if (distSq <= irisRadiusSq)
      {
        if (hasIrisTex)
        {
          int qx = dx < 0 ? -dx : dx;
          if (qx >= m_mapRadius) qx = m_mapRadius - 1;
          uint8_t ta = angleRow[qx];

          // Reconstruct full 0-255 CW angle from north using quadrant symmetry.
          // Table encodes Q1 (dx≥0,dy≥0): ta=0 at south, ta=127 at east.
          uint8_t fullAngle;
          if      (dx >= 0 && dy > 0) fullAngle = (uint8_t)(128 - (ta >> 1)); // SE
          else if (dx <  0 && dy > 0) fullAngle = (uint8_t)(128 + (ta >> 1)); // SW
          else if (dx <  0)           fullAngle = (uint8_t)(     - (ta >> 1)); // NW
          else                        fullAngle = (uint8_t)(       (ta >> 1)); // NE

          int r = radiusRow ? (int)radiusRow[qx] : (int)sqrtf((float)distSq);
          int texU = (uint8_t)(fullAngle + irisRot) * irisTexW / 256;
          int texV = (int)(((uint32_t)r * irisTexVMul) >> 16);
          if (texV >= irisTexH) texV = irisTexH - 1;
          color = irisTexData[texU * irisTexH + texV];
        }
        else
        {
          color = irisColorBE;
        }
      }
      else
      {
        if (hasScleraTex)
        {
          int qx = dx < 0 ? -dx : dx;
          if (qx >= m_mapRadius) qx = m_mapRadius - 1;
          uint8_t ta = angleRow[qx];

          uint8_t fullAngle;
          if      (dx >= 0 && dy > 0) fullAngle = (uint8_t)(128 - (ta >> 1));
          else if (dx <  0 && dy > 0) fullAngle = (uint8_t)(128 + (ta >> 1));
          else if (dx <  0)           fullAngle = (uint8_t)(     - (ta >> 1));
          else                        fullAngle = (uint8_t)(       (ta >> 1));

          int r = radiusRow ? (int)radiusRow[qx] : (int)sqrtf((float)distSq);
          int texU = (uint8_t)(fullAngle + scleraRot) * scleraTexW / 256;
          int rv = r - irisRadius;
          if (rv < 0) rv = 0;
          int texV = (int)(((uint32_t)rv * scleraTexVMul) >> 16);
          if (texV >= scleraTexH) texV = scleraTexH - 1;
          color = scleraTexData[texU * scleraTexH + texV];
        }
        else
        {
          color = scleraColorBE;
        }
      }
      rowBuf[x] = color;
    }
  }

  m_eyelidRenderer.drawEyelids(m_renderBuf);

  uint32_t t1 = micros();

  m_dirtyMinX = eyeMinX;
  m_dirtyMinY = eyeMinY;
  m_dirtyMaxX = eyeMaxX;
  m_dirtyMaxY = eyeMaxY;

  uint16_t *temp = m_renderBuf;
  m_renderBuf = m_displayBuf;
  m_displayBuf = temp;

  // Frame buffer is pre-byte-swapped (big-endian) — use directTransfer → writeBytes
  // to send via PSRAM DMA without the DRAM copy that writePixels requires.
  m_display->directTransfer(m_displayBuf, 0, 0, 0, 0, m_displaySize, m_displaySize);

  uint32_t t2 = micros();

  static uint32_t s_lastTimingPrint = 0;
  static uint32_t s_renderUs = 0, s_transferUs = 0;
  s_renderUs   = t1 - s_t0;
  s_transferUs = t2 - t1;
  if (millis() - s_lastTimingPrint > 2000)
  {
    Serial.printf("[Timing] render=%uus transfer=%uus total=%uus (~%u FPS)\n",
                  s_renderUs, s_transferUs, s_renderUs + s_transferUs,
                  (s_renderUs + s_transferUs) > 0 ? 1000000u / (s_renderUs + s_transferUs) : 0);
    s_lastTimingPrint = millis();
  }
}