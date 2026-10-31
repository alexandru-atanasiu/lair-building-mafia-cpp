"""Creates the warehouse level's low-poly meshes under assets/meshes/warehouse/ with Blender.

From the repository root (pass names after -- to build only those):

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --factory-startup --python py/create_warehouse_meshes.py [-- truck_cab ...]

Then run cook.bat. Same conventions as py/create_meshes.py: one Blender unit is a metre, Blender's
(x, y, z) is the game's (x, z, y), and a mesh has one color, given when it's drawn, so a thing of
several colors is several meshes. The colors are in src/game/scenes/warehouse_scene.cpp.

The truck's nose points along +Y (the game's +Z) and its origin is on the ground under its middle.
"""

import os
import sys

import bpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "meshes", "warehouse")

TILE = 2.0
WALL_HEIGHT = 2.4
WALL_THICKNESS = 0.25
TALL_WALL = 4.0  # the warehouse's walls, where they don't face the camera

TRUCK_WIDTH = 2.4
BED_FRONT = 1.45  # along the truck; keep in sync with kBedFrontZ in warehouse_scene.cpp
BED_REAR = -4.25
BED_TOP = 1.05  # the bed's floor; keep in sync with kBedFloorY in warehouse_scene.cpp


def box(cx, cy, z0, sx, sy, sz):
    """Box of size (sx, sy, sz) centered on (cx, cy) in X/Y, bottom at z0."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(cx, cy, z0 + sz * 0.5))
    bpy.context.active_object.scale = (sx, sy, sz)
    bpy.ops.object.shade_flat()


def hexa(bottom, top):
    """A solid from four bottom and four top corners, each counter-clockwise seen from above."""
    verts = list(bottom) + list(top)
    faces = [(3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    mesh = bpy.data.meshes.new("hexa")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new("hexa", mesh)
    bpy.context.collection.objects.link(obj)


def wedge_y(cx, sx, y0, y1, z0, z1, top_y0, top_y1):
    """A box across X whose top face runs from top_y0 to top_y1 along Y: slanted front and back."""
    x0, x1 = cx - sx * 0.5, cx + sx * 0.5
    hexa([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)],
         [(x0, top_y0, z1), (x1, top_y0, z1), (x1, top_y1, z1), (x0, top_y1, z1)])


def wheel(cx, cy, radius, width):
    """A wheel on the ground, its axle along X."""
    bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=radius, depth=width, location=(cx, cy, radius),
                                        rotation=(0.0, 1.5707963, 0.0))
    bpy.ops.object.shade_flat()


# ---------------------------------------------------------------------------- the truck


def truck_chassis():
    box(0.0, 0.0, 0.5, 1.5, 8.3, 0.4)  # frame
    box(0.0, 4.17, 0.42, TRUCK_WIDTH, 0.18, 0.36)  # front bumper
    box(0.0, BED_REAR + 0.06, 0.55, TRUCK_WIDTH, 0.12, 0.22)  # rear bumper
    box(0.0, 2.75, 0.9, 2.5, 0.9, 0.08)  # front fender tops
    box(0.0, -2.4, 0.9, 2.5, 1.9, 0.06)  # rear fender tops
    for side in (-1.0, 1.0):
        box(side * 1.28, 0.75, 0.75, 0.16, 0.5, 0.35)  # fuel tank / step
        box(side * 1.32, 3.1, 1.95, 0.12, 0.08, 0.3)  # mirror


def truck_cab():
    box(0.0, 3.0, 0.8, TRUCK_WIDTH, 2.3, 0.95)  # lower body
    wedge_y(0.0, TRUCK_WIDTH - 0.06, 1.9, 3.95, 1.75, 2.8, 1.9, 3.1)  # cabin, windscreen slanted
    box(0.0, 2.5, 2.8, TRUCK_WIDTH - 0.3, 1.1, 0.08)  # roof cap


def truck_glass():
    wedge_y(0.0, TRUCK_WIDTH - 0.3, 3.92, 3.99, 1.85, 2.72, 3.14, 3.2)  # windscreen
    for side in (-1.0, 1.0):
        box(side * (TRUCK_WIDTH * 0.5 - 0.01), 2.65, 1.9, 0.04, 1.0, 0.72)  # side window
    box(0.0, 1.89, 1.95, TRUCK_WIDTH - 0.5, 0.04, 0.6)  # back window


def truck_bed():
    length = BED_FRONT - BED_REAR
    mid = (BED_FRONT + BED_REAR) * 0.5
    box(0.0, mid, 0.9, TRUCK_WIDTH, length, BED_TOP - 0.9)  # floor
    box(0.0, BED_FRONT, BED_TOP, TRUCK_WIDTH, 0.08, 1.7)  # headboard
    for side in (-1.0, 1.0):
        x = side * (TRUCK_WIDTH * 0.5 - 0.03)
        box(x, mid, BED_TOP + 0.05, 0.05, length, 0.26)  # boards
        box(x, mid, BED_TOP + 0.42, 0.05, length, 0.26)
        for i in range(5):
            box(side * (TRUCK_WIDTH * 0.5 - 0.02), BED_REAR + 0.1 + i * (length - 0.2) / 4.0, BED_TOP, 0.1, 0.1, 0.85)


def truck_tarp():
    """Canvas over the bed; the game squeezes it toward the headboard to open up."""
    length = BED_FRONT - BED_REAR
    mid = (BED_FRONT + BED_REAR) * 0.5
    box(0.0, mid, BED_TOP + 0.7, TRUCK_WIDTH + 0.06, length, 0.95)
    hexa([(-1.23, BED_REAR, BED_TOP + 1.65), (1.23, BED_REAR, BED_TOP + 1.65), (1.23, BED_FRONT, BED_TOP + 1.65), (-1.23, BED_FRONT, BED_TOP + 1.65)],
         [(-0.8, BED_REAR, BED_TOP + 1.9), (0.8, BED_REAR, BED_TOP + 1.9), (0.8, BED_FRONT, BED_TOP + 1.9), (-0.8, BED_FRONT, BED_TOP + 1.9)])
    for i in range(1, 5):  # hoops showing through
        box(0.0, BED_REAR + i * length / 5.0, BED_TOP + 0.7, TRUCK_WIDTH + 0.1, 0.06, 0.97)


def truck_gate():
    """One rear door: hinged at the origin, reaching along +X to the truck's middle."""
    w = TRUCK_WIDTH * 0.5
    box(w * 0.5, 0.0, 0.0, w, 0.05, 0.8)
    for z in (0.05, 0.4, 0.72):
        box(w * 0.5, -0.03, z, w, 0.04, 0.08)
    box(0.05, -0.03, 0.0, 0.08, 0.04, 0.8)
    box(w - 0.05, -0.03, 0.0, 0.08, 0.04, 0.8)


