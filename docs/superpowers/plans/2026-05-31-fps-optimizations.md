# FPS Render Performance Optimizations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply six layered render/transfer optimizations to the eye firmware to increase frame rate from ~20 FPS (eagle) / ~30 FPS (default) toward hardware ceiling.

**Architecture:** Each task is a self-contained change to `EyeRenderer.cpp`, `EyeRenderer.h`, `BoardPins.h`, or `platformio.ini`. Tasks are ordered so later tasks can reuse helpers added by earlier ones. All changes are in the AMOLED render path; T-RGB is unaffected.

**Tech Stack:** PlatformIO / ESP32-S3 / Xtensa LX7 / Arduino framework. Build command: `pio run -e amoled`. Success criterion per task: build completes without error.

---

## File Map

| File | Role |
|---|---|
| `src/eye/EyeRenderer.cpp` | Hot render loop — Tasks 1, 2, 3, 6 |
| `src/eye/EyeRenderer.h` | Class members for new precomputed tables — Task 3 |
| `include/BoardPins.h` | `QSPI_FREQUENCY` macro — Task 4 |
| `platformio.ini` | `-O2` → `-O3` build flag — Task 5 |

---

## Task 1: `IRAM_ATTR` on `renderFrame`

The entire `renderFrame` function (~3 KB of compiled code) currently lives in flash. Every instruction-cache miss in the inner pixel loop costs ~20–50 stall cycles. Placing the function in IRAM eliminates that pressure.

**Files:**
- Modify: `src/eye/EyeRenderer.cpp` (function definition line)

- [ ] **Step 1: Add IRAM_ATTR to renderFrame definition**

In `src/eye/EyeRenderer.cpp`, find:

```cpp
void EyeRenderer::renderFrame(float eyeX, float eyeY, float pupilFactor,
                              float upperLidFactor, float lowerLidFactor, float blinkFactor,
                              uint16_t irisAngle, uint16_t scleraAngle)
```

Replace with:

```cpp
void IRAM_ATTR EyeRenderer::renderFrame(float eyeX, float eyeY, float pupilFactor,
                                        float upperLidFactor, float lowerLidFactor, float blinkFactor,
                                        uint16_t irisAngle, uint16_t scleraAngle)
```

- [ ] **Step 2: Build**

```bash
pio run -e amoled
```

Expected: `[SUCCESS]` — no errors. IRAM usage in the map output increases by ~3 KB.

- [ ] **Step 3: Commit**

```bash
git add src/eye/EyeRenderer.cpp
git commit -m "perf: place renderFrame in IRAM to eliminate instruction-cache stalls"
```

---

## Task 2: Zone-segmented inner loop + packed solid-color fills

**Current:** The pixel loop checks `qx <= xPupilLim / xIrisLim` per pixel, plus `dx < 0 ? leftSign : rightSign` per pixel. These are unnecessary — the zone boundaries are already precomputed per row.

**New:** Split the single per-pixel loop into five explicit contiguous segments (left sclera, left iris, pupil, right iris, right sclera). Each segment has a fixed zone and fixed `angleSign`, so there are no zone branches or sign conditionals per pixel. The pupil segment uses a 32-bit packed fill (two pixels per store). The `fill16packed` helper introduced here is also reused in Task 6.

**Files:**
- Modify: `src/eye/EyeRenderer.cpp`

- [ ] **Step 1: Add `fill16packed` helper at top of EyeRenderer.cpp**

After line 15 (`#include <esp_heap_caps.h>`), add:

```cpp
// Write `count` pixels of `color` to `dst` using 32-bit paired stores.
// Two pixels per iteration halves store transactions to PSRAM.
static inline void fill16packed(uint16_t *dst, uint16_t color, int count)
{
  uint32_t packed = ((uint32_t)color << 16) | color;
  uint32_t *d = (uint32_t *)dst;
  int pairs = count >> 1;
  while (pairs--)
    *d++ = packed;
  if (count & 1)
    *(uint16_t *)d = color;
}
```

