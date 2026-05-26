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
import re
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
EYELIBRARY_H = PROJECT_ROOT / 'include' / 'EyeLibrary.h'
EYEMOVEMENT_H = PROJECT_ROOT / 'src' / 'animation' / 'EyeMovement.h'

# Screen size configurations
SCREEN_SIZES = {
    240: {'width': 240, 'height': 240, 'eye_radius': 120, 'map_radius': 120},
    466: {'width': 466, 'height': 466, 'eye_radius': 233, 'map_radius': 233},
    480: {'width': 480, 'height': 480, 'eye_radius': 240, 'map_radius': 240},
}


def parse_eyemovement_defaults() -> tuple:
    """Extract eyelid closure defaults from EyeMovement.h #define lines.

    Returns (normal_closure_default, wide_closure_default) as floats.
    Falls back to (0.15, 0.0) if the file cannot be read or parsed.
    """
    fallback = (0.15, 0.0)

    if not EYEMOVEMENT_H.exists():
        print(f"  Warning: {EYEMOVEMENT_H} not found — using hardcoded defaults")
        return fallback

    text = EYEMOVEMENT_H.read_text(encoding='utf-8')

    def extract(name: str, default: float) -> float:
        m = re.search(rf'#define\s+{name}\s+([\d.]+)f?', text)
        return float(m.group(1)) if m else default

    return (
        extract('EYELID_NORMAL_CLOSURE_DEFAULT', fallback[0]),
        extract('EYELID_WIDE_CLOSURE_DEFAULT', fallback[1]),
    )


_SIZE_SUFFIX_RE = re.compile(r'_(\d+)\.eye$', re.IGNORECASE)


def find_eye_configs() -> list:
    """Find all .eye files in resources/eyes directory structure."""
    eye_configs = []

    if not RESOURCES_EYES_DIR.exists():
        return eye_configs

    for eye_dir in RESOURCES_EYES_DIR.iterdir():
        if not eye_dir.is_dir():
            continue

        for eye_file in eye_dir.glob('*.eye'):
            m = _SIZE_SUFFIX_RE.search(eye_file.name)
            if not m or int(m.group(1)) not in SCREEN_SIZES:
                print(f"  Warning: cannot determine screen size from filename "
                      f"'{eye_file.name}' — skipping (expected suffix _NNN.eye "
                      f"where NNN is one of {sorted(SCREEN_SIZES)})")
                continue
            eye_configs.append({
                'path': eye_file,
                'name': eye_dir.name,
                'screen_size': int(m.group(1)),
            })

    return eye_configs


def generate_no_eyelids(screen_width: int) -> list:
    """Return a table of (0, 0) sentinel pairs that trigger the curved boundary
    fallback in the renderer — one entry per column."""
    return [(0, 0)] * screen_width


# Maximum texture dimensions to cap PROGMEM usage (~64 KB per texture at 256×128).
MAX_TEX_W = 256
MAX_TEX_H = 128


def convert_texture_to_rgb565(
    image_path: Path,
    max_w: int = MAX_TEX_W,
    max_h: int = MAX_TEX_H,
) -> tuple:
    """Convert a PNG to an angle-major RGB565 uint16 list, capped at max_w × max_h.

    Stored column-major: T[angle_index * height + radius_index].
    This matches the render loop access pattern (radius changes faster than angle),
    converting dominant 512-byte stride jumps to sequential 2-byte steps.

    Returns (pixel_list, width, height).
    """
    img = Image.open(image_path).convert('RGB')
    w, h = img.size

    if w > max_w or h > max_h:
        scale = min(max_w / w, max_h / h)
        w = max(1, int(w * scale))
        h = max(1, int(h * scale))
        img = img.resize((w, h), Image.LANCZOS)

    pixels = img.load()
    data = []
    for x in range(w):          # angle is the outer (slow-changing) dimension
        for y in range(h):      # radius is the inner (fast-changing) dimension
            r, g, b = pixels[x, y]
            data.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

    return data, w, h


