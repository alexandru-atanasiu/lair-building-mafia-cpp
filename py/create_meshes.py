"""Creates placeholder low-poly mesh sources under assets/meshes/ with Blender.

Run once (or whenever the placeholders should be regenerated), from the repository root:

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --factory-startup --python py/create_meshes.py

Then run cook.bat: every assets/<path>.blend becomes __generated/data/<path>.mesh. To add a mesh,
write a function of primitives below and add it to MESHES (or pass names to build only those:
... --python py/create_meshes.py -- crate).

Units: one Blender unit is one world unit, about a metre. A tile is 2 x 2 (kTileSize in
src/game/world/world_grid.h); walls are 2.4 high (kWallHeight). The floor is z = 0. Blender's
(x, y, z) is the game's (x, z, y), so the game's +Z is Blender's +Y. Meshes take one color per
instance, given when they are drawn: a thing of several colors is several meshes.
"""

import os
import sys

import bpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "meshes")

TILE = 2.0
WALL_HEIGHT = 2.4
WALL_THICKNESS = 0.25
LEVEL_DROP = 4.0  # floor to floor; keep in sync with kUndergroundFloorY in world_grid.h


def box(cx, cy, z0, sx, sy, sz):
    """Box of size (sx, sy, sz) centered on (cx, cy) in X/Y, bottom at z0."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(cx, cy, z0 + sz * 0.5))
    bpy.context.active_object.scale = (sx, sy, sz)
    bpy.ops.object.shade_flat()


def floor_tile():
    """One tile, top surface at z = 0. Slightly inset so the grid reads."""
    box(0.0, 0.0, -0.1, TILE - 0.04, TILE - 0.04, 0.1)


def earth_block():
    """Undug earth filling a tile, up to the top of the walls."""
    box(0.0, 0.0, 0.0, TILE, TILE, WALL_HEIGHT)


def wall_segment():
    """A wall on one tile edge: along X, centered on the origin."""
    box(0.0, 0.0, 0.0, TILE + WALL_THICKNESS, WALL_THICKNESS, WALL_HEIGHT)


def door_frame():
    """A doorway on one tile edge (along X, like wall_segment): posts, lintel and an open door."""
    gap = 1.1
    post = (TILE + WALL_THICKNESS - gap) * 0.5
    offset = (gap + post) * 0.5
    box(-offset, 0.0, 0.0, post, WALL_THICKNESS, WALL_HEIGHT)
    box(offset, 0.0, 0.0, post, WALL_THICKNESS, WALL_HEIGHT)
    box(0.0, 0.0, 2.05, gap, WALL_THICKNESS, WALL_HEIGHT - 2.05)
    box(-gap * 0.5 - 0.02, -gap * 0.5, 0.0, 0.05, gap, 2.05)  # the door, swung open


def stairs():
    """Stairs from the origin going down along +Y (the game's +Z): flat for half a tile from the
    upper tile's center, down LEVEL_DROP over two tiles, ending on the lower floor."""
    steps = 16
    run = 2.0 * TILE / steps
    rise = LEVEL_DROP / steps
    for i in range(steps):
        top = -rise * (i + 1)
        box(0.0, TILE * 0.5 + run * (i + 0.5), -LEVEL_DROP, TILE - 0.2, run, top + LEVEL_DROP)
    for side in (-1.0, 1.0):
        box(side * (TILE * 0.5 - 0.05), TILE * 1.5, -LEVEL_DROP, 0.1, 2.0 * TILE, LEVEL_DROP + 0.1)


def crate():
    """A wooden crate, about a metre across, with slats."""
    box(0.0, 0.0, 0.0, 0.9, 0.9, 0.8)
    for x in (-0.47, 0.47):
        for y in (-0.47, 0.47):
            box(x, y, 0.0, 0.08, 0.08, 0.84)
    for z in (0.0, 0.76):
        box(0.0, -0.47, z, 0.94, 0.08, 0.08)
        box(0.0, 0.47, z, 0.94, 0.08, 0.08)
        box(-0.47, 0.0, z, 0.08, 0.94, 0.08)
        box(0.47, 0.0, z, 0.08, 0.94, 0.08)


def cyl(cx, cy, z0, radius, height, sides=8):
    """Upright cylinder of `sides` flat faces, bottom at z0."""
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=height, location=(cx, cy, z0 + height * 0.5))
    bpy.ops.object.shade_flat()


