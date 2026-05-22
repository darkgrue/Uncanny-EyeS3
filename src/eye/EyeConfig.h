/**
 * @file EyeConfig.h
 * @brief Top-level configuration container for the eye project.
 *
 * Holds display dimensions, polar map settings, timing parameters, light sensor
 * configuration, input settings, and network settings. Used to parameterize the
 * EyeAnimator and related components.
 */
#ifndef EYE_CONFIG_H
#define EYE_CONFIG_H

#include "common/EyeState.h"
#include "common/DisplayGeometry.h"

/**
 * @brief Project-wide configuration structure.
 *
 * Contains display settings, polar map dimensions, timing ranges for blinks
 * and movements, light sensor parameters, input enable flags, and network
 * configuration for ESP-NOW peer synchronization.
 */
struct EyeProjectConfig
{
  // Display settings
  int displayWidth;   // Display horizontal resolution in pixels
  int displayHeight;  // Display vertical resolution in pixels
  int displayOffsetX; // Horizontal offset for eye centering
  int displayOffsetY; // Vertical offset for eye centering

  // Polar map settings
  int mapRadius;   // Radius of the precomputed polar displacement map
  int mapDiameter; // Diameter (= 2 * mapRadius)
  int pupilMin;    // Pupil radius at minimum dilation
  int pupilMax;    // Pupil radius at maximum dilation

  // Timing
  uint32_t blinkMinDuration; // Minimum blink duration in milliseconds
  uint32_t blinkMaxDuration; // Maximum blink duration in milliseconds
  uint32_t moveMinDuration;  // Minimum saccade duration in milliseconds
  uint32_t moveMaxDuration;  // Maximum saccade duration in milliseconds

  // Light sensor settings (-1 = disabled)
  int lightSensorPin;      // ADC pin for light sensor, -1 if not used
  uint16_t lightSensorMin; // Raw ADC minimum value (brightest light)
  uint16_t lightSensorMax; // Raw ADC maximum value (darkest light)
  float lightSensorCurve;  // Power curve exponent for response shaping
  float irisMin;           // Minimum pupil factor (dilated)
  float irisRange;         // Iris range (max - min)

  // Input settings
  bool wiiChuckEnabled; // Enable Wii Nunchuck input
  int wiiChuckAddress;  // I2C address for Wii Nunchuck

  // Network settings
  bool espNowEnabled;    // Enable ESP-NOW sync
  uint8_t espNowChannel; // WiFi channel for ESP-NOW

  // User hook settings
  bool moveEyesRandomly; // Enable random autonomous movement
  float eyeTargetX;      // Manual target X position (-1.0 to +1.0)
  float eyeTargetY;      // Manual target Y position (-1.0 to +1.0)
};

#endif // EYE_CONFIG_H