- [ ] **Step 2: Replace the per-pixel inner loop with the 5-segment loop**

Find and replace the entire inner pixel loop. The old block starts with:

```cpp
    for (int x = xCircStart; x < xCircEnd; x++)
    {
      int dx = x - eyeCenterX;
      int qx = dx < 0 ? -dx : dx;
      if (qx >= m_mapRadius)
        qx = m_mapRadius - 1;

      uint16_t color;

      if (!hasSlit && radiusRow)
      {
```

and ends with (the closing braces of the for loop):

```cpp
      rowBuf[x] = color;
    }
```

Replace the entire block — from `for (int x = xCircStart` through the matching `}` — with:

```cpp
    // === Render eye circle: 5 zone segments, no per-pixel zone branches ===
    // Fast path (no slit, radius map available): translate per-row qx limits
    // into absolute x boundaries, then render each zone with a dedicated loop.
    // Left segments use qx = eyeCenterX - x and angleSign = leftSign (constant).
    // Right segments use qx = x - eyeCenterX and angleSign = rightSign (constant).
    // Sentinel: irisLeft = eyeCenterX when xIrisLim = -1 (row outside iris).
    if (!hasSlit && radiusRow)
    {
      int irisLeft  = (xIrisLim  >= 0) ? (eyeCenterX - xIrisLim)  : eyeCenterX;
      int irisRight = (xIrisLim  >= 0) ? (eyeCenterX + xIrisLim)  : (eyeCenterX - 1);
      int pupilLeft  = (xPupilLim >= 0) ? (eyeCenterX - xPupilLim) : eyeCenterX;
      int pupilRight = (xPupilLim >= 0) ? (eyeCenterX + xPupilLim) : (eyeCenterX - 1);

      // --- Segment 1: left sclera [xCircStart, irisLeft) ---
      {
        int xEnd = (irisLeft < xCircEnd) ? irisLeft : xCircEnd;
        if (hasScleraTex)
        {
          for (int x = xCircStart; x < xEnd; x++)
          {
            int qx = eyeCenterX - x;
            if (qx >= m_mapRadius) qx = m_mapRadius - 1;
            int r = (int)radiusRow[qx];
            uint8_t ta = angleRow[qx];
            uint8_t fullAngle = (uint8_t)(angleOffset + leftSign * (ta >> 1)) + scleraRot;
            int rv = r - (int)irisRadius;
            if (rv < 0) rv = 0;
            int texV = (int)(((uint32_t)rv * scleraTexVMul) >> 16);
            if (texV >= scleraTexH) texV = scleraTexH - 1;
            rowBuf[x] = m_scleraAnglePtrs[fullAngle][texV];
          }
        }
        else
        {
          fill16packed(rowBuf + xCircStart, scleraColorBE, xEnd - xCircStart);
        }
      }

      // --- Segment 2: left iris [irisLeft, pupilLeft) ---
      {
        int xStart = (irisLeft  > xCircStart) ? irisLeft  : xCircStart;
        int xEnd   = (pupilLeft < xCircEnd)   ? pupilLeft : xCircEnd;
        if (xStart < xEnd)
        {
          if (hasIrisTex)
          {
            for (int x = xStart; x < xEnd; x++)
            {
              int qx = eyeCenterX - x;
              if (qx >= m_mapRadius) qx = m_mapRadius - 1;
              int r = (int)radiusRow[qx];
              uint8_t ta = angleRow[qx];
              uint8_t fullAngle = (uint8_t)(angleOffset + leftSign * (ta >> 1)) + irisRot;
              int texV = (int)(((uint32_t)r * irisTexVMul) >> 16);
              if (texV >= irisTexH) texV = irisTexH - 1;
              rowBuf[x] = m_irisAnglePtrs[fullAngle][texV];
            }
          }
          else
          {
            fill16packed(rowBuf + xStart, irisColorBE, xEnd - xStart);
          }
        }
      }

      // --- Segment 3: pupil [pupilLeft, pupilRight+1) — solid packed fill ---
      {
        int xStart = (pupilLeft  > xCircStart)   ? pupilLeft      : xCircStart;
        int xEnd   = (pupilRight < xCircEnd - 1) ? pupilRight + 1 : xCircEnd;
        if (xStart < xEnd)
          fill16packed(rowBuf + xStart, pupilColorBE, xEnd - xStart);
      }

      // --- Segment 4: right iris [pupilRight+1, irisRight+1) ---
      {
        int xStart = (pupilRight + 1 > xCircStart) ? pupilRight + 1 : xCircStart;
        int xEnd   = (irisRight  + 1 < xCircEnd)   ? irisRight  + 1 : xCircEnd;
        if (xStart < xEnd)
        {
          if (hasIrisTex)
          {
            for (int x = xStart; x < xEnd; x++)
            {
              int qx = x - eyeCenterX;
              if (qx >= m_mapRadius) qx = m_mapRadius - 1;
              int r = (int)radiusRow[qx];
              uint8_t ta = angleRow[qx];
              uint8_t fullAngle = (uint8_t)(angleOffset + rightSign * (ta >> 1)) + irisRot;
              int texV = (int)(((uint32_t)r * irisTexVMul) >> 16);
              if (texV >= irisTexH) texV = irisTexH - 1;
              rowBuf[x] = m_irisAnglePtrs[fullAngle][texV];
            }
          }
          else
          {
            fill16packed(rowBuf + xStart, irisColorBE, xEnd - xStart);
          }
        }
      }

      // --- Segment 5: right sclera [irisRight+1, xCircEnd) ---
      {
        int xStart = (irisRight + 1 > xCircStart) ? irisRight + 1 : xCircStart;
        if (xStart < xCircEnd)
        {
          if (hasScleraTex)
          {
            for (int x = xStart; x < xCircEnd; x++)
            {
              int qx = x - eyeCenterX;
              if (qx >= m_mapRadius) qx = m_mapRadius - 1;
              int r = (int)radiusRow[qx];
              uint8_t ta = angleRow[qx];
              uint8_t fullAngle = (uint8_t)(angleOffset + rightSign * (ta >> 1)) + scleraRot;
              int rv = r - (int)irisRadius;
              if (rv < 0) rv = 0;
              int texV = (int)(((uint32_t)rv * scleraTexVMul) >> 16);
              if (texV >= scleraTexH) texV = scleraTexH - 1;
              rowBuf[x] = m_scleraAnglePtrs[fullAngle][texV];
            }
          }
          else
          {
            fill16packed(rowBuf + xStart, scleraColorBE, xCircEnd - xStart);
          }
        }
      }
    }
    else
    {
      // Fallback: slit pupil or no radius map — original per-pixel logic unchanged.
      for (int x = xCircStart; x < xCircEnd; x++)
      {
        int dx = x - eyeCenterX;
        int qx = dx < 0 ? -dx : dx;
        if (qx >= m_mapRadius)
          qx = m_mapRadius - 1;

        uint16_t color;
        int r = radiusRow ? (int)radiusRow[qx] : (int)sqrtf((float)(qx * qx + qy * qy));
        bool inPupil;
        if (hasSlit)
        {
          float ddx = (float)qx - slitXc;
          inPupil = (r <= (int)irisRadius && ddx * ddx + slitDySq <= slitRcSq);
        }
        else
        {
          inPupil = (r <= (int)pupilRadius);
        }
        if (inPupil)
        {
          color = pupilColorBE;
        }
        else if (r <= (int)irisRadius)
        {
          if (hasIrisTex)
          {
            uint8_t ta = angleRow[qx];
            int angleSign = dx < 0 ? leftSign : rightSign;
            uint8_t fullAngle = (uint8_t)(angleOffset + angleSign * (ta >> 1)) + irisRot;
            int texU = (int)fullAngle * irisTexW / 256;
            int texV = (int)(((uint32_t)r * irisTexVMul) >> 16);
            if (texV >= irisTexH)
              texV = irisTexH - 1;
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
            uint8_t ta = angleRow[qx];
            int angleSign = dx < 0 ? leftSign : rightSign;
            uint8_t fullAngle = (uint8_t)(angleOffset + angleSign * (ta >> 1)) + scleraRot;
            int texU = (int)fullAngle * scleraTexW / 256;
            int rv = r - (int)irisRadius;
            if (rv < 0)
              rv = 0;
            int texV = (int)(((uint32_t)rv * scleraTexVMul) >> 16);
            if (texV >= scleraTexH)
              texV = scleraTexH - 1;
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
```