# The lair. Objects are centered on their footprint, width along X and depth along Y, and face +Y
# (the game's +Z). One function per color; the colors are in src/game/sim/lair_tables.cpp.

def earth_rock(seed):
    """Undug rock filling a tile up to the top of the walls, its top broken into uneven slabs."""
    import random
    rng = random.Random(seed)
    box(0.0, 0.0, 0.0, TILE, TILE, WALL_HEIGHT - 0.14)
    for ix in (-0.5, 0.5):
        for iy in (-0.5, 0.5):
            box(ix + rng.uniform(-0.06, 0.06), iy + rng.uniform(-0.06, 0.06), WALL_HEIGHT - 0.2,
                rng.uniform(0.82, 0.96), rng.uniform(0.82, 0.96), rng.uniform(0.12, 0.2))


def earth_rock_a():
    earth_rock(1)


def earth_rock_b():
    earth_rock(2)


def earth_rock_c():
    earth_rock(3)


def stairs_cut():
    """The stairs as the lair sees them, cut off where the cutaway plane is so they hide nothing:
    the same steps as stairs(), no rails, and a flat top where they pass the plane."""
    steps = 16
    cut = 1.3  # metres above the lower floor
    run = 2.0 * TILE / steps
    rise = LEVEL_DROP / steps
    for i in range(steps):
        top = LEVEL_DROP - rise * (i + 1)  # above the lower floor
        box(0.0, TILE * 0.5 + run * (i + 0.5), -LEVEL_DROP, TILE - 0.2, run, min(top, cut))


def rock_chip():
    """A bit of rock thrown up by digging."""
    box(0.0, 0.0, 0.0, 0.2, 0.16, 0.14)


def lair_door_frame():
    """A doorway on one tile edge (along X, like wall_segment): posts and lintel, no door."""
    gap = 1.2
    post = (TILE + WALL_THICKNESS - gap) * 0.5
    offset = (gap + post) * 0.5
    box(-offset, 0.0, 0.0, post, WALL_THICKNESS, WALL_HEIGHT)
    box(offset, 0.0, 0.0, post, WALL_THICKNESS, WALL_HEIGHT)
    box(0.0, 0.0, 2.05, gap, WALL_THICKNESS, WALL_HEIGHT - 2.05)


def door_leaf():
    """The door itself: hinged at the origin, closed along +X, 1.2 wide. Panels and a handle."""
    box(0.6, 0.0, 0.0, 1.18, 0.08, 2.04)
    for z in (0.2, 1.15):
        box(0.6, 0.0, z, 0.86, 0.11, 0.7)
    box(1.04, 0.0, 0.95, 0.08, 0.17, 0.08)


def bunk_frame():
    for x in (-0.9, 0.9):
        for y in (-0.42, 0.42):
            box(x, y, 0.0, 0.07, 0.07, 1.7)
    for z in (0.32, 1.12):
        box(0.0, 0.42, z, 1.87, 0.05, 0.08)
        box(0.0, -0.42, z, 1.87, 0.05, 0.08)
        box(0.0, 0.0, z, 1.8, 0.8, 0.04)
    box(0.0, 0.42, 1.62, 1.87, 0.05, 0.05)
    box(0.0, -0.42, 1.62, 1.87, 0.05, 0.05)


def bunk_mattress():
    for z in (0.36, 1.16):
        box(0.05, 0.0, z, 1.68, 0.78, 0.16)


def bunk_pillow():
    for z in (0.52, 1.32):
        box(-0.68, 0.0, z, 0.36, 0.56, 0.1)


def footlocker_box():
    box(0.0, 0.0, 0.0, 1.0, 0.56, 0.44)
    box(0.0, 0.0, 0.46, 1.02, 0.58, 0.08)


def footlocker_trim():
    for x in (-0.36, 0.36):
        box(x, 0.0, 0.0, 0.08, 0.6, 0.56)
    box(0.0, 0.3, 0.32, 0.14, 0.04, 0.16)


def counter_body():
    box(0.0, 0.0, 0.0, 3.7, 0.7, 1.0)
    box(0.0, 0.32, 0.0, 3.7, 0.12, 0.14)  # the foot rail side


