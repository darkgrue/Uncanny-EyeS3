#!/usr/bin/env python3
"""
Generate eye header files from .eye JSON config files for Uncanny EyeS3

This script reads .eye configuration files and generates C header files with:
- Eyelid lookup tables (from upper.png/lower.png if specified in config)
- EyeDefinition struct initialization

Usage:
    python geneye.py -eye <eye_name>      Generate header for specific eye
    python geneye.py -all               Generate headers for all eyes
    python geneye.py -list              List available eyes

Examples:
    python geneye.py -eye default_eye
    python geneye.py -all

Output is written to: include/eyes/<eye_name>.h
"""

import argparse
import json
import math
import os
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow library required. Install with: pip install Pillow")
    sys.exit(1)


# Base directories (relative to project root)
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
RESOURCES_EYES_DIR = PROJECT_ROOT / 'resources' / 'eyes'
INCLUDE_EYES_DIR = PROJECT_ROOT / 'include' / 'eyes'

# Screen size configurations
SCREEN_SIZES = {
    240: {'width': 240, 'height': 240, 'eye_radius': 120, 'map_radius': 120},
    466: {'width': 466, 'height': 466, 'eye_radius': 233, 'map_radius': 240},
    480: {'width': 480, 'height': 480, 'eye_radius': 240, 'map_radius': 240},
}


def find_eye_configs():
    """Find all .eye files in resources/eyes directory structure."""
    eye_configs = []

    if not RESOURCES_EYES_DIR.exists():
        return eye_configs

    for eye_dir in RESOURCES_EYES_DIR.iterdir():
        if not eye_dir.is_dir():
            continue

        # Look for .eye files in this directory
        for eye_file in eye_dir.glob('*.eye'):
            # Detect screen size from filename
            screen_size = 480  # default
            filename_lower = eye_file.name.lower()
            if '466' in filename_lower:
                screen_size = 466
            elif '480' in filename_lower:
                screen_size = 480
            elif '240' in filename_lower:
                screen_size = 240

            eye_configs.append({
                'path': eye_file,
                'name': eye_dir.name,
                'screen_size': screen_size
            })

    return eye_configs


def generate_no_eyelids(screen_width: int, eye_radius: int) -> list:
    """
    Generate default circular eyelids - simple circular opening with no custom shape.
    Returns table of (startY, endY) pairs in 0-255 range.
    For upper: (0, 0) signals "use curved boundary"
    For lower: (255, 0) signals "use curved boundary"
    """
    table = []
    for x in range(screen_width):
        # Use markers that trigger curved boundary fallback in renderer
        # upperEnd=0 triggers fallback, lowerStart=255 triggers fallback
        table.append((0, 0))  # upper: signals use curved boundary
    return table


def convert_eyelid_image(image_path: Path, screen_width: int, screen_height: int, is_upper: bool = True) -> list:
    """
    Convert an eyelid PNG to a lookup table.
    Black (0) = transparent (eye opening), non-black = eyelid area.
    Returns list of (startY, endY) pairs in 0-255 range.

    For upper eyelids: white region is at TOP of image, scan top-to-bottom
    For lower eyelids: white region is at BOTTOM of image, scan bottom-to-top
    """
    img = Image.open(image_path)
    img = img.convert('L')
    pixels = img.load()
    width, height = img.size

    table = []
    for x in range(screen_width):
        if is_upper:
            # Upper eyelid: white at TOP, black at BOTTOM
            # Find first white from top, then first gap after white
            start_y = 0
            end_y = height  # Default: lid covers everything

            found_start = False
            for y in range(height):
                if pixels[x, y] > 0:
                    if not found_start:
                        start_y = y
                        found_start = True
                elif found_start:
                    # Found first black pixel after white region
                    end_y = y
                    break

            # If no white found at all, set end_y = 0 to signal "no eyelid"
            if not found_start:
                end_y = 0
        else:
            # Lower eyelid: white at BOTTOM, black at TOP
            # Find first white from bottom, then first gap after white (going up)
            start_y = 0
            end_y = height  # Default: lid covers everything

            found_start = False
            for y in range(height - 1, -1, -1):
                if pixels[x, y] > 0:
                    if not found_start:
                        start_y = y
                        found_start = True
                elif found_start:
                    # Found first black pixel after white region (going up)
                    end_y = y
                    break

            # If no white found at all, set end_y = 0 to signal "no eyelid"
            if not found_start:
                end_y = 0

        table.append((
            min(int(start_y * 255 / screen_height), 255),
            min(int(end_y * 255 / screen_height), 255) if end_y > 0 else 0
        ))

    return table


