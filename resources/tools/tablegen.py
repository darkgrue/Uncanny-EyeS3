#!/usr/bin/python3
"""
Display table generator for Uncanny-EyeS3 project.
Generates C++ header files with precomputed polar maps and displacement maps.
These tables are shared across all eyes for a given display size.

Supports multiple display sizes:
  - 240x240  (default/reference)
  - 466x466  (T-Display S3 AMOLED)
  - 480x480  (T-RGB, 2.1" and 2.8" variants)

Usage:
    python tablegen.py <output_dir> [display_type]
    python tablegen.py <output_dir> --all

display_type: amoled (466x466), trgb (480x480), or default (240x240)
"""

import math
import sys
import numpy as np
from pathlib import Path

PI = math.pi
PI_2 = PI / 2.0

# Display configurations
DISPLAY_CONFIGS = {
    "default": {"width": 240, "height": 240, "mapRadius": 120},
    "amoled":  {"width": 466, "height": 466, "mapRadius": 233},
    "trgb":    {"width": 480, "height": 480, "mapRadius": 240},
}


def screen_to_map(mapRadius, eyeRadius, value):
    """Scale screen pixels to polar map coordinates."""
    return math.atan2(value, math.sqrt(eyeRadius * eyeRadius - value * value)) / PI_2 * mapRadius


def generate_polar_maps(mapRadius, eyeRadius, irisRadius, slitRadius=0):
    """Generate polar angle and distance lookup tables for one quadrant.

    Returns:
        polarAngle: uint8 array [mapRadius * mapRadius] - angle 0-255
        polarDist: uint8 array [mapRadius * mapRadius] - distance encoding
            0-127: sclera (outer edge to iris boundary)
            128-254: iris/pupil region
            255: off-map
    """
    mapSize = mapRadius * mapRadius
    polarAngle = np.zeros(mapSize, dtype=np.uint8)
    polarDist = np.zeros(mapSize, dtype=np.uint8)

    # Iris size in polar map pixels
    iRad = screen_to_map(mapRadius, eyeRadius, irisRadius)

    for y in range(mapRadius):
        dy = y + 0.5
        dy2 = dy * dy
        for x in range(mapRadius):
            dx = x + 0.5
            d2 = dx * dx + dy2

            if d2 > mapRadius * mapRadius:
                # Outside the circular map
                polarAngle[y * mapRadius + x] = 0
                polarDist[y * mapRadius + x] = 255
            else:
                # Angle: 0-255 representing 0-360 degrees
                angle = math.atan2(dy, dx)
                angle = PI_2 - angle  # Clockwise from top
                angle = angle * 255.0 / (2.0 * PI_2)  # Scale to 0-255
                polarAngle[y * mapRadius + x] = int(angle) & 0xFF

                d = math.sqrt(d2)
                if d > iRad:
                    # Sclera region (0-127)
                    dist = (mapRadius - d) / (mapRadius - iRad) * 127.0
                    polarDist[y * mapRadius + x] = int(dist) & 0x7F
                else:
                    # Iris/pupil region (128-254)
                    if slitRadius == 0:
                        dist = (iRad - d) / iRad * 127.0
                        polarDist[y * mapRadius + x] = (int(dist) & 0x7F) + 128
                    else:
                        # Slit pupil - simplified, use max iris
                        polarDist[y * mapRadius + x] = 254

    return polarAngle, polarDist


def generate_displacement_map(screenWidth, screenHeight, mapRadius, eyeRadius):
    """Generate spherical displacement lookup table.

    The displacement map corrects for the spherical projection of the eye,
    offsetting X coordinates to create the 3D sphere illusion.

    Returns:
        dispMap: uint8 array [halfWidth * halfHeight]
        255 = off eye boundary
    """
    halfW = screenWidth // 2
    halfH = screenHeight // 2
    eyeRadiusSq = float(eyeRadius * eyeRadius)

    dispMap = np.zeros(halfW * halfH, dtype=np.uint8)

    for y in range(halfH):
        dy = float(y) + 0.5
        dySq = dy * dy
        for x in range(halfW):
            dx = float(x) + 0.5
            d2 = dx * dx + dySq

            if d2 <= eyeRadiusSq:
                d = math.sqrt(d2)
                if d > 0:
                    # Angle from center
                    angle = math.atan2(dx, dy)
                    # Spherical projection offset
                    sphericalOffset = math.sin(
                        math.acos(d / eyeRadius)) * mapRadius
                    # Displacement = spherical projection - original position
                    dispX = int(sphericalOffset * math.sin(angle) - dx) & 0xFF
                    dispMap[y * halfW + x] = dispX
                else:
                    dispMap[y * halfW + x] = 0
            else:
                dispMap[y * halfW + x] = 255  # Mark as outside

    return dispMap, halfW, halfH


def output_hex_array(out, data, bytesPerLine=16):
    """Output numpy array as C++ hex array."""
    size = data.size
    for i in range(size):
        out.write(f"0x{data[i]:02X}")
        if i < size - 1:
            out.write(", ")
        if (i + 1) % bytesPerLine == 0:
            out.write("\n")
    if size % bytesPerLine != 0:
        out.write("\n")


