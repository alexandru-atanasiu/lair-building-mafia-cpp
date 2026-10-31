"""Packs the meshes of a .blend file into the engine's mesh format (.mesh). Runs inside Blender;
cook.bat invokes it once per file:

    blender --background --factory-startup <input.blend> --python-exit-code 1 --python pack_mesh.py -- <output.mesh>

All mesh objects of the scene are merged into one mesh, with their object transforms and
modifiers applied. Normals are Blender's corner (split) normals, so flat/smooth shading and
sharp edges come out as authored: vertices are shared wherever position and normal match.

Coordinates are converted from Blender's right-handed Z-up to the engine's left-handed Y-up
(x, y, z) -> (x, z, y). The scene looks the same afterwards, so front faces are still
counter-clockwise on screen: each triangle's winding is reversed to the clockwise D3D expects.

Output layout (little endian), mirrored by include/render/mesh.h:

    char[4] magic        "DXMS"
    u32     version      1
    u32     vertexCount
    u32     indexCount   multiple of 3 (triangle list)
    f32[3]  boundsMin    axis-aligned bounding box
    f32[3]  boundsMax
    f32[3]  sphereCenter bounding sphere
    f32     sphereRadius
    then vertexCount vertices: f32[3] position, f32[3] normal
    then indexCount u32 indices
"""

import math
import os
import struct
import sys

import bpy

MAGIC = b"DXMS"
VERSION = 1


def to_engine(v):
    return (v[0], v[2], v[1])


def gather_triangles():
    """Yields (position, normal) triples per triangle, in engine space."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        try:
            world = obj.matrix_world
            normal_matrix = world.to_3x3().inverted_safe().transposed()
            mesh.calc_loop_triangles()
            corner_normals = mesh.corner_normals
            for tri in mesh.loop_triangles:
                corners = []
                for loop_index in tri.loops:
                    vertex_index = mesh.loops[loop_index].vertex_index
                    position = world @ mesh.vertices[vertex_index].co
                    normal = (normal_matrix @ corner_normals[loop_index].vector).normalized()
                    corners.append((to_engine(position), to_engine(normal)))
                yield (corners[0], corners[2], corners[1])  # CCW -> CW
        finally:
            evaluated.to_mesh_clear()


def pack(dst_path: str) -> str:
    vertices = []  # (position, normal)
    lookup = {}
    indices = []
    for corners in gather_triangles():
        for corner in corners:
            index = lookup.get(corner)
            if index is None:
                index = len(vertices)
                lookup[corner] = index
                vertices.append(corner)
            indices.append(index)

    if not indices:
        raise ValueError("no mesh triangles in the scene")

    positions = [v[0] for v in vertices]
    bmin = tuple(min(p[i] for p in positions) for i in range(3))
    bmax = tuple(max(p[i] for p in positions) for i in range(3))
    center = tuple((bmin[i] + bmax[i]) * 0.5 for i in range(3))
    radius = max(math.dist(center, p) for p in positions)

    dst_dir = os.path.dirname(dst_path)
    if dst_dir:
        os.makedirs(dst_dir, exist_ok=True)
    tmp_path = dst_path + ".tmp"
    with open(tmp_path, "wb") as f:
        f.write(struct.pack("<4s3I", MAGIC, VERSION, len(vertices), len(indices)))
        f.write(struct.pack("<3f3f3ff", *bmin, *bmax, *center, radius))
        f.write(b"".join(struct.pack("<3f3f", *p, *n) for p, n in vertices))
        f.write(struct.pack(f"<{len(indices)}I", *indices))
    os.replace(tmp_path, dst_path)  # never leave a half-written .mesh behind

    return f"{len(vertices)} vertices, {len(indices) // 3} triangles"


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(args) != 1:
        raise SystemExit("usage: blender -b <input.blend> --python pack_mesh.py -- <output.mesh>")
    summary = pack(args[0])
    # stderr: cook.bat hides Blender's stdout chatter.
    print(f"  {os.path.basename(bpy.data.filepath)} -> {summary}", file=sys.stderr)


main()