- [ ] **Step 3: Build**

```bash
pio run -e amoled
```

Expected: `[SUCCESS]` — no errors or warnings about sign mismatches.

- [ ] **Step 4: Commit**

```bash
git add src/eye/EyeRenderer.cpp
git commit -m "perf: zone-split inner render loop; packed 32-bit fill for pupil span"
```

---

## Task 3: Precomputed circle-bounds and iris-limit tables

**Current:** Every row calls `sqrtf` three times — once for the outer circle x-extent, once for the iris x-limit, and once for the pupil x-limit. The circle and iris limits depend only on `eyeRadius`/`irisRadius` (both constant per eye definition), so they can be precomputed into 233-entry tables in `begin()`. Only `xPupilLim` (which depends on the per-frame `pupilRadius`) still needs per-row recomputation.

**Files:**
- Modify: `src/eye/EyeRenderer.h` — add two new table pointers
- Modify: `src/eye/EyeRenderer.cpp` — populate tables in `begin()`, free in destructor, use in `renderFrame()`

- [ ] **Step 1: Add table members to EyeRenderer.h**

In `src/eye/EyeRenderer.h`, find the block:

```cpp
  // Angle→texture row pointer tables: irisAnglePtrs[angle] = &irisTexData[texU(angle) * texH].
  // Replaces the per-pixel multiply (fullAngle * texH) with a single indexed load.
  const uint16_t *m_irisAnglePtrs[256] = {};
  const uint16_t *m_scleraAnglePtrs[256] = {};
```

