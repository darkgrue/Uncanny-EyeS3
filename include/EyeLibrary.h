#pragma once

#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)

#include "default_eye_466.h"
#include "eagle_466.h"

static const EyeDefinition* const s_eyeRegistry[] = {
    &default_eye::eye,
    &eagle::eye
};

static constexpr int s_eyeCount = 2;

inline const char* getEyeName(int index) {
    if (index < 0 || index >= s_eyeCount) return nullptr;
    return s_eyeRegistry[index]->name;
}

#elif defined(ARDUINO_LILYGO_T_RGB)

#include "default_eye_480.h"
#include "eagle_480.h"

static const EyeDefinition* const s_eyeRegistry[] = {
    &default_eye::eye,
    &eagle::eye
};

static constexpr int s_eyeCount = 2;

inline const char* getEyeName(int index) {
    if (index < 0 || index >= s_eyeCount) return nullptr;
    return s_eyeRegistry[index]->name;
}

#endif