def _build_texture_array_lines(name: str, data: list, width: int, height: int) -> list:
    """Emit a PROGMEM uint16_t array for one texture."""
    lines = [
        f'  // {name} ({width}x{height} px, RGB565 angle-major: T[angle*{height}+radius])',
        f'  const uint16_t {name}[{width * height}] PROGMEM = {{',
    ]
    for i in range(0, len(data), 8):
        chunk = data[i:i + 8]
        lines.append('    ' + ', '.join(f'0x{v:04X}' for v in chunk) + ',')
    lines.append('  };')
    return lines


def convert_eyelid_image(
    image_path: Path,
    screen_width: int,
    screen_height: int,
    is_upper: bool = True,
) -> list:
    """Convert an eyelid PNG to a per-column (startY, endY) lookup table.

    Black (0) = transparent (eye opening), non-black = eyelid area.
    Returns a list of (startY, endY) pairs scaled to 0-255.

    Upper eyelids: white region at TOP, scanned top-to-bottom.
    Lower eyelids: white region at BOTTOM, scanned bottom-to-top.
    """
    img = Image.open(image_path)
    img = img.convert('L')
    img_w, img_h = img.size
    if img_w != screen_width or img_h != screen_height:
        print(f"    Warning: eyelid image {image_path.name} is {img_w}x{img_h},"
              f" resizing to {screen_width}x{screen_height}")
        img = img.resize((screen_width, screen_height), Image.LANCZOS)
    pixels = img.load()
    _, height = img.size

    table = []
    for x in range(screen_width):
        if is_upper:
            start_y = 0
            end_y = height
            found_start = False
            for y in range(height):
                if pixels[x, y] > 0:
                    if not found_start:
                        start_y = y
                        found_start = True
                elif found_start:
                    end_y = y
                    break
            if not found_start:
                end_y = 0
        else:
            start_y = 0
            end_y = height
            found_start = False
            for y in range(height - 1, -1, -1):
                if pixels[x, y] > 0:
                    if not found_start:
                        start_y = y
                        found_start = True
                elif found_start:
                    end_y = y
                    break
            if not found_start:
                end_y = 0

        table.append((
            min(int(start_y * 255 / screen_height), 255),
            min(int(end_y * 255 / screen_height), 255) if end_y > 0 else 0,
        ))

    return table


def _build_eye_definition_lines(
    config: dict,
    eyelid_config: dict,
    map_radius: int,
    normal_closure_str: str,
    wide_closure_str: str,
    iris_tex_name: str = '',
    iris_tex_w: int = 0,
    iris_tex_h: int = 0,
    sclera_tex_name: str = '',
    sclera_tex_w: int = 0,
    sclera_tex_h: int = 0,
) -> list:
    """Return the EyeDefinition struct initializer lines."""
    eye_name = config.get('name', 'unnamed')
    pupil = config.get('pupil', {})
    iris = config.get('iris', {})
    sclera = config.get('sclera', {})

    radius_frac = config.get('radiusFraction', 0.5)
    back_color = config.get('backColor', 0)
    squint = config.get('squint', 0)
    lid_color = eyelid_config.get('color', 0)
    tracking = str(eyelid_config.get('tracking', True)).lower()

    pupil_color = pupil.get('color', 0)
    pupil_slit = float(pupil.get('slitRadius', 0))
    pupil_min = pupil.get('minFraction', 0.35)
    pupil_max = pupil.get('maxFraction', 1.67)

    iris_rad = iris.get('radiusFraction', 0.5)
    iris_color = iris.get('color', 0x001F)
    iris_angle = iris.get('angle', 0)
    iris_spin = iris.get('spin', 0)

    sclera_color = sclera.get('color', 0xFFFF)
    sclera_angle = sclera.get('angle', 0)
    sclera_spin = sclera.get('spin', 0)

    if iris_tex_name:
        iris_tex = f'{{ {iris_tex_name}, {iris_tex_w}, {iris_tex_h} }}'
    else:
        iris_tex = '{ nullptr, 0, 0 }'

    if sclera_tex_name:
        sclera_tex = f'{{ {sclera_tex_name}, {sclera_tex_w}, {sclera_tex_h} }}'
    else:
        sclera_tex = '{ nullptr, 0, 0 }'

    return [
        '  const EyeDefinition eye PROGMEM = {',
        f'      "{eye_name}", {radius_frac}, {back_color},'
        f' {squint}, nullptr,',
        f'      {{ {pupil_color}, {pupil_slit},'
        f' {pupil_min}, {pupil_max} }},',
        f'      {{ {iris_rad}, {iris_tex},'
        f' {iris_color}, {iris_angle}, {iris_spin}, 0, 0 }},',
        f'      {{ {sclera_tex}, {sclera_color},'
        f' {sclera_angle}, {sclera_spin}, 0, 0 }},',
        f'      {{ upper, lower, {lid_color},'
        f' {normal_closure_str}, {wide_closure_str}, {tracking} }},',
        f'      {{ {map_radius}, nullptr, nullptr }}',
        '  };',
    ]


