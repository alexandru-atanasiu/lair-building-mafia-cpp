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


# The lair's build menu: tools, rooms, objects.

def icon_tool_dig(d):
    d.line((14, 54, 44, 18), fill=WHITE, width=7)  # the handle
    d.arc((14, 2, 62, 44), 190, 350, fill=WHITE, width=8)  # the pick's head


def icon_tool_rooms(d):
    d.rectangle((6, 10, 58, 54), outline=WHITE, width=5)
    d.line((30, 10, 30, 30), fill=WHITE, width=5)
    d.line((30, 40, 30, 54), fill=WHITE, width=5)
    d.line((30, 30, 58, 30), fill=WHITE, width=5)


def icon_tool_door(d):
    d.rectangle((14, 4, 50, 60), outline=WHITE, width=5)
    d.polygon([(19, 9), (41, 15), (41, 60), (19, 56)], fill=WHITE)
    d.ellipse((33, 34, 38, 39), fill=CLEAR)


def icon_tool_objects(d):
    d.rectangle((8, 26, 56, 36), fill=WHITE)  # a table
    d.rectangle((12, 36, 19, 58), fill=WHITE)
    d.rectangle((45, 36, 52, 58), fill=WHITE)
    d.rectangle((18, 8, 38, 26), fill=WHITE)  # a box on it
    d.rectangle((23, 13, 33, 20), fill=CLEAR)


def icon_tool_delete(d):
    d.line((12, 12, 52, 52), fill=WHITE, width=9)
    d.line((52, 12, 12, 52), fill=WHITE, width=9)


def icon_room_bunks(d):
    d.rectangle((6, 8, 13, 58), fill=WHITE)
    d.rectangle((51, 8, 58, 58), fill=WHITE)
    for y in (14, 38):
        d.rectangle((13, y, 51, y + 10), fill=WHITE)
        d.rectangle((16, y - 6, 28, y), fill=WHITE)


def icon_room_bar(d):
    d.polygon([(8, 8), (56, 8), (34, 32), (34, 50), (46, 54), (46, 58), (18, 58), (18, 54), (30, 50), (30, 32)], fill=WHITE)
    d.polygon([(19, 13), (45, 13), (39, 20), (25, 20)], fill=CLEAR)


