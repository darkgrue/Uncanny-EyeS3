/**
 * @file DisplayGeometry.h
 * @brief Display geometry helpers, polar maps, and circular clipping.
 *
 * Provides display size constants, polar coordinate conversion helpers,
 * EyelidData and PolarMapData structures used by eye definitions, and
 * the CircularClip class for efficient per-scanline circle boundary
 * computation (precomputed once, reused every frame to avoid sqrt()).
 */
#ifndef DISPLAY_GEOMETRY_H
#define DISPLAY_GEOMETRY_H

#include <stdint.h>
#include <vector>
#include <cmath>

#define MAX_DISPLAY_SIZE 480 // Maximum supported display dimension
#define MAP_MAX_RADIUS 240   // Half of maximum display size

#define EYE_RADIUS_AMOLED 233 // AMOLED eye radius (~466 / 2)
#define EYE_RADIUS_TRGB 240   // T-RGB eye radius (~480 / 2)

/**
 * @brief Custom eyelid shape data from eye definition.
 *
 * Contains per-column Y positions for upper and lower eyelids in both
 * open and closed states. Values of 255 indicate no data for that column.
 */
struct EyelidData
{
  uint8_t upperOpen[MAX_DISPLAY_SIZE];
  uint8_t upperClosed[MAX_DISPLAY_SIZE];
  uint8_t lowerOpen[MAX_DISPLAY_SIZE];
  uint8_t lowerClosed[MAX_DISPLAY_SIZE];
};

/**
 * @brief Precomputed polar displacement map for an eye.
 *
 * Allocated based on display size. Contains angle, distance, and X/Y
 * displacement lookup tables for fast per-pixel eye rendering.
 */
struct PolarMapData
{
  uint16_t *angle;    // Angle lookup [diameter * diameter]
  uint8_t *dist;      // Distance lookup [diameter * diameter]
  uint8_t *displaceX; // X displacement lookup [radius * diameter]
  uint8_t *displaceY; // Y displacement lookup [radius * diameter]
  uint16_t radius;    // Map radius in pixels
  uint16_t diameter;  // Map diameter in pixels
};

/** @brief Precomputed scanline extent for circular clipping. */
struct ScanlineBounds
{
  int16_t xStart; // First active X in this row (-1 if outside circle)
  int16_t xEnd;   // Last active X in this row
};

/**
 * @brief Efficient circular clipping bounds precomputer.
 *
 * Computes xStart/xEnd for every scanline once at initialization,
 * avoiding per-pixel sqrt() calls during rendering.
 */
class CircularClip
{
public:
  /**
   * @brief Precompute circular bounds for the given circle.
   * @param centerX Circle center X.
   * @param centerY Circle center Y.
   * @param radius Circle radius.
   * @param width Frame buffer width.
   * @param height Frame buffer height.
   */
  void compute(int centerX, int centerY, int radius, int width, int height);

  /** @brief Returns true if this row intersects the circle. */
  bool isRowActive(int y) const
  {
    return y >= 0 && y < m_height && m_bounds[y].xStart >= 0;
  }

  /** @brief First active X in row y. */
  int16_t getXStart(int y) const { return m_bounds[y].xStart; }

  /** @brief Last active X in row y. */
  int16_t getXEnd(int y) const { return m_bounds[y].xEnd; }

  int getCenterX() const { return m_centerX; }
  int getCenterY() const { return m_centerY; }
  int getRadius() const { return m_radius; }

private:
  int m_centerX = 0;
  int m_centerY = 0;
  int m_radius = 0;
  int m_width = 0;
  int m_height = 0;
  std::vector<ScanlineBounds> m_bounds;
};

#endif // DISPLAY_GEOMETRY_H