def truck_wheels():
    for x in (-1.05, 1.05):
        wheel(x, 2.75, 0.5, 0.34)
        wheel(x, -1.9, 0.5, 0.42)
        wheel(x, -2.95, 0.5, 0.42)


def truck_lights():
    for side in (-0.85, 0.85):
        box(side, 4.26, 0.95, 0.42, 0.04, 0.22)  # headlights
        box(side, BED_REAR - 0.01, 0.58, 0.32, 0.04, 0.14)  # tail lights


# ---------------------------------------------------------------------------- inside


def rack_frame():
    """A pallet rack two tiles long: along X, one tile deep, centered on the origin."""
    length = 2.0 * TILE - 0.1
    depth = 1.2
    for x in (-length * 0.5, -length / 6.0, length / 6.0, length * 0.5):
        for y in (-depth * 0.5, depth * 0.5):
            box(x, y, 0.0, 0.09, 0.09, 2.2)
        box(x, 0.0, 0.3, 0.05, depth, 0.05)
        box(x, 0.0, 1.5, 0.05, depth, 0.05)


def rack_beams():
    length = 2.0 * TILE - 0.1
    depth = 1.2
    for z in (1.0, 2.1):
        for y in (-depth * 0.5, depth * 0.5):
            box(0.0, y, z, length, 0.07, 0.1)
    for i in range(-5, 6):  # slats, so what's below shows from above
        box(i * length / 11.0, 0.0, 1.07, 0.08, depth, 0.03)


def rack_cartons():
    """The front's honest stock, filling a rack: not crates, and never counted as crates."""
    for x in (-1.3, 0.0, 1.3):
        box(x, 0.0, 0.02, 1.1, 0.95, 0.72)
        box(x + 0.05, 0.03, 1.1, 1.0, 0.9, 0.62)


def pallet():
    """A wooden pallet, a metre and a bit across, 0.14 high."""
    for x in (-0.55, 0.0, 0.55):
        box(x, 0.0, 0.0, 0.14, 1.3, 0.09)
    for y in (-0.5, -0.25, 0.0, 0.25, 0.5):
        box(0.0, y, 0.09, 1.3, 0.17, 0.05)


def goods_stack():
    """The front's honest goods: shrink-wrapped cartons on a pallet, stepping up. Not crates."""
    box(0.0, 0.0, 0.14, 1.24, 1.24, 0.6)
    box(-0.02, 0.01, 0.74, 1.2, 1.2, 0.5)
    box(0.15, -0.1, 1.24, 0.82, 0.92, 0.42)