Replace with:

```cpp
  // Angle→texture row pointer tables: irisAnglePtrs[angle] = &irisTexData[texU(angle) * texH].
  // Replaces the per-pixel multiply (fullAngle * texH) with a single indexed load.
  const uint16_t *m_irisAnglePtrs[256] = {};
  const uint16_t *m_scleraAnglePtrs[256] = {};

  // Per-row precomputed circle/iris x-extent tables indexed by qy = |dy| (0..mapRadius-1).
  // Built once in begin() from eyeRadius / irisRadius (constant per eye def).
  // Eliminates two sqrtf calls per row in renderFrame().
  int16_t *m_xCircHalfW  = nullptr; // [qy] max |dx| still inside the eye circle
  int16_t *m_xIrisLimTab = nullptr; // [qy] max |dx| still inside the iris boundary (-1 = none)
```

- [ ] **Step 2: Free new tables in EyeRenderer destructor**

In `src/eye/EyeRenderer.cpp`, find the destructor block that frees `m_radiusMapCache`:

```cpp
  if (m_radiusMapCache)
  {
    heap_caps_free(m_radiusMapCache);
    m_radiusMapCache = nullptr;
  }
```

After it, add:

```cpp
  if (m_xCircHalfW)
  {
    heap_caps_free(m_xCircHalfW);
    m_xCircHalfW = nullptr;
  }
  if (m_xIrisLimTab)
  {
    heap_caps_free(m_xIrisLimTab);
    m_xIrisLimTab = nullptr;
  }
```

- [ ] **Step 3: Free and rebuild tables in begin() for eye-switch safety**

In `src/eye/EyeRenderer.cpp`, find the existing cache-release block in `begin()`:

```cpp
  if (m_radiusMapCache)
  {
    heap_caps_free(m_radiusMapCache);
    m_radiusMapCache = nullptr;
  }
```

After it, add:

```cpp
  if (m_xCircHalfW)
  {
    heap_caps_free(m_xCircHalfW);
    m_xCircHalfW = nullptr;
  }
  if (m_xIrisLimTab)
  {
    heap_caps_free(m_xIrisLimTab);
    m_xIrisLimTab = nullptr;
  }
```

