#pragma once

#include <Arduino.h>
#include "common/DisplayHAL.h"

/**
 * @brief Get screen dimensions based on display type.
 *
 * AMOLED boards use 466x466, T-RGB uses 480x480, unknown boards default to 240x240.
 */
#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)
#define SCREEN_WIDTH 466
#define SCREEN_HEIGHT 466
#elif defined(ARDUINO_LILYGO_T_RGB)
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480
#else
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#endif

/**
 * @brief Lookup tables (polarAngle_*, polarDist_*, disp_*) are defined
 * in display/display_466.h or display/display_480.h depending on display type.
 */

/**
 * @brief Pupil/iris configuration.
 */
struct PupilConfig
{
  uint16_t color;
  float slitRadius;  // 0.0 = round pupil; fraction of iris radius for slit half-height
  float minFraction; // min pupil size as fraction of iris radius
  float maxFraction; // max pupil size as fraction of iris radius
};

/**
 * @brief Iris texture configuration.
 */
struct IrisConfig
{
  float radiusFraction; // iris radius as fraction of eye radius
  struct
  {
    const uint16_t *data; // pointer to texture data
    uint16_t width;
    uint16_t height;
  } texture;
  uint16_t color;  // default color if no texture
  uint16_t angle;  // initial rotation (0-1023)
  float spin;      // spin rate (RPM * 1024)
  int16_t iSpin;   // fixed per-frame spin override
  uint16_t mirror; // 0 = normal, 1023 = flip X
};

/**
 * @brief Sclera (white of the eye) configuration.
 */
struct ScleraConfig
{
  struct
  {
    const uint16_t *data;
    uint16_t width;
    uint16_t height;
  } texture;
  uint16_t color;
  uint16_t angle;
  float spin;
  int16_t iSpin;
  uint16_t mirror;
};

/**
 * @brief Eyelid configuration.
 */
struct EyelidConfig
{
  const uint8_t *upper; // upper eyelid lookup table (pairs of startY, endY per column)
  const uint8_t *lower; // lower eyelid lookup table
  uint16_t color;
  float normalClosure; // eyelid coverage at rest (0.0=fully open, 1.0=fully closed)
  float wideClosure;   // eyelid coverage when wide/surprised (0.0=fully retracted, 1.0=fully closed)
  bool tracking;       // eyelids track pupil vertical position
};

/**
 * @brief Polar map info for eye geometry.
 */
struct PolarMapInfo
{
  uint16_t radius;
  const uint8_t *angleMap;
  const uint8_t *distMap;
};

/**
 * @brief Main eye definition structure.
 *
 * Contains all parameters needed to render a complete eye including
 * pupil, iris, sclera, eyelids, and geometry mappings.
 */
struct EyeDefinition
{
  const char *name;
  float radiusFraction; // eye radius as fraction of smaller screen dimension
  uint16_t backColor;
  uint8_t squint;
  const uint8_t *dispMap; // spherical displacement map

  PupilConfig pupil;
  IrisConfig iris;
  ScleraConfig sclera;
  EyelidConfig eyelid;
  PolarMapInfo polarMap;
};

/**
 * @brief Compute actual pixel radius from fraction.
 * @param eye Eye definition reference.
 * @return Eye radius in pixels.
 */
inline uint16_t eyeRadiusPixels(const EyeDefinition &eye)
{
  uint16_t minDim = (SCREEN_WIDTH < SCREEN_HEIGHT) ? SCREEN_WIDTH : SCREEN_HEIGHT;
  return (uint16_t)(eye.radiusFraction * minDim);
}

/**
 * @brief Compute iris radius in pixels.
 * @param eye Eye definition reference.
 * @return Iris radius in pixels.
 */
inline uint16_t irisRadiusPixels(const EyeDefinition &eye)
{
  return (uint16_t)(eyeRadiusPixels(eye) * eye.iris.radiusFraction);
}

/**
 * @brief Macro to declare an eye namespace.
 *
 * Usage: DECLARE_EYE(EyeName) - creates namespace EyeName with extern EyeDefinition eye.
 */
#define DECLARE_EYE(EyeName)        \
  namespace EyeName                 \
  {                                 \
    extern const EyeDefinition eye; \
  }
