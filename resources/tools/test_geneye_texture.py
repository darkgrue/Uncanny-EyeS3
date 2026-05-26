#!/usr/bin/env python3
"""Verification tests for geneye.py texture pre-processing changes."""
import sys
from pathlib import Path
import tempfile

sys.path.insert(0, str(Path(__file__).parent))
from geneye import convert_texture_to_rgb565

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"

def test_column_major_output_order():
    """After transpose: col0 pixels come before col1 pixels in the flat array."""
    from PIL import Image
    # 2-column × 3-row image: column 0 = pure red, column 1 = pure blue
    img = Image.new('RGB', (2, 3))
    for y in range(3):
        img.putpixel((0, y), (248, 0, 0))   # red  → RGB565 0xF800
        img.putpixel((1, y), (0, 0, 248))   # blue → RGB565 0x001F
    tmp = Path(tempfile.mktemp(suffix='.png'))
    try:
        img.save(tmp)
        data, w, h = convert_texture_to_rgb565(tmp)
        assert w == 2 and h == 3, f"Expected 2x3, got {w}x{h}"
        # Column-major: [col0_r0, col0_r1, col0_r2, col1_r0, col1_r1, col1_r2]
        assert all(v == 0xF800 for v in data[:3]), \
            f"First 3 entries should be col0 (red 0xF800), got {[hex(v) for v in data[:3]]}"
        assert all(v == 0x001F for v in data[3:]), \
            f"Last 3 entries should be col1 (blue 0x001F), got {[hex(v) for v in data[3:]]}"
        print(f"  {PASS}: column-major output order")
    finally:
        tmp.unlink(missing_ok=True)

def test_max_w_max_h_downscaling():
    """max_w/max_h params cap the output dimensions."""
    from PIL import Image
    img = Image.new('RGB', (300, 200))
    tmp = Path(tempfile.mktemp(suffix='.png'))
    try:
        img.save(tmp)
        data, w, h = convert_texture_to_rgb565(tmp, max_w=128, max_h=64)
        assert w <= 128 and h <= 64, \
            f"Expected at most 128x64, got {w}x{h}"
        assert len(data) == w * h, \
            f"Expected {w*h} pixels, got {len(data)}"
        print(f"  {PASS}: max_w/max_h downscaling → {w}x{h}")
    finally:
        tmp.unlink(missing_ok=True)

def test_default_size_unchanged():
    """No max_w/max_h args: existing behaviour preserved (caps at 256x128)."""
    from PIL import Image
    img = Image.new('RGB', (100, 50))
    tmp = Path(tempfile.mktemp(suffix='.png'))
    try:
        img.save(tmp)
        data, w, h = convert_texture_to_rgb565(tmp)
        assert w == 100 and h == 50, \
            f"Small image should be kept at original size, got {w}x{h}"
        assert len(data) == 100 * 50, f"Expected 5000 pixels, got {len(data)}"
        print(f"  {PASS}: default size unchanged for small image")
    finally:
        tmp.unlink(missing_ok=True)

if __name__ == '__main__':
    print("Running geneye texture tests...")
    test_column_major_output_order()
    test_max_w_max_h_downscaling()
    test_default_size_unchanged()
    print("All tests passed.")