def counter_top():
    box(0.0, 0.03, 1.0, 3.85, 0.84, 0.08)


def counter_bottles():
    for i, x in enumerate((-1.5, -1.25, -1.05, 0.6, 0.85, 1.1, 1.45)):
        h = 0.3 if i % 2 else 0.24
        cyl(x, -0.16, 1.08, 0.05, h, 6)
        cyl(x, -0.16, 1.08 + h, 0.02, 0.1, 5)


def stool_legs():
    cyl(0.0, -0.45, 0.0, 0.2, 0.05, 8)
    cyl(0.0, -0.45, 0.05, 0.035, 0.68, 6)


def stool_seat():
    cyl(0.0, -0.45, 0.72, 0.21, 0.09, 8)


def table_legs():
    cyl(0.0, 0.0, 0.0, 0.06, 0.72, 6)
    cyl(0.0, 0.0, 0.0, 0.42, 0.05, 8)


def table_top():
    cyl(0.0, 0.0, 0.72, 0.8, 0.07, 8)


def shelf_frame():
    for x in (-0.86, 0.86):
        for y in (-0.3, 0.3):
            box(x, y, 0.0, 0.07, 0.07, 2.05)
    for z in (0.12, 0.8, 1.45, 2.0):
        box(0.0, 0.0, z, 1.75, 0.62, 0.05)


def shelf_boxes():
    for z, boxes in ((0.17, (-0.55, 0.05, 0.58)), (0.85, (-0.5, 0.42)), (1.5, (-0.6, -0.05, 0.5))):
        for x in boxes:
            box(x, 0.0, z, 0.46, 0.48, 0.42)


def gun_rack_frame():
    box(0.0, -0.26, 0.0, 1.8, 0.1, 1.85)
    box(0.0, 0.0, 0.0, 1.8, 0.56, 0.1)
    box(0.0, -0.08, 0.4, 1.76, 0.3, 0.06)
    box(0.0, -0.1, 1.5, 1.76, 0.28, 0.06)


def gun_rack_guns():
    for x in (-0.66, -0.22, 0.22, 0.66):
        box(x, -0.1, 0.22, 0.08, 0.1, 1.45)  # the barrel and stock
        box(x, -0.04, 0.42, 0.11, 0.12, 0.42)  # the receiver
        cyl(x, 0.04, 0.5, 0.075, 0.14, 8)  # the drum


def workbench_body():
    for x in (-1.7, 1.7):
        for y in (-0.36, 0.36):
            box(x, y, 0.0, 0.14, 0.14, 0.86)
    box(0.0, 0.0, 0.26, 3.5, 0.74, 0.05)
    box(0.0, 0.0, 0.86, 3.8, 0.9, 0.09)


def workbench_tools():
    box(-1.2, 0.1, 0.95, 0.3, 0.3, 0.16)  # the vice
    box(-1.2, 0.3, 1.0, 0.08, 0.3, 0.06)
    for x in (0.4, 0.8):
        box(x, -0.1, 0.95, 0.09, 0.5, 0.05)  # tools laid out
    box(1.3, 0.05, 0.95, 0.4, 0.3, 0.2)  # a toolbox


def boss_desk_body():
    for x in (-1.2, 1.2):
        box(x, 0.0, 0.0, 0.8, 0.92, 0.72)  # drawers
    box(0.0, -0.36, 0.0, 1.6, 0.12, 0.72)  # the front panel
    box(0.0, 0.0, 0.72, 3.3, 1.02, 0.09)


def boss_desk_top():
    box(0.0, 0.05, 0.81, 2.2, 0.72, 0.015)  # the blotter


def boss_desk_lamp():
    cyl(-1.25, -0.2, 0.81, 0.13, 0.03, 8)
    cyl(-1.25, -0.2, 0.81, 0.025, 0.3, 5)
    cyl(-1.25, -0.2, 1.05, 0.14, 0.13, 8)
    box(1.15, 0.05, 0.81, 0.3, 0.2, 0.06)  # the telephone
    box(1.15, 0.05, 0.88, 0.32, 0.08, 0.05)