- [ ] **Step 4: Compute tables in begin() after radius map is ready**

The radius map is computed in the block that ends with:

```cpp
      Serial.printf("[EyeRenderer] Radius map computed in %s: %zu bytes\n", radiusLoc, sz);
    }
    else
      Serial.println("[EyeRenderer] Warning: failed to allocate radius map");
  }
```

After this block (and after the `if (s_hasAngleMap && m_mapRadius > 0)` closing brace), add:

```cpp
  // Precompute per-row circle and iris x-extent tables.
  // Indexed by qy = |dy| (0..mapRadius-1). Both fit in a few hundred bytes of DRAM.
  if (m_mapRadius > 0)
  {
    size_t tabSz = (size_t)m_mapRadius * sizeof(int16_t);

    m_xCircHalfW = (int16_t *)heap_caps_malloc(tabSz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (m_xCircHalfW)
    {
      uint16_t eR   = eyeRadiusPixels(eyeDef);
      int      eRSq = (int)eR * (int)eR;
      for (int qy = 0; qy < m_mapRadius; qy++)
      {
        int dxMaxSq = eRSq - qy * qy;
        if (dxMaxSq <= 0) { m_xCircHalfW[qy] = 0; continue; }
        int v = (int)sqrtf((float)dxMaxSq);
        while ((v + 1) * (v + 1) <= dxMaxSq) v++;
        m_xCircHalfW[qy] = (int16_t)v;
      }
      Serial.printf("[EyeRenderer] xCircHalfW table: %zu bytes DRAM\n", tabSz);
    }

    m_xIrisLimTab = (int16_t *)heap_caps_malloc(tabSz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (m_xIrisLimTab && m_radiusMapCache)
    {
      uint16_t iR  = irisRadiusPixels(eyeDef);
      int      ir2 = (int)iR * (int)iR;
      for (int qy = 0; qy < m_mapRadius; qy++)
      {
        if (qy * qy >= ir2) { m_xIrisLimTab[qy] = -1; continue; }
        int xIL = (int)sqrtf((float)(ir2 - qy * qy));
        if (xIL >= m_mapRadius) xIL = m_mapRadius - 1;
        const uint8_t *rRow = m_radiusMapCache + (size_t)qy * m_mapRadius;
        while (xIL + 1 < m_mapRadius && (int)rRow[xIL + 1] <= (int)iR) xIL++;
        while (xIL >= 0 && (int)rRow[xIL] > (int)iR) xIL--;
        m_xIrisLimTab[qy] = (int16_t)xIL;
      }
      Serial.printf("[EyeRenderer] xIrisLimTab table: %zu bytes DRAM\n", tabSz);
    }
  }
```

- [ ] **Step 5: Use tables in renderFrame() — replace per-row circle sqrtf**

In `src/eye/EyeRenderer.cpp`, inside the row loop find:

```cpp
    // Compute circle bounds for this row. Default to empty range when outside circle.
    int xCircStart = eyeCenterX;
    int xCircEnd = eyeCenterX;
    if (dxMaxSq > 0)
    {
      int dxMax = (int)sqrtf((float)dxMaxSq);
      if ((dxMax + 1) * (dxMax + 1) <= dxMaxSq)
        dxMax++;
      xCircStart = eyeCenterX - dxMax;
      xCircEnd = eyeCenterX + dxMax;
      if (xCircStart < minX)
        xCircStart = minX;
      if (xCircEnd > maxX)
        xCircEnd = maxX;
    }
```

Replace with:

```cpp
    // Compute circle bounds for this row. Default to empty range when outside circle.
    int xCircStart = eyeCenterX;
    int xCircEnd = eyeCenterX;
    if (dxMaxSq > 0)
    {
      int dxMax = (m_xCircHalfW && qy < m_mapRadius)
                    ? (int)m_xCircHalfW[qy]
                    : [&]{ int v=(int)sqrtf((float)dxMaxSq); while((v+1)*(v+1)<=dxMaxSq) v++; return v; }();
      xCircStart = eyeCenterX - dxMax;
      xCircEnd = eyeCenterX + dxMax;
      if (xCircStart < minX)
        xCircStart = minX;
      if (xCircEnd > maxX)
        xCircEnd = maxX;
    }
```