def _build_eyelid_table_lines(label: str, table: list, width: int) -> list:
    """Return PROGMEM array lines for one eyelid table."""
    lines = [
        f'  // {label} eyelid lookup table ({width} columns, 0-255 range)',
        f'  const uint8_t {label}[{width * 2}] PROGMEM = {{',
    ]
    for i in range(0, width, 8):
        parts = [
            f'{s}, {e}'
            for j in range(8)
            if (idx := i + j) < width
            for s, e in [table[idx]]
        ]
        if parts:
            lines.append('    ' + ', '.join(parts) + ',')
    lines.append('  };')
    return lines


def generate_header(config: dict, eye_file: Path, output_path: Path, screen_size: int):
    """Generate a complete eye header file from an .eye config dict."""
    size_info = SCREEN_SIZES.get(screen_size, SCREEN_SIZES[480])
    width = size_info['width']
    height = size_info['height']
    eye_radius = size_info['eye_radius']
    map_radius = size_info['map_radius']

    namespace = eye_file.parent.name.replace(' ', '_').replace('-', '_')
    eyelid_config = config.get('eyelid', {})

    # ---- Eyelid tables ----
    upper_filename = eyelid_config.get('upperFilename')
    lower_filename = eyelid_config.get('lowerFilename')

    if upper_filename and lower_filename:
        upper_path = eye_file.parent / upper_filename
        lower_path = eye_file.parent / lower_filename
        if upper_path.exists() and lower_path.exists():
            print(f"    Loading custom eyelids from {upper_filename},"
                  f" {lower_filename}")
            upper_table = convert_eyelid_image(upper_path, width, height, True)
            lower_table = convert_eyelid_image(lower_path, width, height, False)
        else:
            print("    Warning: Eyelid files not found, using circular defaults")
            upper_table = generate_no_eyelids(width)
            lower_table = generate_no_eyelids(width)
    else:
        print("    Using circular defaults")
        upper_table = generate_no_eyelids(width)
        lower_table = generate_no_eyelids(width)

    # Suppress unused warning — eye_radius used implicitly via size_info lookup
    _ = eye_radius

    # ---- Closure values ----
    # When the .eye file omits a field, use the value from EyeMovement.h and
    # emit the C++ macro name so the header stays in sync without regeneration.
    normal_default, wide_default = parse_eyemovement_defaults()

    normal_from_file = 'normalClosure' in eyelid_config
    wide_from_file = 'wideClosure' in eyelid_config

    normal_closure = eyelid_config.get('normalClosure', normal_default)
    wide_closure = eyelid_config.get('wideClosure', wide_default)

    normal_closure_str = (
        str(normal_closure) if normal_from_file
        else 'EYELID_NORMAL_CLOSURE_DEFAULT'
    )
    wide_closure_str = (
        str(wide_closure) if wide_from_file
        else 'EYELID_WIDE_CLOSURE_DEFAULT'
    )
    need_eyemovement_include = not normal_from_file or not wide_from_file

    # ---- Iris / sclera textures ----
    iris_cfg = config.get('iris', {})
    sclera_cfg = config.get('sclera', {})

    def _load_texture(filename_key: str, cfg: dict, label: str):
        """Return (data, w, h) or (None, 0, 0) if the file is absent."""
        fname = cfg.get(filename_key)
        if not fname:
            return None, 0, 0
        tex_path = eye_file.parent / fname
        if tex_path.exists():
            print(f"    Loading {label} texture from {fname}")
            max_w = cfg.get('maxTexW', MAX_TEX_W)
            max_h = cfg.get('maxTexH', MAX_TEX_H)
            data, tw, th = convert_texture_to_rgb565(tex_path, max_w, max_h)
            return data, tw, th
        print(f"    Warning: {label} texture not found: {tex_path}")
        return None, 0, 0

    iris_tex_data, iris_tex_w, iris_tex_h = _load_texture(
        'filename', iris_cfg, 'iris')
    sclera_tex_data, sclera_tex_w, sclera_tex_h = _load_texture(
        'filename', sclera_cfg, 'sclera')

    # ---- Assemble header ----
    lines = ['#pragma once', '']
    lines.append('#include <Arduino.h>')
    lines.append('#include "eyes.h"')
    if need_eyemovement_include:
        lines.append('#include "animation/EyeMovement.h"')
    lines.append('')
    lines.append(f'namespace {namespace} {{')
    lines.append('')

    lines.extend(_build_eyelid_table_lines('upper', upper_table, width))
    lines.append('')
    lines.extend(_build_eyelid_table_lines('lower', lower_table, width))
    lines.append('')

    if iris_tex_data:
        lines.extend(_build_texture_array_lines(
            'iris_texture', iris_tex_data, iris_tex_w, iris_tex_h))
        lines.append('')
    if sclera_tex_data:
        lines.extend(_build_texture_array_lines(
            'sclera_texture', sclera_tex_data, sclera_tex_w, sclera_tex_h))
        lines.append('')

    lines.extend(_build_eye_definition_lines(
        config, eyelid_config, map_radius,
        normal_closure_str, wide_closure_str,
        iris_tex_name='iris_texture' if iris_tex_data else '',
        iris_tex_w=iris_tex_w,
        iris_tex_h=iris_tex_h,
        sclera_tex_name='sclera_texture' if sclera_tex_data else '',
        sclera_tex_w=sclera_tex_w,
        sclera_tex_h=sclera_tex_h,
    ))
    lines.append('}  // namespace')
    lines.append('')

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text('\n'.join(lines), encoding='utf-8')
    print(f"    Written: {output_path}")


