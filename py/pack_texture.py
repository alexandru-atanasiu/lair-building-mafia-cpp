"""Packs a PNG into the engine's texture format (.tex). Invoked by cook.bat once per file.

Usage: python pack_texture.py <input.png> <output.tex>

Output layout (little endian), mirrored by include/render/texture_file.h:

    char[4] magic       "DXTX"
    u32     version     1
    u32     format      1 = RGBA8 UNORM, premultiplied alpha
    u32     width       of mip 0, in pixels
    u32     height
    u32     mipCount    full chain down to 1x1
    then mipCount levels, largest first. Level i is max(1, width >> i) x max(1, height >> i)
    pixels, rows top to bottom, 4 bytes per pixel (R, G, B, A), tightly packed.

Alpha is premultiplied here so bilinear filtering and the mip chain don't bleed the color of
fully transparent pixels into visible edges. Mips are box-filtered from mip 0.
"""

import os
import struct
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("pack_texture.py: Pillow is required (python -m pip install pillow)")

MAGIC = b"DXTX"
VERSION = 1
FORMAT_RGBA8_UNORM_PREMULTIPLIED = 1
MAX_SIZE = 16384  # D3D11 2D texture limit


def mip_count(width: int, height: int) -> int:
    return max(width, height).bit_length()


def pack(src_path: str, dst_path: str) -> None:
    with Image.open(src_path) as img:
        if img.format != "PNG":
            raise ValueError(f"not a PNG file (format {img.format})")
        # Handles every PNG color type (palette, grayscale, RGB, 16-bit, tRNS transparency).
        base = img.convert("RGBA").convert("RGBa")

    width, height = base.size
    if not (1 <= width <= MAX_SIZE and 1 <= height <= MAX_SIZE):
        raise ValueError(f"size {width}x{height} out of range (1..{MAX_SIZE})")

    count = mip_count(width, height)
    levels = [base.tobytes()]
    for i in range(1, count):
        size = (max(1, width >> i), max(1, height >> i))
        levels.append(base.resize(size, Image.Resampling.BOX).tobytes())

    dst_dir = os.path.dirname(dst_path)
    if dst_dir:
        os.makedirs(dst_dir, exist_ok=True)
    tmp_path = dst_path + ".tmp"
    with open(tmp_path, "wb") as f:
        f.write(struct.pack("<4s5I", MAGIC, VERSION, FORMAT_RGBA8_UNORM_PREMULTIPLIED, width, height, count))
        for level in levels:
            f.write(level)
    os.replace(tmp_path, dst_path)  # never leave a half-written .tex behind

    total = sum(len(level) for level in levels)
    print(f"  {os.path.basename(src_path)} -> {width}x{height}, {count} mips, {total / 1024:.0f} KiB")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.splitlines()[2], file=sys.stderr)
        return 2
    src_path, dst_path = sys.argv[1], sys.argv[2]
    try:
        pack(src_path, dst_path)
    except Exception as e:  # report and fail the cook, no traceback noise
        print(f"  {src_path}: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
