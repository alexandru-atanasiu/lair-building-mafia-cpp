"""Draws the city map and its icons with Pillow, and writes the road graph and sites for the game.

Run from the repository root, then run cook.bat and build:

    python py/create_city_art.py

Outputs:
  assets/ui/city/map.png              the map (MAP_W x MAP_H), flat top-down art
  assets/ui/city/markers/<name>.png   mission icons, white with alpha (widgets tint them)
  assets/ui/city/*.png                marker disc, cars (8 headings), portrait layers
  src/game/sim/city_map_data.h        GENERATED: roads, sites, districts in map pixels

The map is one handmade table: CELLS says what fills each city block, between the avenues XS and
the streets YS. Roads run around every block on land, and only cross the river at BRIDGES. To add
a place missions can happen at, add a line to SITES.
"""

import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "ui", "city")
HEADER = os.path.join(ROOT, "src", "game", "sim", "city_map_data.h")

MAP_W, MAP_H = 2048, 1536
SS = 2  # drawn at twice the size and filtered down, for clean diagonals

XS = [60, 230, 420, 590, 770, 980, 1140, 1310, 1500, 1660, 1840, 1990]
YS = [50, 190, 340, 480, 630, 780, 930, 1080, 1220]

# One letter per block, rows top to bottom:
#   U Uptown (villas and lawns)   N Northside (terraces)  M Midtown (towers)  L Little Sicily (tile roofs)
#   O Old Town (brick)            I Ironworks (sheds)     D the Docks         S Seaview (hotels)
#   P park   R river   ~ open water
CELLS = [
    "UUUPPNNRNNN",
    "UUUPPNNRNNN",
    "UUUMMMMRSSS",
    "LLLMMMPRSSS",
    "LLLMMMMRSSS",
    "IILLOOORSS~",
    "IIIOOOORS~~",
    "DDDDDOO~~~~",
]

# Bridges: the street (index into YS) that crosses the river.
BRIDGES = {2, 5}

# The boulevard, cutting across the grid through these (column, row) intersections of XS and YS.
BOULEVARD = [(3, 5), (4, 4), (5, 3), (6, 2)]

WAREHOUSE = {"cell": (1, 6), "u": 0.5, "v": 0.5}

DISTRICTS = [
    ("Uptown", 1.5, 1.5),
    ("Northside", 9.5, 0.5),
    ("Midtown", 3.5, 2.5),
    ("Little Sicily", 1.5, 3.5),
    ("Old Town", 5.5, 6.5),
    ("Ironworks", 0.5, 5.5),
    ("The Docks", 2.5, 7.5),
    ("Seaview", 9.5, 4.5),
]

# Name, location type, block (column, row), position in the block (0..1).
SITES = [
    # Shops and bars to lean on
    ("Rosa's Bakery", "Shop", (2, 3), 0.3, 0.3),
    ("Marino's Butcher", "Shop", (1, 4), 0.7, 0.3),
    ("Lucky Laundry", "Shop", (3, 5), 0.3, 0.7),
    ("Ferro's Hardware", "Shop", (4, 6), 0.7, 0.3),
    ("Palace Tailors", "Shop", (4, 3), 0.3, 0.3),
    ("The Blue Note", "Shop", (6, 5), 0.5, 0.3),
    ("Corner Pharmacy", "Shop", (5, 1), 0.3, 0.7),
    ("Seaview Arcade", "Shop", (9, 3), 0.7, 0.3),
    # Roads where trucks pass
    ("Foundry Road", "TruckRoute", (0, 5), 0.8, 0.8),
    ("Old Mill Depot", "TruckRoute", (2, 6), 0.8, 0.2),
    ("Northern Freight Yard", "TruckRoute", (9, 0), 0.5, 0.2),
    ("River Road Depot", "TruckRoute", (6, 6), 0.8, 0.8),
    ("Hilltop Truck Stop", "TruckRoute", (0, 2), 0.2, 0.8),
    # Water's edge
    ("Pier 4", "Dock", (1, 7), 0.5, 0.8),
    ("Pier 9", "Dock", (3, 7), 0.5, 0.8),
    ("Customs Shed", "Dock", (4, 7), 0.7, 0.3),
    ("Fisherman's Quay", "Dock", (6, 7), 0.3, 0.7),
    ("Seaview Marina", "Dock", (8, 6), 0.4, 0.75),
    # Houses with something in them
    ("The Vanderholt House", "Residence", (0, 0), 0.5, 0.5),
    ("Maple Court", "Residence", (2, 1), 0.3, 0.3),
    ("Judge Whitlock's Villa", "Residence", (1, 2), 0.5, 0.3),
    ("Terrace Row", "Residence", (6, 1), 0.7, 0.3),
    ("Grand Marine Hotel", "Residence", (10, 2), 0.5, 0.5),
    # Money
    ("First Merchants Bank", "Bank", (4, 2), 0.5, 0.5),
    ("Harbour Savings", "Bank", (5, 6), 0.3, 0.3),
    ("Goldman's Jewellers", "Bank", (4, 4), 0.3, 0.7),
    ("Northside Trust", "Bank", (8, 1), 0.5, 0.5),
    # People who can be bought
    ("City Hall", "Office", (5, 4), 0.5, 0.5),
    ("11th Precinct", "Office", (2, 5), 0.5, 0.3),
    ("County Courthouse", "Office", (6, 4), 0.5, 0.4),
    # Where men look for work
    ("Sully's Bar", "Bar", (2, 4), 0.3, 0.7),
    ("The Anchor", "Bar", (5, 5), 0.7, 0.8),
    ("Boxing Club", "Bar", (8, 4), 0.3, 0.5),
]

