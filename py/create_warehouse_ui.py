"""Draws the warehouse panel's icons into assets/ui/icons/ with Pillow. Run from anywhere, then cook.bat.

White on clear, like py/create_ui_assets.py: the UI tints them.
"""

import os

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "ui", "icons")

ICON = 64
WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)


def icon_truck(d):
    d.rectangle((4, 14, 38, 44), fill=WHITE)
    d.polygon([(42, 22), (52, 22), (60, 34), (60, 44), (42, 44)], fill=WHITE)
    d.polygon([(46, 26), (51, 26), (55, 33), (46, 33)], fill=CLEAR)
    for cx in (16, 48):
        d.ellipse((cx - 9, 37, cx + 9, 55), fill=CLEAR)
        d.ellipse((cx - 6, 40, cx + 6, 52), fill=WHITE)


def icon_crate(d):
    d.rectangle((8, 10, 56, 54), fill=WHITE)
    d.rectangle((14, 16, 50, 48), fill=CLEAR)
    d.line((14, 16, 50, 48), fill=WHITE, width=6)
    d.line((14, 48, 50, 16), fill=WHITE, width=6)


ICONS = {
    "truck": icon_truck,
    "crate": icon_crate,
}


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, draw in ICONS.items():
        img = Image.new("RGBA", (ICON, ICON), CLEAR)
        draw(ImageDraw.Draw(img))
        path = os.path.join(OUT_DIR, name + ".png")
        img.save(path)
        print(f"Saved {path}")


main()
