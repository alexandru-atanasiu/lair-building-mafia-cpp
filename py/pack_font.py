"""Bakes a TrueType font into a glyph atlas and a metrics file. Invoked by cook.bat once per file.

Usage: python pack_font.py <input.ttf> <output stem> <intermediate atlas.png>

Writes <stem>.tex (the atlas, packed by py/pack_texture.py like any PNG) and <stem>.font. The
font is rasterized once per size in STRIKE_SIZES: the game picks the size matching the UI text
size times the DPI scale, so text stays sharp without a special shader.

.font layout (little endian), mirrored by src/game/ui/ui_font.cpp:

    char[4] magic        "LFNT"
    u32     version      1
    u32     strikeCount
    then per strike:
        u32 pixelSize
        f32 ascent       baseline below the top of a line, pixels
        f32 lineHeight   pixels
        u32 glyphCount
        then per glyph:
            u32 codepoint
            u16 x, y, w, h   atlas rectangle, pixels
            i16 xOffset      left of the rectangle relative to the pen, pixels
            i16 yOffset      top of the rectangle relative to the baseline (negative is up)
            f32 advance      pixels
"""

import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pack_texture  # noqa: E402

MAGIC = b"LFNT"
VERSION = 1
STRIKE_SIZES = (12, 14, 16, 18, 20, 24, 28, 32, 40, 48)
# Printable ASCII, Latin-1 and a little typographic punctuation.
CODEPOINTS = (list(range(0x20, 0x7F)) + list(range(0xA1, 0x100)) +
              [0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026])
PADDING = 2  # around each glyph, so filtering and the first mips don't touch a neighbor
ATLAS_WIDTH = 1024


def bake(ttf_path):
    """Returns (atlas image, [(size, ascent, line height, [glyph tuples])])."""
    glyphs_to_place = []
    strikes = []
    for size in STRIKE_SIZES:
        font = ImageFont.truetype(ttf_path, size)
        ascent, descent = font.getmetrics()
        glyphs = []
        for cp in CODEPOINTS:
            ch = chr(cp)
            left, top, right, bottom = font.getbbox(ch, anchor="ls")
            glyph = {"cp": cp, "font": font, "left": left, "top": top, "w": right - left, "h": bottom - top,
                     "advance": font.getlength(ch), "x": 0, "y": 0}
            glyphs.append(glyph)
            if glyph["w"] > 0 and glyph["h"] > 0:
                glyphs_to_place.append(glyph)
        strikes.append((size, float(ascent), float(ascent + descent), glyphs))

    # Shelves, tallest first.
    glyphs_to_place.sort(key=lambda g: -g["h"])
    x = y = shelf = PADDING
    y = PADDING
    shelf = 0
    for glyph in glyphs_to_place:
        if x + glyph["w"] + PADDING > ATLAS_WIDTH:
            x = PADDING
            y += shelf + PADDING
            shelf = 0
        glyph["x"], glyph["y"] = x, y
        x += glyph["w"] + PADDING
        shelf = max(shelf, glyph["h"])
    height = 1
    while height < y + shelf + PADDING:
        height *= 2

    coverage = Image.new("L", (ATLAS_WIDTH, height), 0)
    draw = ImageDraw.Draw(coverage)
    for glyph in glyphs_to_place:
        draw.text((glyph["x"] - glyph["left"], glyph["y"] - glyph["top"]), chr(glyph["cp"]),
                  font=glyph["font"], fill=255, anchor="ls")

    # White with the coverage as alpha: the game tints it.
    atlas = Image.merge("RGBA", (Image.new("L", coverage.size, 255), Image.new("L", coverage.size, 255),
                                 Image.new("L", coverage.size, 255), coverage))
    return atlas, strikes


def write_font(path, strikes):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    tmp_path = path + ".tmp"
    with open(tmp_path, "wb") as f:
        f.write(struct.pack("<4s2I", MAGIC, VERSION, len(strikes)))
        for size, ascent, line_height, glyphs in strikes:
            f.write(struct.pack("<I2fI", size, ascent, line_height, len(glyphs)))
            for g in glyphs:
                f.write(struct.pack("<I4H2hf", g["cp"], g["x"], g["y"], g["w"], g["h"], g["left"], g["top"], g["advance"]))
    os.replace(tmp_path, path)


def main() -> int:
    if len(sys.argv) != 4:
        print(__doc__.splitlines()[2], file=sys.stderr)
        return 2
    ttf_path, stem, png_path = sys.argv[1:]
    try:
        atlas, strikes = bake(ttf_path)
        os.makedirs(os.path.dirname(png_path) or ".", exist_ok=True)
        atlas.save(png_path)
        pack_texture.pack(png_path, stem + ".tex")
        # Written last: cook.py checks the .font to decide whether the font is up to date.
        write_font(stem + ".font", strikes)
    except Exception as e:  # report and fail the cook, no traceback noise
        print(f"  {ttf_path}: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