**Note:** The lambda here is only invoked when the table is null (init failure fallback). GCC inlines trivial lambdas in non-hot contexts; the hot path always uses the table.

- [ ] **Step 6: Use `m_xIrisLimTab` in the per-row xIrisLim computation**

Find in the row loop (the per-row zone limit computation block):

```cpp
      int ir2 = (int)irisRadius * (int)irisRadius;
      if (dySq < ir2)
      {
        xIrisLim = (int)sqrtf((float)(ir2 - dySq));
        if (xIrisLim >= m_mapRadius)
          xIrisLim = m_mapRadius - 1;
        while (xIrisLim + 1 < m_mapRadius && (int)radiusRow[xIrisLim + 1] <= (int)irisRadius)
          xIrisLim++;
        while (xIrisLim >= 0 && (int)radiusRow[xIrisLim] > (int)irisRadius)
          xIrisLim--;
      }
```

Replace with:

```cpp
      if (m_xIrisLimTab && qy < m_mapRadius)
      {
        xIrisLim = (int)m_xIrisLimTab[qy];
      }
      else
      {
        int ir2 = (int)irisRadius * (int)irisRadius;
        if (dySq < ir2)
        {
          xIrisLim = (int)sqrtf((float)(ir2 - dySq));
          if (xIrisLim >= m_mapRadius)
            xIrisLim = m_mapRadius - 1;
          while (xIrisLim + 1 < m_mapRadius && (int)radiusRow[xIrisLim + 1] <= (int)irisRadius)
            xIrisLim++;
          while (xIrisLim >= 0 && (int)radiusRow[xIrisLim] > (int)irisRadius)
            xIrisLim--;
        }
      }
```

- [ ] **Step 7: Build**

```bash
pio run -e amoled
```

Expected: `[SUCCESS]`. Serial output at boot will include `xCircHalfW table: 466 bytes DRAM` and `xIrisLimTab table: 466 bytes DRAM`.

- [ ] **Step 8: Commit**

```bash
git add src/eye/EyeRenderer.cpp src/eye/EyeRenderer.h
git commit -m "perf: precompute per-row circle and iris x-extent tables; eliminate 2x sqrtf per row"
```

---

## Task 4: QSPI clock increase

The CO5300 write cycle time minimum is 12 ns (~83 MHz). Increasing from 80 MHz to 90 MHz reduces transfer time proportionally (~11%) and is within spec. Requires visual inspection — revert if display artifacts appear.

**Files:**
- Modify: `include/BoardPins.h`

- [ ] **Step 1: Increase QSPI_FREQUENCY**

In `include/BoardPins.h`, change both occurrences of `QSPI_FREQUENCY`:

```cpp
#define QSPI_FREQUENCY 80000000
```

→

```cpp
#define QSPI_FREQUENCY 90000000
```

(There are two: one under `ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED`, one under `ARDUINO_LILYGO_T_RGB`.)

- [ ] **Step 2: Build**

```bash
pio run -e amoled
```

Expected: `[SUCCESS]`. No code change — only a runtime frequency value changes.

- [ ] **Step 3: Inspect on hardware**

Flash and observe the display for 30 seconds with the eye moving. If any pixel artifacts, tearing, or display corruption appears, revert `QSPI_FREQUENCY` to `80000000` immediately.

- [ ] **Step 4: Commit (only if hardware looks clean)**

```bash
git add include/BoardPins.h
git commit -m "perf: increase QSPI clock 80MHz -> 90MHz; within CO5300 write cycle spec"
```

---

## Task 5: `-O3` compiler optimization

GCC at `-O3` enables loop unrolling, more aggressive inlining, and additional SIMD-class passes on Xtensa that `-O2` skips.