def detect_screen_size(eye_file: Path) -> int:
    """Detect screen size from file path or name."""
    path_str = str(eye_file)
    if '466' in path_str:
        return 466
    if '480' in path_str:
        return 480
    if '240' in path_str:
        return 240
    return 480


def _eye_namespace(eye_name: str) -> str:
    """C++ namespace derived from an eye directory name."""
    return eye_name.replace(' ', '_').replace('-', '_')


def _board_block(guard: str, board_name: str, size: int, eye_names: list) -> str:
    """Generate one #if/#elif board section for EyeLibrary.h."""
    includes = '\n'.join(
        f'#include "eyes/{name}_{size}.h"' for name in eye_names
    )
    if eye_names:
        entries = ',\n    '.join(f'&{_eye_namespace(n)}::eye' for n in eye_names)
        registry = f'    {entries}}};'
    else:
        registry = '};'
    count = len(eye_names)
    return (
        f'{includes}\n\n'
        f'/**\n'
        f' * @brief Registry of available eye definitions for {board_name} boards.\n'
        f' */\n'
        f'static const EyeDefinition *const s_eyeRegistry[] = {{\n'
        f'{registry}\n\n'
        f'/**\n'
        f' * @brief Number of eyes in the registry.\n'
        f' */\n'
        f'static constexpr int s_eyeCount = {count};\n\n'
        f'/**\n'
        f' * @brief Get the name of an eye by index.\n'
        f' * @param index Eye index (0 to s_eyeCount-1).\n'
        f' * @return Eye name string, or nullptr if index is out of range.\n'
        f' */\n'
        f'inline const char *getEyeName(int index)\n'
        f'{{\n'
        f'  if (index < 0 || index >= s_eyeCount)\n'
        f'    return nullptr;\n'
        f'  return s_eyeRegistry[index]->name;\n'
        f'}}\n'
    )


