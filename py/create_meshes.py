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


# name -> (path under assets/meshes, builder)
MESHES = {
    "floor_tile": ("world/floor_tile", floor_tile),
    "earth_block": ("world/earth_block", earth_block),
    "wall_segment": ("world/wall_segment", wall_segment),
    "door_frame": ("world/door_frame", door_frame),
    "stairs": ("world/stairs", stairs),
    "crate": ("props/crate", crate),
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
