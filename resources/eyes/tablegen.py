#!/usr/bin/python3
"""
Eye data generator for Uncanny-EyeS3 project.
Generates C++ header files with precomputed polar maps, displacement maps,
and eyelid lookup tables from configuration files.

Supports multiple display sizes:
  - 240x240  (default/reference)
  - 466x466  (T-Display S3 AMOLED)
  - 480x480  (T-RGB, 2.1" and 2.8" variants)

Usage:
    python tablegen.py <output_dir> [config.eye] [display_type]
    python tablegen.py <output_dir> --all [display_type]

display_type: amoled (466x466), trgb (480x480), or default (240x240)
"""

import json
import math
import os
import sys
import numpy as np
from PIL import Image
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


def generate_eyelid_lookup(image, screenWidth, screenHeight):
    """Generate eyelid lookup table from grayscale image.

    Image should be white where eyelid covers, black where eye is visible.
    Returns array of (startY, endY) pairs for each X column.
    """
    pixels = image.convert("L").load()

    if image.size != (screenWidth, screenHeight):
        raise Exception(
            f"Eyelid image must be {screenWidth}x{screenHeight}, got {image.size}")

    lookup = np.zeros(screenWidth * 2, dtype=np.uint8)

    for x in range(screenWidth):
        found_start = False
        found_end = False

        for y in range(screenHeight):
            if not found_start:
                if pixels[x, y] > 127:
                    lookup[x * 2] = y
                    found_start = True
            elif not found_end:
                if pixels[x, y] < 127:
                    lookup[x * 2 + 1] = y
                    found_end = True
                    break

        if not found_start:
            raise Exception(f"Eyelid image has no white pixels in column {x}")
        if not found_end:
            lookup[x * 2 + 1] = screenHeight

    return lookup


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


def generate_display_header(outputDir, screenWidth, screenHeight, mapRadius):
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