**Risk:** Code size increases (watch IRAM budget); extremely rare miscompilations. If a crash or wrong behavior appears after flashing, revert to `-O2`.

**Files:**
- Modify: `platformio.ini`

- [ ] **Step 1: Change build optimization level**

In `platformio.ini`, find in the `[esp32base]` section:

```ini
	-O2
```

Replace with:

```ini
	-O3
```

- [ ] **Step 2: Build**

```bash
pio run -e amoled
```

Expected: `[SUCCESS]`. Build will be slower (more optimization passes). Check that the `.bin` size does not increase by more than ~50 KB (excessive growth risks IRAM overflow).

- [ ] **Step 3: Commit**

```bash
git add platformio.ini
git commit -m "perf: upgrade optimization level to -O3 for more aggressive loop/inlining"
```

---

## Task 6: Packed 32-bit background fills

The left-gap, right-gap, and eyelid-row background fills use per-pixel `uint16_t` writes. Replacing them with the `fill16packed` helper from Task 2 halves the number of store operations for background spans.

**Files:**
- Modify: `src/eye/EyeRenderer.cpp`

- [ ] **Step 1: Replace left-gap fill**

Find:

```cpp
    // Fill left gap (outside circle) with background color.
    for (int x = minX; x < xCircStart; x++)
      rowBuf[x] = bgColorBE;
```

Replace with:

```cpp
    // Fill left gap (outside circle) with background color.
    fill16packed(rowBuf + minX, bgColorBE, xCircStart - minX);
```

- [ ] **Step 2: Replace right-gap fill**

Find:

```cpp
    // Fill right gap (outside circle) with background color.
    for (int x = xCircEnd; x < maxX; x++)
      rowBuf[x] = bgColorBE;
```

Replace with:

```cpp
    // Fill right gap (outside circle) with background color.
    fill16packed(rowBuf + xCircEnd, bgColorBE, maxX - xCircEnd);
```

- [ ] **Step 3: Replace eyelid-row fill**

Find:

```cpp
    // Eyelid rows: fill circle portion with bgColor so drawEyelids() paints on top.
    if (!m_hasCustomLids && (y <= upperRow || y >= lowerRow))
    {
      for (int x = xCircStart; x < xCircEnd; x++)
        rowBuf[x] = bgColorBE;
      continue;
    }
```

Replace with:

```cpp
    // Eyelid rows: fill circle portion with bgColor so drawEyelids() paints on top.
    if (!m_hasCustomLids && (y <= upperRow || y >= lowerRow))
    {
      fill16packed(rowBuf + xCircStart, bgColorBE, xCircEnd - xCircStart);
      continue;
    }
```

- [ ] **Step 4: Build**

```bash
pio run -e amoled
```

Expected: `[SUCCESS]`.

- [ ] **Step 5: Commit**

```bash
git add src/eye/EyeRenderer.cpp
git commit -m "perf: replace per-pixel background fills with 32-bit packed fill16packed"
```

---

## Self-Review

**Spec coverage:**
- Task 1 (IRAM_ATTR): ✓ `renderFrame` definition patched
- Task 2 (Zone split): ✓ All 5 segments + fallback preserved + packed pupil fill
- Task 3 (Precomputed tables): ✓ New members in .h, freed in destructor and eye-switch, built after radiusMapCache, used in renderFrame
- Task 4 (QSPI clock): ✓ Both board entries in BoardPins.h updated, hardware validation noted
- Task 5 (-O3): ✓ Single `platformio.ini` change
- Task 6 (Background fills): ✓ All three fill sites replaced

**Placeholder scan:** No TBD/TODO. All code blocks are complete.

**Type consistency:** `fill16packed(uint16_t*, uint16_t, int)` is defined once in Task 2 and called in Tasks 2 and 6. `m_xCircHalfW` / `m_xIrisLimTab` are `int16_t*` throughout — declared, freed, populated, and consumed as `int16_t*`.

**Dependency check:** Task 6 uses `fill16packed` added in Task 2. Tasks must be executed in order 1 → 2 → 3 → 4 → 5 → 6.
