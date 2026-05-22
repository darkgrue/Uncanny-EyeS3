#pragma once

#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)

#include "eyes/default_eye_466.h"
#include "eyes/eagle_466.h"
#include "eyes/human_eye_466.h"

static const EyeDefinition *const s_eyeRegistry[] = {
    &default_eye::eye,
    &eagle::eye,
    &human_eye::eye};

static constexpr int s_eyeCount = 3;

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

static const EyeDefinition *const s_eyeRegistry[] = {
    &default_eye::eye,
    &eagle::eye,
    &human_eye::eye};

static constexpr int s_eyeCount = 3;

inline const char *getEyeName(int index)
{
  if (index < 0 || index >= s_eyeCount)
    return nullptr;
  return s_eyeRegistry[index]->name;
}

#endif