def generate_eye_header(outputDir, config, screenWidth, screenHeight, mapRadius, displayTables, configDir=None):
    """Generate complete eye header file."""

    eyeName = config.get("name", "eye")
    radiusFraction = config.get("radiusFraction", 0.5)
    backColor = config.get("backColor", 0x7BEF)
    tracking = config.get("tracking", True)
    squint = config.get("squint", 0)

    baseDir = Path(configDir) if configDir else Path.cwd()

    # Pupil config
    pupil = config.get("pupil", {})
    pupilColor = pupil.get("color", 0x0000)
    slitRadius = pupil.get("slitRadius", 0)
    pupilMinFraction = pupil.get("minFraction", 0.35)
    pupilMaxFraction = pupil.get("maxFraction", 1.67)

    # Iris config
    iris = config.get("iris", {})
    irisRadiusFraction = iris.get("radiusFraction", 0.5)
    irisColor = iris.get("color", 0xFF01)
    irisAngle = iris.get("angle", 0)
    irisSpin = iris.get("spin", 0)
    irisMirror = 1023 if iris.get("mirror", False) else 0

    # Sclera config
    sclera = config.get("sclera", {})
    scleraColor = sclera.get("color", 0xFFFF)
    scleraAngle = sclera.get("angle", 0)
    scleraSpin = sclera.get("spin", 0)
    scleraMirror = 1023 if sclera.get("mirror", False) else 0

    # Eyelid config
    eyelid = config.get("eyelid", {})
    eyelidColor = eyelid.get("color", 0x0000)

    # Determine display type suffix
    if screenWidth == 466 and screenHeight == 466:
        dispType = "466"
    elif screenWidth == 480 and screenHeight == 480:
        dispType = "480"
    else:
        dispType = "240"

    # Use shared display-level tables
    angleName = displayTables["polarAngle"][1]
    distName = displayTables["polarDist"][1]
    dispName = displayTables["dispMap"][1]

    # Output header filename
    headerPath = Path(outputDir) / f"{eyeName}_{dispType}.h"

    with open(headerPath, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <Arduino.h>\n")
        f.write('#include "eyes.h"\n')
        f.write(f'#include "display_{dispType}.h"\n\n')

        f.write(
            f"// Eye: {eyeName} for {screenWidth}x{screenHeight} display\n")
        f.write(f"// Map radius: {mapRadius}\n")
        f.write(f"// Eye radius fraction: {radiusFraction}\n\n")

        # Eyelid lookup tables (per-eye, per-display-size)
        upperFilename = eyelid.get("upper")
        lowerFilename = eyelid.get("lower")

        upperName = f"{eyeName}_upper"
        lowerName = f"{eyeName}_lower"

        if upperFilename:
            upperImg = Image.open(baseDir / upperFilename)
            upperLookup = generate_eyelid_lookup(
                upperImg, screenWidth, screenHeight)
            f.write(
                f"const uint8_t {upperName}[{screenWidth} * 2] PROGMEM = {{\n")
            output_hex_array(f, upperLookup)
            f.write("};\n\n")
        else:
            f.write(
                f"const uint8_t {upperName}[{screenWidth} * 2] PROGMEM = {{0}};\n\n")

        if lowerFilename:
            lowerImg = Image.open(baseDir / lowerFilename)
            lowerLookup = generate_eyelid_lookup(
                lowerImg, screenWidth, screenHeight)
            f.write(
                f"const uint8_t {lowerName}[{screenWidth} * 2] PROGMEM = {{\n")
            output_hex_array(f, lowerLookup)
            f.write("};\n\n")
        else:
            f.write(
                f"const uint8_t {lowerName}[{screenWidth} * 2] PROGMEM = {{0}};\n\n")

        # Iris texture
        irisFilename = iris.get("filename")
        irisDataName = None
        if irisFilename:
            irisDataName = f"{eyeName}_iris"
            irisImg = Image.open(baseDir / irisFilename).convert("RGB")
            irisW, irisH = irisImg.size
            f.write(f"constexpr uint16_t {irisDataName}Width = {irisW};\n")
            f.write(f"constexpr uint16_t {irisDataName}Height = {irisH};\n")
            f.write(
                f"const uint16_t {irisDataName}[{irisW} * {irisH}] PROGMEM = {{\n")
            pixels = irisImg.load()
            for y in range(irisH):
                for x in range(irisW):
                    r, g, b = pixels[x, y]
                    color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    f.write(f"0x{color:04X}, ")
                    if (y * irisW + x + 1) % 8 == 0:
                        f.write("\n")
            f.write("\n};\n\n")

        # Sclera texture
        scleraFilename = sclera.get("filename")
        scleraDataName = None
        if scleraFilename:
            scleraDataName = f"{eyeName}_sclera"
            scleraImg = Image.open(baseDir / scleraFilename).convert("RGB")
            scleraW, scleraH = scleraImg.size
            f.write(f"constexpr uint16_t {scleraDataName}Width = {scleraW};\n")
            f.write(
                f"constexpr uint16_t {scleraDataName}Height = {scleraH};\n")
            f.write(
                f"const uint16_t {scleraDataName}[{scleraW} * {scleraH}] PROGMEM = {{\n")
            pixels = scleraImg.load()
            for y in range(scleraH):
                for x in range(scleraW):
                    r, g, b = pixels[x, y]
                    color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    f.write(f"0x{color:04X}, ")
                    if (y * scleraW + x + 1) % 8 == 0:
                        f.write("\n")
            f.write("\n};\n\n")

        # EyeDefinition struct
        trackingStr = "true" if tracking else "false"
        f.write(f"namespace {eyeName} {{\n")
        f.write(f"  const EyeDefinition eye PROGMEM = {{\n")
        f.write(f"      \"{eyeName}\", {radiusFraction}, 0x{backColor:04X}, ")
        f.write(f"{trackingStr}, {squint}, {dispName},\n")

        # Pupil
        f.write(
            f"      {{ 0x{pupilColor:04X}, {slitRadius}, {pupilMinFraction}, {pupilMaxFraction} }},\n")

        # Iris
        if irisDataName:
            f.write(
                f"      {{ {irisRadiusFraction}, {{ {irisDataName}, {irisDataName}Width, {irisDataName}Height }}, ")
        else:
            f.write(f"      {{ {irisRadiusFraction}, {{ nullptr, 0, 0 }}, ")
        f.write(
            f"0x{irisColor:04X}, {irisAngle}, {irisSpin}, {irisMirror} }},\n")

        # Sclera
        if scleraDataName:
            f.write(
                f"      {{ {{ {scleraDataName}, {scleraDataName}Width, {scleraDataName}Height }}, ")
        else:
            f.write(f"      {{ {{ nullptr, 0, 0 }}, ")
        f.write(
            f"0x{scleraColor:04X}, {scleraAngle}, {scleraSpin}, {scleraMirror} }},\n")

        # Eyelid
        f.write(
            f"      {{ {upperName}, {lowerName}, 0x{eyelidColor:04X} }},\n")

        # Polar map info - uses shared display-level tables
        f.write(f"      {{ {mapRadius}, {angleName}, {distName} }}\n")

        f.write(f"  }};\n")
        f.write(f"}}  // namespace {eyeName}\n")

    print(f"Generated: {headerPath}")
    return headerPath


def generate_display_header_file(outputDir, screenWidth, screenHeight, mapRadius, displayTables):
    """Generate a header file with display-level tables."""

    if screenWidth == 466 and screenHeight == 466:
        dispType = "466"
    elif screenWidth == 480 and screenHeight == 480:
        dispType = "480"
    else:
        dispType = "240"

    headerPath = Path(outputDir) / f"display_{dispType}.h"

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


def generate_all_eyes(configDir, outputDir, displayType="default"):
    """Generate eye headers for all config files in a directory."""

    if displayType not in DISPLAY_CONFIGS:
        print(f"Unknown display type: {displayType}")
        print(f"Available: {list(DISPLAY_CONFIGS.keys())}")
        return

    cfg = DISPLAY_CONFIGS[displayType]
    screenWidth = cfg["width"]
    screenHeight = cfg["height"]
    mapRadius = cfg["mapRadius"]

    print(
        f"Generating for {displayType}: {screenWidth}x{screenHeight}, mapRadius={mapRadius}")

    # Generate display-level tables once
    displayTables = generate_display_header(
        outputDir, screenWidth, screenHeight, mapRadius)

    # Generate display header file with shared tables
    generate_display_header_file(
        outputDir, screenWidth, screenHeight, mapRadius, displayTables)

    # Generate per-eye headers
    configPath = Path(configDir)
    for configFile in configPath.glob("**/*.eye"):
        try:
            with open(configFile) as f:
                config = json.load(f)

            generate_eye_header(outputDir, config, screenWidth, screenHeight, mapRadius, displayTables,
                                configDir=configFile.parent)
        except Exception as e:
            print(f"Error processing {configFile}: {e}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: tablegen.py <output_dir> [config.eye] [display_type]")
        print("       tablegen.py <output_dir> --all [display_type]")
        print("")
        print("display_type: amoled (466x466), trgb (480x480), or default (240x240)")
        sys.exit(1)

    outputDir = sys.argv[1]

    # Determine mode and arguments
    if len(sys.argv) >= 3 and sys.argv[2] == "--all":
        # Generate all eyes in resources/eyes/ directory
        displayType = sys.argv[3] if len(sys.argv) > 3 else "default"
        configDir = Path(__file__).parent
        generate_all_eyes(configDir, outputDir, displayType)
    elif len(sys.argv) >= 3:
        # Generate single eye
        configFile = sys.argv[2]
        displayType = sys.argv[3] if len(sys.argv) > 3 else "default"

        if displayType not in DISPLAY_CONFIGS:
            print(f"Unknown display type: {displayType}")
            print(f"Available: {list(DISPLAY_CONFIGS.keys())}")
            sys.exit(1)

        cfg = DISPLAY_CONFIGS[displayType]
        screenWidth = cfg["width"]
        screenHeight = cfg["height"]
        mapRadius = cfg["mapRadius"]

        # Generate display tables for this single eye
        displayTables = generate_display_header(
            outputDir, screenWidth, screenHeight, mapRadius)
        generate_display_header_file(
            outputDir, screenWidth, screenHeight, mapRadius, displayTables)

        with open(configFile) as f:
            config = json.load(f)

        generate_eye_header(outputDir, config, screenWidth, screenHeight, mapRadius, displayTables,
                            configDir=Path(configFile).parent)
    else:
        print("Usage: tablegen.py <output_dir> <config.eye> [display_type]")
        print("       tablegen.py <output_dir> --all [display_type]")
        sys.exit(1)