WATER = (47, 66, 82)
WATER_DEEP = (40, 58, 74)
WATER_LINE = (58, 79, 96)
SAND = (156, 144, 116)
LAND = (92, 90, 84)
ROAD = (50, 50, 54)
ROAD_EDGE = (118, 114, 104)
ROAD_MARK = (150, 134, 92)
QUAY = (104, 104, 104)
LAWN = (86, 100, 74)
LAWN_DARK = (72, 88, 64)
TREE = (62, 82, 58)
TREE_LIGHT = (76, 96, 68)

STYLES = {
    "U": {"ground": LAWN, "roofs": [(132, 104, 90), (120, 108, 100), (128, 112, 94)], "min": 90, "gap": 22, "fill": 0.55},
    "N": {"ground": (88, 88, 86), "roofs": [(104, 108, 118), (96, 100, 112), (112, 112, 118)], "min": 56, "gap": 8, "fill": 1.0},
    "M": {"ground": (80, 82, 88), "roofs": [(112, 118, 130), (98, 106, 120), (124, 126, 134), (90, 98, 112)], "min": 72, "gap": 12, "fill": 1.0},
    "L": {"ground": (98, 88, 78), "roofs": [(148, 106, 82), (136, 96, 74), (154, 118, 92), (128, 94, 76)], "min": 44, "gap": 6, "fill": 1.0},
    "O": {"ground": (94, 86, 72), "roofs": [(136, 118, 88), (124, 108, 84), (146, 128, 98), (114, 104, 86)], "min": 52, "gap": 7, "fill": 1.0},
    "I": {"ground": (80, 76, 70), "roofs": [(104, 90, 78), (92, 84, 78), (112, 94, 76)], "min": 90, "gap": 14, "fill": 0.9},
    "D": {"ground": (86, 88, 88), "roofs": [(92, 100, 104), (84, 94, 100)], "min": 100, "gap": 16, "fill": 1.0},
    "S": {"ground": (118, 110, 94), "roofs": [(140, 134, 120), (148, 140, 124), (128, 126, 118)], "min": 66, "gap": 12, "fill": 0.92},
}

ICON = 96  # drawn big, cooked with mips


def cell_at(col, row):
    if row < 0 or row >= len(CELLS) or col < 0 or col >= len(CELLS[0]):
        return None
    return CELLS[row][col]


def is_land(kind):
    return kind is not None and kind not in "R~"


def darker(c, f):
    return (int(c[0] * f), int(c[1] * f), int(c[2] * f))