def generate_header(config: dict, eye_file: Path, output_path: Path, screen_size: int):
    """Generate a complete eye header file from .eye config."""

    size_info = SCREEN_SIZES.get(screen_size, SCREEN_SIZES[480])
    width = size_info['width']
    height = size_info['height']
    eye_radius = size_info['eye_radius']
    map_radius = size_info['map_radius']

    eye_name = config.get(
        'name', eye_file.stem.replace(' ', '_').replace('-', '_'))
    namespace = eye_file.parent.name.replace(' ', '_').replace('-', '_')

    eyelid_config = config.get('eyelid', {})
    upper_filename = eyelid_config.get('upperFilename')
    lower_filename = eyelid_config.get('lowerFilename')

    if upper_filename and lower_filename:
        upper_path = eye_file.parent / upper_filename
        lower_path = eye_file.parent / lower_filename

        if upper_path.exists() and lower_path.exists():
            print(
                f"    Loading custom eyelids from {upper_filename}, {lower_filename}")
            upper_table = convert_eyelid_image(
                upper_path, width, height, True)   # is_upper=True
            lower_table = convert_eyelid_image(
                lower_path, width, height, False)  # is_upper=False
        else:
            print(f"    Warning: Eyelid files not found, using circular defaults")
            upper_table = generate_no_eyelids(width, eye_radius)
            lower_table = generate_no_eyelids(width, eye_radius)
    else:
        print(f"    Using circular defaults")
        upper_table = generate_no_eyelids(width, eye_radius)
        lower_table = generate_no_eyelids(width, eye_radius)

    lines = []
    lines.append('#pragma once')
    lines.append('')
    lines.append('#include <Arduino.h>')
    lines.append('#include "eyes.h"')
    lines.append('')
    lines.append(f'namespace {namespace} {{')
    lines.append('')

    lines.append(
        f'  // Upper eyelid lookup table ({width} columns, 0-255 range)')
    lines.append(f'  const uint8_t upper[{width * 2}] PROGMEM = {{')
    for i in range(0, width, 8):
        parts = []
        for j in range(8):
            idx = i + j
            if idx < width:
                start, end = upper_table[idx]
                parts.append(f'{start}, {end}')
        if parts:
            lines.append('    ' + ', '.join(parts) + ',')
    lines.append('  };')
    lines.append('')

    lines.append(
        f'  // Lower eyelid lookup table ({width} columns, 0-255 range)')
    lines.append(f'  const uint8_t lower[{width * 2}] PROGMEM = {{')
    for i in range(0, width, 8):
        parts = []
        for j in range(8):
            idx = i + j
            if idx < width:
                start, end = lower_table[idx]
                parts.append(f'{start}, {end}')
        if parts:
            lines.append('    ' + ', '.join(parts) + ',')
    lines.append('  };')
    lines.append('')

    pupil = config.get('pupil', {})
    iris = config.get('iris', {})
    sclera = config.get('sclera', {})

    radius_frac = config.get('radiusFraction', 0.5)
    back_color = config.get('backColor', 0)
    tracking = config.get('tracking', True)
    squint = config.get('squint', 0)

    normal_closure = eyelid_config.get('normalClosure', 0.15)
    wide_closure = eyelid_config.get('wideClosure', 0.0)

    lines.append(f'  const EyeDefinition eye PROGMEM = {{')
    lines.append(
        f'      "{eye_name}", {radius_frac}, {back_color}, {str(tracking).lower()}, {squint}, nullptr,')
    lines.append(f'      {{ {pupil.get("color", 0)}, {pupil.get("slitRadius", 0)}, '
                 f'{pupil.get("minFraction", 0.35)}, {pupil.get("maxFraction", 1.67)} }},')
    lines.append(f'      {{ {iris.get("radiusFraction", 0.5)}, {{ nullptr, 0, 0 }}, '
                 f'{iris.get("color", 0xFF01)}, {iris.get("angle", 0)}, '
                 f'{iris.get("spin", 0)}, 0, 0 }},')
    lines.append(f'      {{ {{ nullptr, 0, 0 }}, {sclera.get("color", 0xFFFF)}, '
                 f'{sclera.get("angle", 0)}, {sclera.get("spin", 0)}, 0, 0 }},')
    lines.append(f'      {{ upper, lower, {eyelid_config.get("color", 0)}, {normal_closure}, {wide_closure} }},')
    lines.append(f'      {{ {map_radius}, nullptr, nullptr }}')
    lines.append('  };')
    lines.append('}  // namespace')
    lines.append('')

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text('\n'.join(lines))
    print(f"    Written: {output_path}")