def icon_room_storage(d):
    for x0, y0, x1, y1 in ((6, 32, 30, 58), (34, 32, 58, 58), (20, 6, 44, 28)):
        d.rectangle((x0, y0, x1, y1), fill=WHITE)
        d.rectangle(((x0 + x1) // 2 - 2, y0, (x0 + x1) // 2 + 2, y0 + 9), fill=CLEAR)


def icon_room_armoury(d):
    d.polygon([(4, 18), (46, 18), (46, 14), (60, 14), (60, 26), (34, 26), (28, 34), (24, 54), (10, 54), (14, 28), (4, 28)], fill=WHITE)
    d.arc((26, 24, 40, 40), 0, 180, fill=WHITE, width=3)


def icon_room_office(d):
    d.rectangle((4, 22, 60, 30), fill=WHITE)  # desk
    d.rectangle((8, 30, 24, 58), fill=WHITE)
    d.rectangle((44, 30, 56, 58), fill=WHITE)
    d.rectangle((30, 8, 34, 22), fill=WHITE)  # lamp
    d.pieslice((20, 0, 44, 22), 180, 360, fill=WHITE)


def icon_room_vault(d):
    d.ellipse((6, 6, 58, 58), outline=WHITE, width=7)
    d.ellipse((24, 24, 40, 40), fill=WHITE)
    for a, b, c, e in ((32, 14, 32, 22), (32, 42, 32, 50), (14, 32, 22, 32), (42, 32, 50, 32)):
        d.line((a, b, c, e), fill=WHITE, width=5)


def icon_obj_bunk(d):
    d.rectangle((4, 8, 10, 58), fill=WHITE)
    d.rectangle((54, 8, 60, 58), fill=WHITE)
    for y in (12, 36):
        d.rectangle((10, y + 8, 54, y + 16), fill=WHITE)
        d.rectangle((12, y, 26, y + 8), fill=WHITE)


def icon_obj_footlocker(d):
    d.rectangle((6, 22, 58, 54), fill=WHITE)
    d.rectangle((6, 28, 58, 32), fill=CLEAR)
    d.rectangle((28, 24, 36, 38), fill=CLEAR)
    d.rectangle((30, 26, 34, 36), fill=WHITE)


def icon_obj_counter(d):
    d.rectangle((2, 30, 62, 38), fill=WHITE)
    d.rectangle((6, 38, 58, 58), fill=WHITE)
    for x in (14, 28, 42):
        d.rectangle((x, 16, x + 7, 30), fill=WHITE)
        d.rectangle((x + 2, 8, x + 5, 16), fill=WHITE)


def icon_obj_stool(d):
    d.ellipse((14, 8, 50, 22), fill=WHITE)
    d.line((22, 20, 14, 58), fill=WHITE, width=5)
    d.line((42, 20, 50, 58), fill=WHITE, width=5)
    d.line((18, 42, 46, 42), fill=WHITE, width=4)


def icon_obj_table(d):
    d.ellipse((4, 14, 60, 34), fill=WHITE)
    d.rectangle((28, 28, 36, 54), fill=WHITE)
    d.rectangle((16, 52, 48, 58), fill=WHITE)


def icon_obj_shelf(d):
    d.rectangle((6, 4, 11, 60), fill=WHITE)
    d.rectangle((53, 4, 58, 60), fill=WHITE)
    for y in (18, 36, 54):
        d.rectangle((6, y, 58, y + 4), fill=WHITE)
    for x0, y0, x1 in ((16, 6, 28), (34, 24, 48), (15, 42, 25), (31, 42, 44)):
        d.rectangle((x0, y0, x1, y0 + 12), fill=WHITE)


def icon_obj_gun_rack(d):
    d.rectangle((4, 10, 60, 16), fill=WHITE)
    d.rectangle((4, 46, 60, 52), fill=WHITE)
    for x in (12, 28, 44):
        d.rectangle((x, 4, x + 5, 58), fill=WHITE)
        d.rectangle((x - 2, 28, x + 8, 40), fill=WHITE)


def icon_obj_workbench(d):
    d.rectangle((2, 26, 62, 34), fill=WHITE)
    d.rectangle((6, 34, 13, 60), fill=WHITE)
    d.rectangle((51, 34, 58, 60), fill=WHITE)
    d.rectangle((14, 12, 28, 26), fill=WHITE)  # a vice
    d.rectangle((18, 6, 24, 12), fill=WHITE)
    d.line((36, 24, 52, 8), fill=WHITE, width=5)  # a wrench


def icon_obj_boss_desk(d):
    d.rectangle((2, 24, 62, 32), fill=WHITE)
    d.rectangle((6, 32, 22, 58), fill=WHITE)
    d.rectangle((42, 32, 58, 58), fill=WHITE)
    d.rectangle((26, 18, 30, 24), fill=WHITE)
    d.polygon([(18, 6), (38, 6), (34, 18), (22, 18)], fill=WHITE)


def icon_obj_boss_chair(d):
    d.rectangle((14, 4, 50, 32), fill=WHITE)
    d.rectangle((8, 30, 56, 40), fill=WHITE)
    d.rectangle((28, 40, 36, 52), fill=WHITE)
    d.polygon([(32, 50), (50, 60), (14, 60)], fill=WHITE)


def icon_obj_safe(d):
    d.rectangle((6, 6, 58, 54), fill=WHITE)
    d.rectangle((12, 54, 22, 60), fill=WHITE)
    d.rectangle((42, 54, 52, 60), fill=WHITE)
    d.ellipse((20, 18, 44, 42), fill=CLEAR)
    d.ellipse((27, 25, 37, 35), fill=WHITE)


ICONS = {
    "tool_dig": icon_tool_dig,
    "tool_rooms": icon_tool_rooms,
    "tool_door": icon_tool_door,
    "tool_objects": icon_tool_objects,
    "tool_delete": icon_tool_delete,
    "room_bunks": icon_room_bunks,
    "room_bar": icon_room_bar,
    "room_storage": icon_room_storage,
    "room_armoury": icon_room_armoury,
    "room_office": icon_room_office,
    "room_vault": icon_room_vault,
    "obj_bunk": icon_obj_bunk,
    "obj_footlocker": icon_obj_footlocker,
    "obj_counter": icon_obj_counter,
    "obj_stool": icon_obj_stool,
    "obj_table": icon_obj_table,
    "obj_shelf": icon_obj_shelf,
    "obj_gun_rack": icon_obj_gun_rack,
    "obj_workbench": icon_obj_workbench,
    "obj_boss_desk": icon_obj_boss_desk,
    "obj_boss_chair": icon_obj_boss_chair,
    "obj_safe": icon_obj_safe,
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
