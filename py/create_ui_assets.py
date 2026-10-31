"""Creates placeholder 2D UI assets under assets/ui/ with Pillow.

Run once (or whenever the placeholders should be regenerated), from the repository root:

    python py/create_ui_assets.py

Then run cook.bat: every assets/<path>.png becomes __generated/data/<path>.tex, which the game
gets with Ui::Texture("<path>.tex"). To add an icon, write a function drawing into a 64 x 64
image below and add it to ICONS. Icons are white with alpha, so widgets can tint them.

Outputs:
  white.png            plain white (flat fills)
  icons/<name>.png     HUD icons
  city_map.png         the city screen's placeholder map
"""

import os

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "ui")

ICON = 64
WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)


def icon_warehouse(d):
    d.polygon([(6, 28), (32, 8), (58, 28)], fill=WHITE)
    d.rectangle((10, 30, 54, 56), fill=WHITE)
    d.rectangle((20, 38, 44, 56), fill=CLEAR)
    for y in (43, 49):
        d.line((20, y, 44, y), fill=WHITE, width=2)


def icon_lair(d):
    d.rectangle((4, 14, 60, 22), fill=WHITE)  # the ground
    d.rectangle((14, 22, 22, 36), fill=WHITE)  # shaft
    d.rectangle((8, 36, 56, 56), fill=WHITE)  # room
    d.rectangle((13, 41, 51, 51), fill=CLEAR)
    d.rectangle((30, 36, 34, 56), fill=WHITE)


def icon_city(d):
    for x0, y0, x1 in ((6, 30, 20), (24, 12, 40), (44, 24, 58)):
        d.rectangle((x0, y0, x1, 56), fill=WHITE)
        for y in range(y0 + 5, 50, 9):
            for x in range(x0 + 3, x1 - 3, 6):
                d.rectangle((x, y, x + 2, y + 3), fill=CLEAR)


def icon_pause(d):
    d.rectangle((16, 12, 26, 52), fill=WHITE)
    d.rectangle((38, 12, 48, 52), fill=WHITE)


def icon_money(d):
    d.ellipse((8, 8, 56, 56), fill=WHITE)
    d.rectangle((29, 14, 35, 50), fill=CLEAR)
    d.arc((20, 18, 44, 34), 90, 330, fill=CLEAR, width=5)
    d.arc((20, 30, 44, 46), 270, 150, fill=CLEAR, width=5)


def icon_heat(d):
    d.polygon([(32, 4), (46, 26), (52, 40), (46, 54), (32, 60), (18, 54), (12, 40), (20, 26), (24, 34)], fill=WHITE)
    d.polygon([(32, 32), (40, 44), (32, 54), (24, 44)], fill=CLEAR)


def icon_crew(d):
    d.ellipse((22, 6, 42, 26), fill=WHITE)
    d.pieslice((10, 30, 54, 90), 180, 360, fill=WHITE)


ICONS = {
    "warehouse": icon_warehouse,
    "lair": icon_lair,
    "city": icon_city,
    "pause": icon_pause,
    "money": icon_money,
    "heat": icon_heat,
    "crew": icon_crew,
}


def make_city_map():
    """A flat grid of city blocks in muted colors, with grey roads and a river."""
    w, h = 1280, 800
    img = Image.new("RGBA", (w, h), (70, 72, 76, 255))
    d = ImageDraw.Draw(img)
    colors = [(92, 88, 80, 255), (84, 92, 84, 255), (96, 84, 80, 255), (82, 86, 96, 255)]
    block, road = 96, 16
    for row, y in enumerate(range(road, h, block + road)):
        for col, x in enumerate(range(road, w, block + road)):
            d.rectangle((x, y, x + block, y + block), fill=colors[(row * 3 + col * 5) % len(colors)])
            for inset, shade in ((14, 10), (30, 18)):
                r, g, b, a = colors[(row * 3 + col * 5) % len(colors)]
                d.rectangle((x + inset, y + inset, x + block - inset, y + block - inset),
                            fill=(r + shade, g + shade, b + shade, a))
    d.polygon([(0, 640), (w, 520), (w, 620), (0, 740)], fill=(52, 72, 92, 255))
    return img


def save(img, rel):
    path = os.path.join(OUT_DIR, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"Saved {path}")


def main():
    save(Image.new("RGBA", (8, 8), WHITE), "white.png")
    for name, draw in ICONS.items():
        img = Image.new("RGBA", (ICON, ICON), CLEAR)
        draw(ImageDraw.Draw(img))
        save(img, os.path.join("icons", name + ".png"))
    save(make_city_map(), "city_map.png")


main()