def forklift_body():
    box(0.0, -0.2, 0.25, 1.05, 1.6, 0.6)  # body
    box(0.0, -0.85, 0.85, 1.0, 0.45, 0.35)  # counterweight
    box(0.0, -0.3, 0.85, 0.45, 0.4, 0.42)  # seat


def forklift_dark():
    for x in (-0.46, 0.46):
        wheel(x, 0.4, 0.28, 0.2)
        wheel(x, -0.75, 0.22, 0.18)
        box(x, -0.55, 0.85, 0.06, 0.06, 1.25)  # cage, back posts
        box(x, 0.3, 0.85, 0.06, 0.06, 1.25)  # cage, front posts
    box(0.0, -0.13, 2.08, 1.0, 0.9, 0.05)  # cage roof
    for x in (-0.28, 0.28):
        box(x, 0.68, 0.05, 0.1, 0.1, 2.4)  # mast
        box(x, 1.2, 0.07, 0.14, 1.05, 0.05)  # forks
    box(0.0, 0.72, 0.1, 0.8, 0.05, 0.5)  # carriage


def office_desk():
    box(0.0, 0.0, 0.7, 1.7, 0.8, 0.06)
    box(-0.75, 0.0, 0.0, 0.06, 0.75, 0.7)
    box(0.75, 0.0, 0.0, 0.06, 0.75, 0.7)
    box(0.5, 0.05, 0.0, 0.5, 0.65, 0.7)  # drawers


def office_things():
    """What's on and around the desk, in a dark color: a typewriter, a chair, a phone."""
    box(-0.2, 0.05, 0.76, 0.5, 0.35, 0.14)
    box(0.55, -0.1, 0.76, 0.2, 0.25, 0.08)
    box(0.0, -0.85, 0.0, 0.5, 0.5, 0.45)
    box(0.0, -1.08, 0.45, 0.5, 0.06, 0.5)


def office_cabinet():
    """A row of two filing cabinets, backing onto a wall along X."""
    for x in (-0.3, 0.3):
        box(x, 0.0, 0.0, 0.56, 0.6, 1.35)


# ---------------------------------------------------------------------------- the building


def wall_upper():
    """Raises a wall_segment to the warehouse's height. Along X, like the wall it sits on."""
    box(0.0, 0.0, WALL_HEIGHT, TILE + WALL_THICKNESS, WALL_THICKNESS, TALL_WALL - WALL_HEIGHT)


def wall_trim():
    """A dark band under the eaves, drawn with wall_upper."""
    box(0.0, 0.0, TALL_WALL, TILE + WALL_THICKNESS + 0.02, WALL_THICKNESS + 0.1, 0.12)


def dock_frame():
    """The big door's frame: along X, two tiles wide, centered on the origin, 3.2 clear high."""
    w = 2.0 * TILE
    for side in (-1.0, 1.0):
        box(side * (w * 0.5 + 0.1), 0.0, 0.0, 0.3, 0.45, 3.4)
    box(0.0, 0.0, 3.2, w + 0.5, 0.45, 0.8)  # header, with the rolled-up shutter in it


def dock_shutter():
    """The roll-up door, closed. The game squeezes it up toward y = 3.2 to open it."""
    w = 2.0 * TILE
    box(0.0, 0.0, 0.0, w, 0.3, 3.2)
    for i in range(1, 8):
        box(0.0, 0.0, i * 0.4, w, 0.34, 0.05)


def dock_paint():
    """Yellow on the yard: the truck's lane, from the door outward along -Y."""
    for x in (-1.6, 1.6):
        box(x, -5.0, 0.0, 0.14, 9.5, 0.015)
    box(0.0, -2.2, 0.0, 3.34, 0.14, 0.015)  # the stop line


def dock_bumpers():
    for side in (-1.0, 1.0):
        box(side * 1.6, -0.3, 0.0, 0.3, 0.35, 1.1)


def stair_rail():
    """A guard around the stairwell: along +Y from the origin (the game's +Z), two tiles long."""
    for x in (-1.05, 1.05):
        box(x, 2.0, 0.95, 0.06, 4.0, 0.06)
        box(x, 2.0, 0.5, 0.06, 4.0, 0.05)
        for y in (0.0, 2.0, 4.0):
            box(x, y, 0.0, 0.07, 0.07, 1.0)
    box(0.0, 4.0, 0.95, 2.1, 0.06, 0.06)
    box(0.0, 4.0, 0.5, 2.1, 0.05, 0.05)


