/**
 * @file GestureFaceInput.h
 * @brief DFRobot Gravity Offline Edge AI Gesture & Face Detection sensor input.
 *
 * Wraps the DFRobot_GestureFaceDetection_I2C driver and implements InputBase so
 * the face-tracking coordinates feed into EyeAnimator with lower priority than the
 * WiiChuck joystick but higher priority than autonomous random movement.
 *
 * When at least one face is detected and the confidence score meets the threshold,
 * hasExclusiveControl() returns true and getTargetX()/getTargetY() return the
 * normalized gaze target for the nearest face.
 *
 * Camera coordinates (0..frameW, 0..frameH) are mapped to eye space (-1..+1)
 * with (0,0) = top-left and (+1,+1) = bottom-right.
 */
#ifndef GESTURE_FACE_INPUT_H
#define GESTURE_FACE_INPUT_H

#include "InputBase.h"
#include <Wire.h>
#include <DFRobot_GestureFaceDetection.h>

/**
 * @brief InputBase implementation for the DFRobot Gravity Gesture & Face Detection sensor.
 *
 * Provides normalized eye-gaze coordinates derived from the nearest detected face.
 * Expression commands (blink, boop, close, wide) are not supported by this input source.
 */
class GestureFaceInput : public InputBase
{
public:
  /**
   * @brief Construct with I2C address, camera frame dimensions, and bus.
   *
   * @param addr    I2C device address (default 0x72).
   * @param frameW  Camera frame width in pixels used for coordinate normalization (default 320).
   * @param frameH  Camera frame height in pixels used for coordinate normalization (default 240).
   * @param wire    I2C bus instance to use (default Wire).
   */
  explicit GestureFaceInput(uint8_t addr = 0x72, uint16_t frameW = 320, uint16_t frameH = 240, TwoWire *wire = &Wire);

  /**
   * @brief Initialize the sensor on the Wire bus.
   *
   * Prints a startup diagnostic to Serial. Returns false and marks the sensor
   * as not connected if the I2C device is not found.
   *
   * @return true if the sensor responded and initialized successfully.
   */
  bool begin() override;

  /**
   * @brief Poll the sensor for the current face detection result.
   *
   * Updates m_targetX/Y from the nearest detected face when the confidence
   * score is at or above the configured threshold.
   *
   * @return true if a valid face was found this update.
   */
  bool update() override;

  /** @brief Normalized face X position (-1.0 = far left, +1.0 = far right). */
  float getTargetX() const override { return m_targetX; }

  /** @brief Normalized face Y position (-1.0 = far up, +1.0 = far down). */
  float getTargetY() const override { return m_targetY; }

  /**
   * @brief Returns true while a face is detected above the confidence threshold.
   *
   * When true, EyeAnimator routes eye position from this sensor rather than
   * using autonomous random movement.
   */
  bool hasExclusiveControl() const override { return m_hasFace; }

  /** @brief Face detection does not generate blink commands. */
  bool wantsBlink() const override { return false; }

  /** @brief Face detection does not generate boop commands. */
  bool wantsBoop() const override { return false; }

  /** @brief Face detection does not generate close commands. */
  bool wantsClose() const override { return false; }

  /** @brief Face detection does not generate wide commands. */
  bool wantsWide() const override { return false; }

  /** @brief Returns true if the sensor was found and initialized successfully. */
  bool isConnected() const { return m_connected; }

  /** @brief Returns the number of faces detected in the last poll. */
  uint16_t getFaceCount() const { return m_faceCount; }

  /**
   * @brief Set the minimum face confidence score required to drive eye movement.
   *
   * Scores are in the range 0–100 (percent). Faces with scores below this
   * threshold are ignored. Default is 50.
   *
   * @param score Minimum acceptable confidence (0–100).
   */
  void setMinScore(uint8_t score) { m_minScore = score; }

private:
  DFRobot_GestureFaceDetection_I2C m_sensor; // Underlying driver
  TwoWire *m_wire;                           // I2C bus instance
  uint8_t m_addr;                            // I2C address used for pre-probe
  float m_targetX = 0.0f;                    // Normalized gaze X (-1..+1)
  float m_targetY = 0.0f;                    // Normalized gaze Y (-1..+1)
  bool m_hasFace = false;                    // True when a face is actively tracked
  bool m_connected = false;                  // True after successful begin()
  uint16_t m_faceCount = 0;                 // Face count from last valid poll
  uint16_t m_frameW;                         // Camera frame width for normalization
  uint16_t m_frameH;                         // Camera frame height for normalization
  uint8_t m_minScore = 50;                   // Minimum confidence score threshold
  uint32_t m_lastPollMs = 0;                 // Timestamp of last sensor poll
  static constexpr uint32_t POLL_INTERVAL_MS = 100; // 10 Hz max poll rate (each read has 5ms blocking delay)
};

#endif // GESTURE_FACE_INPUT_H
