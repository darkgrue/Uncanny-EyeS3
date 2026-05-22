#pragma once

#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)

#include "eyes/default_eye_466.h"
#include "eyes/eagle_466.h"
#include "eyes/human_eye_466.h"

/**
 * @brief Registry of available eye definitions for AMOLED boards.
 */
static const EyeDefinition *const s_eyeRegistry[] = {
    &default_eye::eye,
    &eagle::eye,
    &human_eye::eye};

/**
 * @brief Number of eyes in the registry.
 */
static constexpr int s_eyeCount = 3;

/**
 * @brief Get the name of an eye by index.
 * @param index Eye index (0 to s_eyeCount-1).
 * @return Eye name string, or nullptr if index is out of range.
 */
inline const char *getEyeName(int index)
{
  if (index < 0 || index >= s_eyeCount)
    return nullptr;
  return s_eyeRegistry[index]->name;
}

#elif defined(ARDUINO_LILYGO_T_RGB)

#include "eyes/default_eye_480.h"
#include "eyes/eagle_480.h"
#include "eyes/human_eye_480.h"

/**
 * @brief Registry of available eye definitions for T-RGB boards.
 */
static const EyeDefinition *const s_eyeRegistry[] = {
    &default_eye::eye,
    &eagle::eye,
    &human_eye::eye};

/**
 * @brief Number of eyes in the registry.
 */
static constexpr int s_eyeCount = 3;

/**
 * @brief Get the name of an eye by index.
 * @param index Eye index (0 to s_eyeCount-1).
 * @return Eye name string, or nullptr if index is out of range.
 */
inline const char *getEyeName(int index)
{
  if (index < 0 || index >= s_eyeCount)
    return nullptr;
  return s_eyeRegistry[index]->name;
}

#endif