def stair_shaft_end():
    """Closes the stairwell's far end below the floor, where the steps stop: same origin as stair_rail."""
    box(0.0, 4.03, -4.0, TILE, 0.06, 4.0)


# ---------------------------------------------------------------------------- outside


def road():
    """Asphalt along Y, centered on the origin, 8 wide: past the map's edge both ways."""
    box(0.0, 0.0, 0.0, 8.0, 600.0, 0.02)


def road_marks():
    for i in range(-30, 31):
        box(0.0, i * 10.0, 0.0, 0.16, 4.0, 0.03)
    for x in (-3.8, 3.8):
        box(x, 0.0, 0.0, 0.1, 600.0, 0.03)


def fence_panel():
    """One tile of chain-link: along X. A dark, thin mesh reads as wire from above."""
    box(0.0, 0.0, 0.0, 0.05, 0.05, 2.1)
    box(0.0, 0.0, 2.0, TILE, 0.04, 0.04)
    box(0.0, 0.0, 1.0, TILE, 0.02, 0.02)
    box(0.0, 0.0, 0.1, TILE, 0.04, 0.04)
    for i in range(1, 8):
        box(-TILE * 0.5 + i * TILE / 8.0, 0.0, 0.1, 0.012, 0.012, 1.9)


def lamp_post():
    box(0.0, 0.0, 0.0, 0.14, 0.14, 5.0)
    box(0.0, 0.5, 4.95, 0.1, 1.1, 0.08)
    box(0.0, 1.0, 4.82, 0.3, 0.5, 0.13)


def container():
    """A shipping container, 6 long along X."""
    box(0.0, 0.0, 0.0, 6.0, 2.4, 2.5)
    for i in range(-7, 8):
        box(i * 0.38, 0.0, 0.1, 0.1, 2.46, 2.3)


def neighbour_block():
    """A plain building across the road, 16 x 12 x 7, with a parapet and blank windows."""
    box(0.0, 0.0, 0.0, 16.0, 12.0, 7.0)
    box(0.0, 0.0, 7.0, 16.3, 12.3, 0.3)


def neighbour_windows():
    for z in (1.6, 4.3):
        for i in range(-3, 4):
            box(i * 2.2, -6.02, z, 1.2, 0.04, 1.5)
            box(i * 2.2, 6.02, z, 1.2, 0.04, 1.5)
        for i in range(-2, 3):
            box(-8.02, i * 2.2, z, 0.04, 1.2, 1.5)
            box(8.02, i * 2.2, z, 0.04, 1.2, 1.5)


def weeds_patch():
    """A rough lot beside the concrete: top barely above the tiles, 2 x 2, scaled when drawn."""
    box(0.0, 0.0, 0.0, 2.0, 2.0, 0.015)


def oil_drums():
    for x, y in ((-0.4, 0.0), (0.4, 0.1), (0.0, 0.65)):
        bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=0.3, depth=0.9, location=(x, y, 0.45))
        bpy.ops.object.shade_flat()


MESHES = {
    "truck_chassis": truck_chassis,
    "truck_cab": truck_cab,
    "truck_glass": truck_glass,
    "truck_bed": truck_bed,
    "truck_tarp": truck_tarp,
    "truck_gate": truck_gate,
    "truck_wheels": truck_wheels,
    "truck_lights": truck_lights,
    "rack_frame": rack_frame,
    "rack_beams": rack_beams,
    "rack_cartons": rack_cartons,
    "pallet": pallet,
    "goods_stack": goods_stack,
    "forklift_body": forklift_body,
    "forklift_dark": forklift_dark,
    "office_desk": office_desk,
    "office_things": office_things,
    "office_cabinet": office_cabinet,
    "wall_upper": wall_upper,
    "wall_trim": wall_trim,
    "dock_frame": dock_frame,
    "dock_shutter": dock_shutter,
    "dock_paint": dock_paint,
    "dock_bumpers": dock_bumpers,
    "stair_rail": stair_rail,
    "stair_shaft_end": stair_shaft_end,
    "road": road,
    "road_marks": road_marks,
    "fence_panel": fence_panel,
    "lamp_post": lamp_post,
    "container": container,
    "neighbour_block": neighbour_block,
    "neighbour_windows": neighbour_windows,
    "weeds_patch": weeds_patch,
    "oil_drums": oil_drums,
}


def main():
    only = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else list(MESHES)
    for name in only:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        MESHES[name]()
        path = os.path.join(OUT_DIR, name + ".blend")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=path, compress=True)
        print(f"Saved {path}")


main()