def boss_chair_frame():
    cyl(0.0, -0.45, 0.0, 0.3, 0.05, 8)
    cyl(0.0, -0.45, 0.05, 0.045, 0.42, 6)


def boss_chair_cushion():
    box(0.0, -0.45, 0.47, 0.72, 0.68, 0.14)  # seat
    box(0.0, -0.76, 0.47, 0.72, 0.14, 0.85)  # high back
    for x in (-0.38, 0.38):
        box(x, -0.45, 0.61, 0.1, 0.56, 0.2)  # arms


def safe_body():
    box(0.0, 0.0, 0.08, 1.18, 1.04, 1.18)
    for x in (-0.48, 0.48):
        box(x, 0.0, 0.0, 0.14, 0.9, 0.1)
    box(0.0, 0.5, 0.18, 1.0, 0.08, 0.98)  # the door


def safe_dial():
    bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=0.14, depth=0.05, location=(-0.12, 0.56, 0.72), rotation=(1.5708, 0.0, 0.0))
    bpy.ops.object.shade_flat()
    box(0.3, 0.57, 0.65, 0.07, 0.07, 0.3)  # the handle
    for z in (0.28, 1.02):
        box(-0.5, 0.55, z, 0.1, 0.04, 0.12)  # hinges


# name -> (path under assets/meshes, builder)
MESHES = {
    "floor_tile": ("world/floor_tile", floor_tile),
    "earth_block": ("world/earth_block", earth_block),
    "wall_segment": ("world/wall_segment", wall_segment),
    "door_frame": ("world/door_frame", door_frame),
    "stairs": ("world/stairs", stairs),
    "crate": ("props/crate", crate),
    "earth_rock_a": ("world/earth_rock_a", earth_rock_a),
    "earth_rock_b": ("world/earth_rock_b", earth_rock_b),
    "earth_rock_c": ("world/earth_rock_c", earth_rock_c),
    "rock_chip": ("world/rock_chip", rock_chip),
    "stairs_cut": ("world/stairs_cut", stairs_cut),
    "lair_door_frame": ("world/lair_door_frame", lair_door_frame),
    "door_leaf": ("world/door_leaf", door_leaf),
    "bunk_frame": ("objects/bunk_frame", bunk_frame),
    "bunk_mattress": ("objects/bunk_mattress", bunk_mattress),
    "bunk_pillow": ("objects/bunk_pillow", bunk_pillow),
    "footlocker_box": ("objects/footlocker_box", footlocker_box),
    "footlocker_trim": ("objects/footlocker_trim", footlocker_trim),
    "counter_body": ("objects/counter_body", counter_body),
    "counter_top": ("objects/counter_top", counter_top),
    "counter_bottles": ("objects/counter_bottles", counter_bottles),
    "stool_legs": ("objects/stool_legs", stool_legs),
    "stool_seat": ("objects/stool_seat", stool_seat),
    "table_legs": ("objects/table_legs", table_legs),
    "table_top": ("objects/table_top", table_top),
    "shelf_frame": ("objects/shelf_frame", shelf_frame),
    "shelf_boxes": ("objects/shelf_boxes", shelf_boxes),
    "gun_rack_frame": ("objects/gun_rack_frame", gun_rack_frame),
    "gun_rack_guns": ("objects/gun_rack_guns", gun_rack_guns),
    "workbench_body": ("objects/workbench_body", workbench_body),
    "workbench_tools": ("objects/workbench_tools", workbench_tools),
    "boss_desk_body": ("objects/boss_desk_body", boss_desk_body),
    "boss_desk_top": ("objects/boss_desk_top", boss_desk_top),
    "boss_desk_lamp": ("objects/boss_desk_lamp", boss_desk_lamp),
    "boss_chair_frame": ("objects/boss_chair_frame", boss_chair_frame),
    "boss_chair_cushion": ("objects/boss_chair_cushion", boss_chair_cushion),
    "safe_body": ("objects/safe_body", safe_body),
    "safe_dial": ("objects/safe_dial", safe_dial),
}


def main():
    only = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else list(MESHES)
    for name in only:
        rel, build = MESHES[name]
        bpy.ops.wm.read_factory_settings(use_empty=True)
        build()
        path = os.path.join(OUT_DIR, rel + ".blend")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=path, compress=True)
        print(f"Saved {path}")


main()