class Painter:
    def __init__(self):
        self.img = Image.new("RGB", (MAP_W * SS, MAP_H * SS), WATER)
        self.d = ImageDraw.Draw(self.img)

    def rect(self, x0, y0, x1, y1, fill):
        self.d.rectangle((x0 * SS, y0 * SS, x1 * SS, y1 * SS), fill=fill)

    def line(self, pts, fill, width):
        self.d.line([(x * SS, y * SS) for x, y in pts], fill=fill, width=int(width * SS), joint="curve")

    def disc(self, x, y, r, fill):
        self.d.ellipse(((x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS), fill=fill)

    def poly(self, pts, fill):
        self.d.polygon([(x * SS, y * SS) for x, y in pts], fill=fill)


def paint_water(p, rng):
    for _ in range(1400):
        x = rng.uniform(0, MAP_W)
        y = rng.uniform(0, MAP_H)
        w = rng.uniform(10, 44)
        p.rect(x, y, x + w, y + 1, WATER_LINE if rng.random() < 0.5 else WATER_DEEP)


def land_mask():
    """Land: blocks, smoothed into a coastline on the open bay, with a straight quay along the Docks."""
    mask = Image.new("L", (MAP_W // 2, MAP_H // 2), 0)
    d = ImageDraw.Draw(mask)
    for row in range(len(CELLS)):
        for col in range(len(CELLS[0])):
            # The river is painted later: its bed counts as land here, it has straight banks.
            if CELLS[row][col] != "~":
                d.rectangle((XS[col] // 2 - 1, YS[row] // 2 - 1, XS[col + 1] // 2 + 1, YS[row + 1] // 2 + 1), fill=255)
    # Land carries on past the map's top, left and right edges.
    d.rectangle((0, 0, MAP_W // 2, YS[0] // 2), fill=255)
    d.rectangle((0, 0, XS[0] // 2, YS[8] // 2), fill=255)
    d.rectangle((XS[11] // 2, 0, MAP_W // 2, YS[6] // 2 - 30), fill=255)
    return mask


def paint_land(p):
    base = land_mask()
    # Rounded: blur, then threshold low enough that the coast stays clear of the road on it.
    soft = base.filter(ImageFilter.GaussianBlur(26))
    big = soft.resize(p.img.size, Image.BICUBIC)
    land = big.point(lambda v: 255 if v > 80 else 0)
    sand = big.point(lambda v: 255 if v > 58 else 0)

    # West of the river's mouth the shore is a built quay, straight.
    quay_land = Image.new("L", p.img.size, 0)
    ImageDraw.Draw(quay_land).rectangle((0, 0, (XS[7] + 20) * SS, (YS[8] + 34) * SS), fill=255)
    # Open water from there to the far bank: the river's mouth.
    mouth = Image.new("L", p.img.size, 255)
    ImageDraw.Draw(mouth).rectangle((0, 0, MAP_W * SS, (YS[8] + 34) * SS), fill=0)
    ImageDraw.Draw(mouth).rectangle(((XS[7] + 20) * SS, YS[7] * SS, (XS[8] - 16) * SS, MAP_H * SS), fill=255)
    land = ImageChops.multiply(land, Image.eval(mouth, lambda v: 255 - v))
    sand = ImageChops.multiply(sand, Image.eval(mouth, lambda v: 255 - v))
    land = ImageChops.lighter(ImageChops.multiply(land, Image.eval(quay_land, lambda v: 255 - v)), quay_land)
    sand = ImageChops.multiply(sand, Image.eval(quay_land, lambda v: 255 - v))

    p.img.paste(SAND, (0, 0), sand)
    p.img.paste(LAND, (0, 0), land)
    # The quay's stone lip.
    p.rect(0, YS[8] + 34, XS[7] + 20, YS[8] + 38, QUAY)


def building(p, rng, x0, y0, x1, y1, roof, ground, style):
    shadow = darker(ground, 0.62)
    off = 5 if style == "M" else 3
    p.rect(x0 + off, y0 + off, x1 + off, y1 + off, shadow)
    p.rect(x0, y0, x1, y1, roof)
    p.line([(x0, y0), (x1, y0)], tuple(min(255, v + 18) for v in roof), 1)
    p.line([(x0, y0), (x0, y1)], tuple(min(255, v + 12) for v in roof), 1)
    w, h = x1 - x0, y1 - y0
    if style == "M":  # a tower: setbacks and plant on the roof
        inset = min(w, h) * 0.18
        p.rect(x0 + inset, y0 + inset, x1 - inset, y1 - inset, tuple(min(255, v + 10) for v in roof))
        if min(w, h) > 40:
            cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
            p.rect(cx - 7, cy - 5, cx + 7, cy + 5, darker(roof, 0.72))
    elif style in ("L", "O", "N"):  # a pitched roof: a ridge along the longer side
        dark = darker(roof, 0.84)
        if w >= h:
            p.rect(x0, (y0 + y1) / 2, x1, y1, dark)
        else:
            p.rect((x0 + x1) / 2, y0, x1, y1, dark)
    elif style in ("I", "D"):  # corrugated sheds
        dark = darker(roof, 0.88)
        if w >= h:
            for x in range(int(x0) + 6, int(x1) - 2, 10):
                p.rect(x, y0 + 1, x + 2, y1 - 1, dark)
        else:
            for y in range(int(y0) + 6, int(y1) - 2, 10):
                p.rect(x0 + 1, y, x1 - 1, y + 2, dark)
    elif style == "S":  # a hotel: a pale roof with a terrace
        p.rect(x0 + 4, y0 + 4, x1 - 4, y0 + h * 0.35, darker(roof, 0.9))
    elif style == "U":  # a villa: hipped roof
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        p.poly([(x0, y1), (x1, y1), (x1 - min(w, h) / 2, cy), (x0 + min(w, h) / 2, cy)], darker(roof, 0.84))
        p.poly([(x1, y0), (x1, y1), (cx if w < h else x1 - min(w, h) / 2, cy)], darker(roof, 0.92))


def split_lots(rng, x0, y0, x1, y1, min_size):
    w, h = x1 - x0, y1 - y0
    if w < min_size * 1.5 and h < min_size * 1.5:
        return [(x0, y0, x1, y1)]
    if w >= h:
        cut = x0 + w * rng.uniform(0.38, 0.62)
        return split_lots(rng, x0, y0, cut, y1, min_size) + split_lots(rng, cut, y0, x1, y1, min_size)
    cut = y0 + h * rng.uniform(0.38, 0.62)
    return split_lots(rng, x0, y0, x1, cut, min_size) + split_lots(rng, x0, cut, x1, y1, min_size)


def trees(p, rng, x0, y0, x1, y1, count, r0=5, r1=10):
    for _ in range(count):
        x, y = rng.uniform(x0 + r1, x1 - r1), rng.uniform(y0 + r1, y1 - r1)
        r = rng.uniform(r0, r1)
        p.disc(x + 2, y + 2, r, LAWN_DARK)
        p.disc(x, y, r, TREE if rng.random() < 0.6 else TREE_LIGHT)


def paint_block(p, rng, kind, x0, y0, x1, y1):
    margin = 13
    x0, y0, x1, y1 = x0 + margin, y0 + margin, x1 - margin, y1 - margin
    if kind == "P":
        p.rect(x0, y0, x1, y1, LAWN)
        return
    style = STYLES[kind]
    p.rect(x0, y0, x1, y1, style["ground"])
    for lot in split_lots(rng, x0, y0, x1, y1, style["min"]):
        gap = style["gap"]
        lx0, ly0, lx1, ly1 = lot[0] + gap / 2, lot[1] + gap / 2, lot[2] - gap / 2, lot[3] - gap / 2
        if kind == "D" and rng.random() < 0.45:
            paint_containers(p, rng, lx0, ly0, lx1, ly1)
            continue
        if rng.random() > style["fill"]:
            if kind == "U":
                trees(p, rng, lx0, ly0, lx1, ly1, 5)
            continue
        if kind == "U":  # the house stands in its garden
            trees(p, rng, lx0, ly0, lx1, ly1, 3, 4, 7)
            sx, sy = (lx1 - lx0) * 0.2, (ly1 - ly0) * 0.2
            lx0, ly0, lx1, ly1 = lx0 + sx, ly0 + sy, lx1 - sx, ly1 - sy
        building(p, rng, lx0, ly0, lx1, ly1, rng.choice(style["roofs"]), style["ground"], kind)


def paint_containers(p, rng, x0, y0, x1, y1):
    colors = [(120, 84, 64), (70, 92, 104), (112, 104, 74), (88, 100, 84), (100, 72, 70)]
    for y in range(int(y0), int(y1) - 10, 14):
        for x in range(int(x0), int(x1) - 26, 30):
            if rng.random() < 0.8:
                c = rng.choice(colors)
                p.rect(x + 2, y + 2, x + 29, y + 13, darker(c, 0.5))
                p.rect(x, y, x + 27, y + 11, c)


def paint_park(p, rng):
    """The one big park: paths, a pond, and trees that keep off them."""
    x0, y0, x1, y1 = XS[3] + 13, YS[0] + 13, XS[5] - 13, YS[2] - 13
    p.rect(x0, y0, x1, y1, LAWN)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    trees(p, rng, x0, y0, x1, y1, 90)
    p.line([(x0, y0), (cx, cy), (x1, y1)], SAND, 5)
    p.line([(x0, y1), (cx, cy), (x1, y0)], SAND, 5)
    p.line([((x0 + x1) / 2, y0), (cx, cy), ((x0 + x1) / 2, y1)], SAND, 5)
    p.disc(cx, cy, 62, SAND)
    p.disc(cx, cy, 55, WATER)
    p.disc(cx, cy, 10, QUAY)
    # The small square in Midtown.
    x0, y0, x1, y1 = XS[6] + 13, YS[3] + 13, XS[7] - 13, YS[4] - 13
    p.rect(x0, y0, x1, y1, LAWN)
    trees(p, rng, x0, y0, x1, y1, 14)


def paint_river(p, rng):
    x0, x1 = XS[7] + 16, XS[8] - 16
    bottom = YS[7] + 40
    p.rect(x0 - 5, -2, x1 + 5, bottom, QUAY)
    p.rect(x0, -2, x1, bottom + 60, WATER)
    for _ in range(170):
        x = rng.uniform(x0 + 6, x1 - 16)
        y = rng.uniform(0, bottom + 40)
        p.rect(x, y, x + 1, y + rng.uniform(10, 30), WATER_LINE if rng.random() < 0.5 else WATER_DEEP)


def paint_pier(p, x, y0, y1, w=26):
    p.rect(x + 3, y0 + 3, x + w + 3, y1 + 3, WATER_DEEP)
    p.rect(x, y0, x + w, y1, QUAY)
    p.rect(x + 3, y0, x + 5, y1 - 2, darker(QUAY, 0.82))
    p.rect(x + w - 5, y0, x + w - 3, y1 - 2, darker(QUAY, 0.82))


def paint_ship(p, x, y, length, hull, deck, along_x=True):
    w = 26
    if along_x:
        p.poly([(x, y), (x + length - 20, y), (x + length, y + w / 2), (x + length - 20, y + w), (x, y + w)], hull)
        p.poly([(x + 4, y + 4), (x + length - 24, y + 4), (x + length - 10, y + w / 2), (x + length - 24, y + w - 4), (x + 4, y + w - 4)], deck)
        p.rect(x + 8, y + 7, x + 34, y + w - 7, (130, 126, 116))
    else:
        p.poly([(x, y), (x + w, y), (x + w, y + length - 20), (x + w / 2, y + length), (x, y + length - 20)], hull)
        p.poly([(x + 4, y + 4), (x + w - 4, y + 4), (x + w - 4, y + length - 24), (x + w / 2, y + length - 10), (x + 4, y + length - 24)], deck)
        p.rect(x + 7, y + 8, x + w - 7, y + 34, (130, 126, 116))


def paint_waterfront(p, rng):
    quay = YS[8] + 34
    for x in (150, 300, 450, 600, 750):
        paint_pier(p, x, quay, quay + 150)
    for x, y, length, hull, deck in (
        (182, quay + 30, 118, (68, 58, 56), (98, 88, 76)),
        (482, quay + 16, 130, (56, 64, 70), (92, 92, 86)),
    ):
        paint_ship(p, x + 2, y, length, hull, deck, along_x=False)
    # Cranes along the quay
    for x in (120, 280, 430, 720):
        p.rect(x, YS[8] + 16, x + 20, YS[8] + 30, (120, 104, 62))
        p.line([(x + 10, YS[8] + 24), (x + 10, quay + 34)], (136, 118, 70), 4)
    # Boats on the bay
    paint_ship(p, 1500, 1330, 150, (62, 66, 70), (100, 100, 94))
    paint_ship(p, 1020, 1380, 110, (66, 58, 54), (104, 94, 80))
    paint_ship(p, 1750, 1130, 90, (58, 64, 70), (98, 98, 92))
    # The marina: short jetties off Seaview
    for y in (1120, 1160):
        p.rect(XS[8] + 96, y, XS[8] + 170, y + 6, QUAY)


def road_list():
    """The drawn and driven roads: (a, b, is_avenue) between intersections (column, row)."""
    roads = []
    rows, cols = len(CELLS), len(CELLS[0])
    for row in range(rows + 1):
        for col in range(cols):  # horizontal, along the top of cell (col, row)
            above, below = cell_at(col, row - 1), cell_at(col, row)
            if above == "P" and below == "P":
                continue  # the park is one piece
            if is_land(above) or is_land(below) or (below == "R" and row in BRIDGES) or (above == "R" and row in BRIDGES):
                if above == "R" or below == "R":
                    if row not in BRIDGES:
                        continue
                roads.append(((col, row), (col + 1, row), row in (2, 5)))
    for col in range(cols + 1):
        for row in range(rows):  # vertical, along the left of cell (col, row)
            left, right = cell_at(col - 1, row), cell_at(col, row)
            if left == "P" and right == "P":
                continue
            if is_land(left) or is_land(right):
                roads.append(((col, row), (col, row + 1), col in (3, 7)))
    for a, b in zip(BOULEVARD, BOULEVARD[1:]):
        roads.append((a, b, True))
    return roads


def node_pos(n):
    return (XS[n[0]], YS[n[1]])


def paint_roads(p, roads):
    # Off-map stubs, so that the city carries on past the edge.
    stubs = []
    for col in range(len(XS)):
        stubs.append(((XS[col], -4), (XS[col], YS[0])))
    for row in range(len(YS)):
        if row < 8:
            stubs.append(((-4, YS[row]), (XS[0], YS[row])))
        if row < 6:
            stubs.append(((XS[11], YS[row]), (MAP_W + 4, YS[row])))
    for a, b in stubs:
        p.line([a, b], ROAD_EDGE, 18)
    for a, b, avenue in roads:
        p.line([node_pos(a), node_pos(b)], ROAD_EDGE, 30 if avenue else 22)
    for a, b in stubs:
        p.line([a, b], ROAD, 14)
    for a, b, avenue in roads:
        p.line([node_pos(a), node_pos(b)], ROAD, 24 if avenue else 16)
    # Round the corners and clean up the crossings.
    nodes = {n for a, b, _ in roads for n in (a, b)}
    for n in nodes:
        x, y = node_pos(n)
        p.disc(x, y, 8, ROAD)
    # Centre markings on the avenues.
    for a, b, avenue in roads:
        if not avenue:
            continue
        ax, ay = node_pos(a)
        bx, by = node_pos(b)
        length = math.hypot(bx - ax, by - ay)
        ux, uy = (bx - ax) / length, (by - ay) / length
        s = 22.0
        while s < length - 22.0:
            p.line([(ax + ux * s, ay + uy * s), (ax + ux * (s + 12), ay + uy * (s + 12))], ROAD_MARK, 1.5)
            s += 26.0


def paint_landmarks(p):
    """Buildings that stand out, under the sites of the same name."""
    # City Hall: a pale stone court.
    x0, y0, x1, y1 = XS[5] + 26, YS[4] + 26, XS[6] - 26, YS[5] - 26
    p.rect(x0 + 4, y0 + 4, x1 + 4, y1 + 4, darker(STYLES["M"]["ground"], 0.6))
    p.rect(x0, y0, x1, y1, (158, 152, 138))
    p.rect(x0 + 16, y0 + 16, x1 - 16, y1 - 16, (108, 106, 100))
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    p.disc(cx, cy, 13, (134, 124, 92))
    # The bank: columns along its front.
    x0, y0, x1, y1 = XS[4] + 22, YS[2] + 22, XS[5] - 22, YS[3] - 22
    p.rect(x0 + 4, y0 + 4, x1 + 4, y1 + 4, darker(STYLES["M"]["ground"], 0.6))
    p.rect(x0, y0, x1, y1, (150, 146, 134))
    for x in range(int(x0) + 8, int(x1) - 6, 12):
        p.rect(x, y1 - 12, x + 5, y1 - 2, (112, 110, 102))


def paint_warehouse(p):
    """The player's warehouse: long sheds and a truck yard."""
    col, row = WAREHOUSE["cell"]
    x0, y0, x1, y1 = XS[col] + 14, YS[row] + 14, XS[col + 1] - 14, YS[row + 1] - 14
    p.rect(x0, y0, x1, y1, (72, 70, 66))
    # Yard markings
    for x in range(int(x0) + 8, int(x1) - 8, 26):
        p.rect(x, y1 - 26, x + 2, y1 - 4, ROAD_MARK)
    roof = (136, 116, 78)
    bx0, by0, bx1, by1 = x0 + 12, y0 + 12, x1 - 12, y1 - 34
    p.rect(bx0 + 5, by0 + 5, bx1 + 5, by1 + 5, (44, 42, 40))
    p.rect(bx0, by0, bx1, by1, roof)
    for x in range(int(bx0) + 6, int(bx1) - 2, 10):
        p.rect(x, by0 + 1, x + 2, by1 - 1, darker(roof, 0.84))


def compute_graph(roads):
    """Road graph: node positions and edges, plus each site's marker position and its stop on a road."""
    nodes = sorted({n for a, b, _ in roads for n in (a, b)})
    index = {n: i for i, n in enumerate(nodes)}
    positions = [node_pos(n) for n in nodes]
    edges = [(index[a], index[b]) for a, b, _ in roads]

    def stop_for(x, y):
        best = None
        for ia, ib in edges:
            ax, ay = positions[ia]
            bx, by = positions[ib]
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
            px, py = ax + dx * t, ay + dy * t
            dist = math.hypot(px - x, py - y)
            if best is None or dist < best[0]:
                best = (dist, px, py, ia if t < 0.5 else ib)
        return best[1], best[2], best[3]

    def place(cell, u, v):
        col, row = cell
        return XS[col] + (XS[col + 1] - XS[col]) * u, YS[row] + (YS[row + 1] - YS[row]) * v

    sites = []
    for name, kind, cell, u, v in SITES:
        x, y = place(cell, u, v)
        sx, sy, node = stop_for(x, y)
        sites.append((name, kind, x, y, sx, sy, node))

    wx, wy = place(WAREHOUSE["cell"], WAREHOUSE["u"], WAREHOUSE["v"])
    wsx, wsy, wnode = stop_for(wx, wy)
    return positions, edges, sites, (wx, wy, wsx, wsy, wnode)


def check_graph(positions, edges, sites):
    """Every site must be reachable from every other, or a man would be stuck."""
    links = {i: [] for i in range(len(positions))}
    for a, b in edges:
        links[a].append(b)
        links[b].append(a)
    seen = {0}
    todo = [0]
    while todo:
        for n in links[todo.pop()]:
            if n not in seen:
                seen.add(n)
                todo.append(n)
    assert len(seen) == len(positions), "the road graph isn't connected"
    for name, _, _, _, _, _, node in sites:
        assert node in seen, name


def write_header(positions, edges, sites, warehouse):
    kinds = []
    for _, kind, *_ in SITES:
        if kind not in kinds:
            kinds.append(kind)
    out = []
    out.append("// GENERATED by py/create_city_art.py: edit that script, not this file.")
    out.append("// The city map's roads, sites and districts, in pixels of ui/city/map.tex (origin top-left).")
    out.append("#pragma once")
    out.append("")
    out.append(f"constexpr float kCityMapWidth  = {MAP_W}.0f;")
    out.append(f"constexpr float kCityMapHeight = {MAP_H}.0f;")
    out.append("")
    out.append("enum class CityLocation")
    out.append("{")
    for kind in kinds:
        out.append(f"    {kind},")
    out.append("};")
    out.append("")
    out.append("struct CityRoadNode { float x, y; };")
    out.append("struct CityRoadEdge { int a, b; };")
    out.append("")
    out.append("// A place on the map: where its marker is, and the point on a road where a car stops for it.")
    out.append("struct CitySite")
    out.append("{")
    out.append("    const char*  name;")
    out.append("    CityLocation location;")
    out.append("    float        x, y;")
    out.append("    float        stopX, stopY;")
    out.append("    int          node; // the road node next to the stop")
    out.append("};")
    out.append("")
    out.append("struct CityDistrict { const char* name; float x, y; };")
    out.append("")
    out.append("inline constexpr CityRoadNode kCityRoadNodes[] = {")
    for x, y in positions:
        out.append(f"    {{ {x}.0f, {y}.0f }},")
    out.append("};")
    out.append("")
    out.append("inline constexpr CityRoadEdge kCityRoadEdges[] = {")
    for a, b in edges:
        out.append(f"    {{ {a}, {b} }},")
    out.append("};")
    out.append("")
    out.append("inline constexpr CitySite kCitySites[] = {")
    for name, kind, x, y, sx, sy, node in sites:
        out.append(f'    {{ "{name}", CityLocation::{kind}, {x:.1f}f, {y:.1f}f, {sx:.1f}f, {sy:.1f}f, {node} }},')
    out.append("};")
    out.append("")
    wx, wy, wsx, wsy, wnode = warehouse
    out.append(f'inline constexpr CitySite kCityWarehouse = {{ "Your warehouse", CityLocation::{kinds[0]}, {wx:.1f}f, {wy:.1f}f, {wsx:.1f}f, {wsy:.1f}f, {wnode} }};')
    out.append("")
    out.append("inline constexpr CityDistrict kCityDistricts[] = {")
    for name, col, row in DISTRICTS:
        x = XS[int(col)] + (XS[min(int(col) + 1, len(XS) - 1)] - XS[int(col)]) * (col - int(col))
        y = YS[int(row)] + (YS[min(int(row) + 1, len(YS) - 1)] - YS[int(row)]) * (row - int(row))
        out.append(f'    {{ "{name}", {x:.1f}f, {y:.1f}f }},')
    out.append("};")
    out.append("")
    with open(HEADER, "w", newline="\r\n") as f:
        f.write("\n".join(out))
    print(f"Saved {HEADER}")


def make_map():
    rng = random.Random(1931)
    p = Painter()
    paint_water(p, rng)
    paint_land(p)
    for row in range(len(CELLS)):
        for col in range(len(CELLS[0])):
            kind = CELLS[row][col]
            if is_land(kind):
                paint_block(p, rng, kind, XS[col], YS[row], XS[col + 1], YS[row + 1])
    # The city carries on past the edges of the map.
    for col in range(len(CELLS[0])):
        if is_land(CELLS[0][col]):
            paint_block(p, rng, "U" if CELLS[0][col] == "P" else CELLS[0][col], XS[col], -120, XS[col + 1], YS[0])
    for row in range(len(CELLS)):
        paint_block(p, rng, CELLS[row][0], -120, YS[row], XS[0], YS[row + 1])
    for row in range(5):
        paint_block(p, rng, CELLS[row][10], XS[11], YS[row], MAP_W + 120, YS[row + 1])
    paint_park(p, rng)
    paint_landmarks(p)
    paint_warehouse(p)
    paint_river(p, rng)
    paint_waterfront(p, rng)
    roads = road_list()
    paint_roads(p, roads)
    img = p.img.resize((MAP_W, MAP_H), Image.LANCZOS).convert("RGBA")
    return img, roads


# ---- Icons, white on clear, 96 x 96 ----

WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)


def icon_shakedown(d):  # a fist
    d.rounded_rectangle((22, 34, 74, 72), 10, fill=WHITE)
    for x in (22, 36, 50, 62):
        d.rounded_rectangle((x, 22, x + 13, 46), 6, fill=WHITE)
        d.line((x, 46, x + 13, 46), fill=CLEAR, width=2)
    d.rounded_rectangle((12, 42, 32, 60), 8, fill=WHITE)
    d.rectangle((30, 70, 66, 84), fill=WHITE)


def icon_hijack(d):  # a truck
    d.rectangle((8, 28, 58, 66), fill=WHITE)
    d.polygon([(62, 38), (78, 38), (90, 52), (90, 66), (62, 66)], fill=WHITE)
    d.polygon([(67, 43), (76, 43), (83, 52), (67, 52)], fill=CLEAR)
    for x in (26, 72):
        d.ellipse((x - 11, 58, x + 11, 80), fill=CLEAR)
        d.ellipse((x - 8, 61, x + 8, 77), fill=WHITE)


def icon_smuggling(d):  # a boat with a crate
    d.polygon([(6, 54), (90, 54), (74, 80), (20, 80)], fill=WHITE)
    d.rectangle((34, 22, 66, 50), fill=WHITE)
    d.line((34, 22, 66, 50), fill=CLEAR, width=4)
    d.line((66, 22, 34, 50), fill=CLEAR, width=4)
    d.rectangle((16, 34, 28, 50), fill=WHITE)


def icon_burglary(d):  # a house and a key hole
    d.polygon([(48, 10), (90, 46), (6, 46)], fill=WHITE)
    d.rectangle((18, 46, 78, 86), fill=WHITE)
    d.ellipse((38, 46, 58, 66), fill=CLEAR)
    d.polygon([(44, 60), (52, 60), (58, 80), (38, 80)], fill=CLEAR)


def icon_robbery(d):  # a money bag
    d.ellipse((16, 34, 80, 90), fill=WHITE)
    d.polygon([(36, 16), (60, 16), (54, 38), (42, 38)], fill=WHITE)
    d.rectangle((30, 10, 66, 18), fill=WHITE)
    d.rectangle((45, 44, 51, 82), fill=CLEAR)
    d.arc((34, 46, 62, 66), 90, 330, fill=CLEAR, width=5)
    d.arc((34, 60, 62, 80), 270, 150, fill=CLEAR, width=5)


def icon_bribe(d):  # an envelope stuffed with money
    d.rectangle((26, 16, 70, 50), fill=WHITE)
    d.ellipse((40, 22, 56, 38), fill=CLEAR)
    d.rectangle((10, 42, 86, 84), fill=CLEAR)
    d.rectangle((12, 46, 84, 84), fill=WHITE)
    d.line((12, 46, 48, 68), fill=CLEAR, width=4)
    d.line((84, 46, 48, 68), fill=CLEAR, width=4)


def icon_recruit(d):  # a man and a plus
    d.ellipse((22, 12, 54, 44), fill=WHITE)
    d.pieslice((8, 50, 68, 130), 180, 360, fill=WHITE)
    d.rectangle((68, 26, 76, 58), fill=WHITE)
    d.rectangle((56, 38, 88, 46), fill=WHITE)


MARKERS = {
    "shakedown": icon_shakedown,
    "hijack": icon_hijack,
    "smuggling": icon_smuggling,
    "burglary": icon_burglary,
    "robbery": icon_robbery,
    "bribe": icon_bribe,
    "recruit": icon_recruit,
}


def supersampled(size, draw):
    img = Image.new("RGBA", (size * 2, size * 2), CLEAR)
    draw(ImageDraw.Draw(img), size * 2)
    return img.resize((size, size), Image.LANCZOS)


def make_marker_icon(draw):
    img = Image.new("RGBA", (ICON, ICON), CLEAR)
    draw(ImageDraw.Draw(img))
    return img


def make_disc():
    def draw(d, s):
        d.ellipse((0, 0, s - 1, s - 1), fill=(14, 13, 12, 235))
        d.ellipse((s * 0.05, s * 0.05, s * 0.95, s * 0.95), fill=WHITE)
        d.ellipse((s * 0.13, s * 0.13, s * 0.87, s * 0.87), fill=(18, 17, 16, 255))
    return supersampled(ICON, draw)


def make_plate():
    """A pale round plate that keeps a dark car readable on a dark road."""
    def draw(d, s):
        d.ellipse((0, 0, s - 1, s - 1), fill=(14, 13, 12, 235))
        d.ellipse((s * 0.07, s * 0.07, s * 0.93, s * 0.93), fill=(226, 218, 196, 255))
    return supersampled(ICON, draw)


def make_car(heading_degrees):
    """Top-down, nose up before turning: the body is white, so the tint shows; glass stays dark."""
    size = 192
    img = Image.new("RGBA", (size, size), CLEAR)
    d = ImageDraw.Draw(img)
    dark = (24, 24, 26, 255)
    x0, x1 = 58, 134
    d.rounded_rectangle((x0 - 6, 44, x0 + 6, 76), 4, fill=dark)  # wheels
    d.rounded_rectangle((x1 - 6, 44, x1 + 6, 76), 4, fill=dark)
    d.rounded_rectangle((x0 - 6, 116, x0 + 6, 148), 4, fill=dark)
    d.rounded_rectangle((x1 - 6, 116, x1 + 6, 148), 4, fill=dark)
    d.rounded_rectangle((x0, 18, x1, 174), 24, fill=WHITE)
    d.polygon([(x0 + 12, 74), (x1 - 12, 74), (x1 - 6, 96), (x0 + 6, 96)], fill=dark)  # windscreen
    d.polygon([(x0 + 8, 134), (x1 - 8, 134), (x1 - 14, 150), (x0 + 14, 150)], fill=dark)  # rear window
    # Pillow turns counter-clockwise; headings are clockwise from up.
    img = img.rotate(-heading_degrees, resample=Image.BICUBIC)
    return img.resize((ICON, ICON), Image.LANCZOS)


def make_portrait_suit():
    def draw(d, s):
        k = s / 96.0
        d.pieslice((12 * k, 54 * k, 84 * k, 150 * k), 180, 360, fill=WHITE)
    return supersampled(ICON, draw)  # white, to take the man's tint; shirt and tie are on the head layer


def make_portrait_head():
    """Fixed colors: face, hat, shirt collar. Drawn over the tinted suit."""
    skin = (206, 170, 138, 255)
    hat = (30, 28, 28, 255)
    band = (112, 104, 96, 255)
    shirt = (226, 222, 210, 255)
    tie = (34, 32, 34, 255)

    def draw(d, s):
        k = s / 96.0
        # Collar and tie
        d.polygon([(38 * k, 56 * k), (58 * k, 56 * k), (53 * k, 84 * k), (43 * k, 84 * k)], fill=shirt)
        d.polygon([(45 * k, 62 * k), (51 * k, 62 * k), (53 * k, 84 * k), (43 * k, 84 * k)], fill=tie)
        # Neck and face
        d.rectangle((41 * k, 46 * k, 55 * k, 62 * k), fill=(186, 150, 120, 255))
        d.ellipse((31 * k, 22 * k, 65 * k, 58 * k), fill=skin)
        # Fedora
        d.rounded_rectangle((20 * k, 26 * k, 76 * k, 32 * k), 3 * k, fill=hat)
        d.rounded_rectangle((32 * k, 8 * k, 64 * k, 28 * k), 6 * k, fill=hat)
        d.rectangle((32 * k, 21 * k, 64 * k, 26 * k), fill=band)
    return supersampled(ICON, draw)


def save(img, rel):
    path = os.path.join(OUT_DIR, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"Saved {path}")


def main():
    img, roads = make_map()
    save(img, "map.png")

    positions, edges, sites, warehouse = compute_graph(roads)
    check_graph(positions, edges, sites)
    write_header(positions, edges, sites, warehouse)

    for name, draw in MARKERS.items():
        save(make_marker_icon(draw), os.path.join("markers", name + ".png"))
    save(make_disc(), "disc.png")
    save(make_plate(), "plate.png")
    for i in range(8):
        save(make_car(i * 45), f"car_{i}.png")
    save(make_portrait_suit(), "portrait_suit.png")
    save(make_portrait_head(), "portrait_head.png")


main()