def generate_display_tables(screenWidth, screenHeight, mapRadius):
    """Generate display-level polar maps and displacement map.

    These tables are shared across all eyes for a given display size.
    Returns dict with table names and data.
    """
    # Calculate eye radius from map radius (eye fills the map)
    eyeRadius = mapRadius

    # Generate polar maps for this display
    polarAngle, polarDist = generate_polar_maps(
        mapRadius, eyeRadius, eyeRadius)

    # Generate displacement map
    dispMap, halfW, halfH = generate_displacement_map(
        screenWidth, screenHeight, mapRadius, eyeRadius)

    return {
        "polarAngle": (polarAngle, f"polarAngle_{mapRadius}"),
        "polarDist": (polarDist, f"polarDist_{mapRadius}"),
        "dispMap": (dispMap, f"disp_{screenWidth}_{screenHeight}"),
        "halfW": halfW,
        "halfH": halfH,
    }


def generate_display_header_file(outputDir, screenWidth, screenHeight, mapRadius, displayTables):
    """Generate a header file with display-level tables."""

    if screenWidth == 466 and screenHeight == 466:
        dispType = "466"
    elif screenWidth == 480 and screenHeight == 480:
        dispType = "480"
    else:
        dispType = "240"

    # Put display headers in a display subdirectory
    displayDir = Path(outputDir) / "display"
    displayDir.mkdir(parents=True, exist_ok=True)
    headerPath = displayDir / f"display_{dispType}.h"

    angleName = displayTables["polarAngle"][1]
    distName = displayTables["polarDist"][1]
    dispName = displayTables["dispMap"][1]
    halfW = displayTables["halfW"]
    halfH = displayTables["halfH"]

    with open(headerPath, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <Arduino.h>\n\n")

        f.write(f"// Display: {screenWidth}x{screenHeight}\n")
        f.write(f"// Map radius: {mapRadius}\n\n")

        # Polar angle map
        f.write(
            f"const uint8_t {angleName}[{mapRadius * mapRadius}] PROGMEM = {{\n")
        output_hex_array(f, displayTables["polarAngle"][0])
        f.write("};\n\n")

        # Polar distance map
        f.write(
            f"const uint8_t {distName}[{mapRadius * mapRadius}] PROGMEM = {{\n")
        output_hex_array(f, displayTables["polarDist"][0])
        f.write("};\n\n")

        # Displacement map
        f.write(f"const uint8_t {dispName}[{halfW * halfH}] PROGMEM = {{\n")
        output_hex_array(f, displayTables["dispMap"][0])
        f.write("};\n\n")

        # Extern declarations for eye headers to reference
        f.write(f"// Table references for eye headers\n")
        f.write(f"extern const uint8_t {angleName}[];\n")
        f.write(f"extern const uint8_t {distName}[];\n")
        f.write(f"extern const uint8_t {dispName}[];\n")

    print(f"Generated display header: {headerPath}")
    return headerPath


def generate_all_displays(outputDir):
    """Generate display tables for all supported display types."""
    for displayType, cfg in DISPLAY_CONFIGS.items():
        screenWidth = cfg["width"]
        screenHeight = cfg["height"]
        mapRadius = cfg["mapRadius"]

        print(
            f"Generating {displayType}: {screenWidth}x{screenHeight}, mapRadius={mapRadius}")

        displayTables = generate_display_tables(
            screenWidth, screenHeight, mapRadius)
        generate_display_header_file(
            outputDir, screenWidth, screenHeight, mapRadius, displayTables)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: tablegen.py <output_dir> [display_type]")
        print("       tablegen.py <output_dir> --all")
        print("")
        print("display_type: amoled (466x466), trgb (480x480), or default (240x240)")
        print("--all generates all display types")
        sys.exit(1)

    outputDir = sys.argv[1]

    # Ensure output directory exists
    Path(outputDir).mkdir(parents=True, exist_ok=True)

    if len(sys.argv) >= 3 and sys.argv[2] == "--all":
        generate_all_displays(outputDir)
    elif len(sys.argv) >= 2:
        displayType = sys.argv[2] if len(sys.argv) > 2 else "default"

        if displayType not in DISPLAY_CONFIGS:
            print(f"Unknown display type: {displayType}")
            print(f"Available: {list(DISPLAY_CONFIGS.keys())}")
            sys.exit(1)

        cfg = DISPLAY_CONFIGS[displayType]
        screenWidth = cfg["width"]
        screenHeight = cfg["height"]
        mapRadius = cfg["mapRadius"]

        print(
            f"Generating {displayType}: {screenWidth}x{screenHeight}, mapRadius={mapRadius}")

        displayTables = generate_display_tables(
            screenWidth, screenHeight, mapRadius)
        generate_display_header_file(
            outputDir, screenWidth, screenHeight, mapRadius, displayTables)
    else:
        print("Usage: tablegen.py <output_dir> [display_type]")
        print("       tablegen.py <output_dir> --all")
        sys.exit(1)