def detect_screen_size(eye_file: Path) -> int:
    """Detect screen size from file path."""
    path_str = str(eye_file)
    if '466' in path_str:
        return 466
    elif '480' in path_str:
        return 480
    elif '240' in path_str:
        return 240
    return 480


def main():
    parser = argparse.ArgumentParser(
        description='Generate eye header files from .eye config files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python geneye.py -eye default_eye       Generate header for default_eye
    python geneye.py -all                   Generate all eye headers
    python geneye.py -list                 List available eyes

Output is written to: include/eyes/<eye_name>.h
        """
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('-eye', metavar='NAME',
                       help='Generate header for specific eye directory name')
    group.add_argument('-all', action='store_true',
                       help='Generate headers for all eyes')
    group.add_argument('-list', action='store_true',
                       help='List available eyes')

    args = parser.parse_args()

    # Ensure output directory exists
    INCLUDE_EYES_DIR.mkdir(parents=True, exist_ok=True)

    if args.list:
        print("Available eyes:")
        eye_configs = find_eye_configs()
        if not eye_configs:
            print("  No eye configurations found in resources/eyes/")
            return
        for ec in eye_configs:
            print(
                f"  - {ec['name']} ({ec['screen_size']}x{ec['screen_size']})")
        return

    if args.all:
        print(f"Generating all eye headers...")
        print(f"  Source: {RESOURCES_EYES_DIR}")
        print(f"  Output: {INCLUDE_EYES_DIR}")
        print()

        # Group by eye name to handle multiple screen sizes
        eye_configs = find_eye_configs()
        if not eye_configs:
            print("Error: No .eye files found in resources/eyes/")
            sys.exit(1)

        # Group by eye name
        eyes_by_name = {}
        for ec in eye_configs:
            name = ec['name']
            if name not in eyes_by_name:
                eyes_by_name[name] = []
            eyes_by_name[name].append(ec)

        total_generated = 0
        for eye_name, configs in eyes_by_name.items():
            print(f"Processing: {eye_name}")

            for ec in configs:
                config_path = ec['path']
                screen_size = ec['screen_size']

                with open(config_path) as f:
                    config = json.load(f)

                output_name = f"{eye_name}_{screen_size}.h"
                output_path = INCLUDE_EYES_DIR / output_name

                generate_header(config, config_path, output_path, screen_size)
                total_generated += 1

        print()
        print(f"Generated {total_generated} eye headers")
        return

    if args.eye:
        eye_name = args.eye

        # Find the eye directory
        eye_dir = RESOURCES_EYES_DIR / eye_name
        if not eye_dir.exists():
            print(f"Error: Eye directory not found: {eye_dir}")
            sys.exit(1)

        # Find .eye file in that directory
        eye_files = list(eye_dir.glob('*.eye'))
        if not eye_files:
            print(f"Error: No .eye file found in {eye_dir}")
            sys.exit(1)

        # Generate header for each screen size variant found
        for config_path in eye_files:
            print(f"Generating header for {eye_name}")

            with open(config_path) as f:
                config = json.load(f)

            screen_size = detect_screen_size(config_path)
            output_name = f"{eye_name}_{screen_size}.h"
            output_path = INCLUDE_EYES_DIR / output_name

            generate_header(config, config_path, output_path, screen_size)

        print(f"Done! Output in: {INCLUDE_EYES_DIR}")


if __name__ == '__main__':
    main()