def update_eye_library(amoled_eyes: list, trgb_eyes: list):
    """Rewrite EyeLibrary.h from the current valid eye sets."""
    amoled = _board_block('ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED', 'AMOLED', 466, amoled_eyes)
    trgb   = _board_block('ARDUINO_LILYGO_T_RGB',                'T-RGB',  480, trgb_eyes)

    content = (
        '#pragma once\n\n'
        f'#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)\n\n'
        f'{amoled}\n'
        f'#elif defined(ARDUINO_LILYGO_T_RGB)\n\n'
        f'{trgb}\n'
        '#endif\n'
    )
    EYELIBRARY_H.write_text(content, encoding='utf-8', newline='\n')
    print(f"  Updated EyeLibrary.h  ({len(amoled_eyes)} AMOLED,"
          f" {len(trgb_eyes)} T-RGB eyes)")


def sync_eye_library():
    """Remove stale include/eyes headers and rewrite EyeLibrary.h.

    A header is stale if no .eye source file exists that would generate it.
    EyeLibrary.h is rebuilt from whatever valid headers remain after cleanup.
    """
    eye_configs = find_eye_configs()

    # Build the expected stem set and per-size eye name lists.
    expected_stems: set = set()
    eyes_by_size: dict = {466: [], 480: []}
    seen: dict = {466: set(), 480: set()}

    for ec in sorted(eye_configs, key=lambda e: e['name']):
        stem = f"{ec['name']}_{ec['screen_size']}"
        expected_stems.add(stem)
        size = ec['screen_size']
        if size in eyes_by_size and ec['name'] not in seen[size]:
            eyes_by_size[size].append(ec['name'])
            seen[size].add(ec['name'])

    # Remove headers with no matching .eye source.
    stale = [h for h in INCLUDE_EYES_DIR.glob('*.h')
             if h.stem not in expected_stems]
    if stale:
        print("\nRemoving stale eye headers:")
        for h in sorted(stale):
            h.unlink()
            print(f"  Removed: {h.name}")

    update_eye_library(eyes_by_size[466], eyes_by_size[480])


def main():
    """Entry point: parse args and dispatch to generate_header."""
    parser = argparse.ArgumentParser(
        description='Generate eye header files from .eye config files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python geneye.py -eye default_eye       Generate header for default_eye
    python geneye.py -all                   Generate all eye headers
    python geneye.py -list                  List available eyes

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
    INCLUDE_EYES_DIR.mkdir(parents=True, exist_ok=True)

    if args.list:
        print("Available eyes:")
        eye_configs = find_eye_configs()
        if not eye_configs:
            print("  No eye configurations found in resources/eyes/")
            return
        for ec in eye_configs:
            print(f"  - {ec['name']} ({ec['screen_size']}x{ec['screen_size']})")
        return

    if args.all:
        print("Generating all eye headers...")
        print(f"  Source: {RESOURCES_EYES_DIR}")
        print(f"  Output: {INCLUDE_EYES_DIR}")
        print()

        eye_configs = find_eye_configs()
        if not eye_configs:
            print("Error: No .eye files found in resources/eyes/")
            sys.exit(1)

        eyes_by_name: dict = {}
        for ec in eye_configs:
            eyes_by_name.setdefault(ec['name'], []).append(ec)

        total = 0
        for eye_name, configs in eyes_by_name.items():
            print(f"Processing: {eye_name}")
            for ec in configs:
                with open(ec['path'], encoding='utf-8') as f:
                    config = json.load(f)
                output_path = INCLUDE_EYES_DIR / f"{eye_name}_{ec['screen_size']}.h"
                generate_header(config, ec['path'], output_path, ec['screen_size'])
                total += 1

        print()
        print(f"Generated {total} eye headers")
        sync_eye_library()
        return

    if args.eye:
        eye_dir = RESOURCES_EYES_DIR / args.eye
        if not eye_dir.exists():
            print(f"Error: Eye directory not found: {eye_dir}")
            sys.exit(1)

        eye_files = list(eye_dir.glob('*.eye'))
        if not eye_files:
            print(f"Error: No .eye file found in {eye_dir}")
            sys.exit(1)

        for config_path in eye_files:
            print(f"Generating header for {args.eye}")
            with open(config_path, encoding='utf-8') as f:
                config = json.load(f)
            screen_size = detect_screen_size(config_path)
            output_path = INCLUDE_EYES_DIR / f"{args.eye}_{screen_size}.h"
            generate_header(config, config_path, output_path, screen_size)

        sync_eye_library()
        print(f"Done! Output in: {INCLUDE_EYES_DIR}")


if __name__ == '__main__':
    main()
