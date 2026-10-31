"""Exports an animated .blend (armature + skinned mesh + actions) to the raw skeletal format
(.skraw) that tools/anim_cook turns into runtime files. Runs inside Blender; cook.bat invokes it
once per file under assets/animated/:

    blender --background --factory-startup <input.blend> --python-exit-code 1 --python pack_skeletal.py -- <output.skraw>

The scene must contain exactly one armature. Every mesh object with an Armature modifier
targeting it is merged into the skinned mesh (other modifiers applied, rest pose); each face's
material gives its vertices a flat color (see material_color, white without a material). Every action
whose slots can animate the armature is exported, sampled at each frame of its frame range.

Coordinates are converted from Blender's right-handed Z-up to the engine's left-handed Y-up by
conjugating every transform with C = swap(y, z): T' = C T C. The scene looks the same after
the conversion, so triangle winding is reversed to stay clockwise for D3D (see pack_mesh.py).

Output layout (little endian), read by tools/anim_cook/anim_cook.cpp:

    char[4] magic           "DXSR"
    u32     version         2
    u32     jointCount      joints ordered parents first
    jointCount x: str name, i32 parent (-1 = root), rest local transform:
                  f32[3] translation, f32[4] rotation (x, y, z, w), f32[3] scale
    u32     animationCount
    animationCount x: str name, f32 frameRate, u32 frameCount,
                  frameCount x jointCount local transforms (same layout as above)
    u32     vertexCount
    vertexCount x: f32[3] position, f32[3] normal, u16[4] joints, f32[4] weights (sum 1),
                  u8[4] color (RGBA, sRGB like the engine's colors)
    u32     indexCount, then u32 indices (clockwise triangle list)
    jointCount x: f32[12] inverse bind matrix (rows 0..2, model space, engine coordinates)
    where str = u32 byte length + UTF-8 bytes.
"""

import os
import struct
import sys

import bpy
from mathutils import Matrix

MAGIC = b"DXSR"
VERSION = 2
MAX_INFLUENCES = 4

# Blender (x, y, z) -> engine (x, z, y). Its own inverse.
C = Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))


def to_engine(m):
    return C @ m @ C


def pack_str(s):
    data = s.encode("utf-8")
    return struct.pack("<I", len(data)) + data


def pack_transform(m, previous_quat=None):
    """Packs a local transform; keeps the quaternion in the same hemisphere as the previous
    sample so interpolation never takes the long way round. Returns (bytes, quat)."""
    loc, quat, scale = m.decompose()
    if previous_quat is not None and quat.dot(previous_quat) < 0.0:
        quat.negate()
    return struct.pack("<3f4f3f", *loc, quat.x, quat.y, quat.z, quat.w, *scale), quat


def material_color(material):
    """Flat RGBA8 color of a material: the Principled BSDF's base color (unless something is
    plugged into it), else the viewport display color. Converted from Blender's linear colors
    to sRGB, the space colors are authored in on the engine side."""
    if material is None:
        return (255, 255, 255, 255)
    color = material.diffuse_color
    if material.node_tree:
        for node in material.node_tree.nodes:
            if node.type == "BSDF_PRINCIPLED" and not node.inputs["Base Color"].is_linked:
                color = node.inputs["Base Color"].default_value
                break

    def srgb(c):
        c = min(max(c, 0.0), 1.0)
        return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1.0 / 2.4) - 0.055

    return tuple(int(round(srgb(c) * 255.0)) for c in color[:3]) + (int(round(min(max(color[3], 0.0), 1.0) * 255.0)),)


def depth_first(bones):
    ordered = []

    def visit(bone):
        ordered.append(bone)
        for child in bone.children:
            visit(child)

    for bone in bones:
        if bone.parent is None:
            visit(bone)
    return ordered


def find_armature():
    armatures = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    if len(armatures) != 1:
        raise ValueError(f"expected exactly one armature, found {len(armatures)}")
    return armatures[0]


def model_matrices(armature_obj, bones, pose):
    """Engine model-space matrix per joint: rest (bone.matrix_local) or posed (pose bone)."""
    world = armature_obj.matrix_world
    if pose:
        return [to_engine(world @ armature_obj.pose.bones[b.name].matrix) for b in bones]
    return [to_engine(world @ b.matrix_local) for b in bones]


def local_matrices(models, parents):
    return [models[j] if parents[j] < 0 else models[parents[j]].inverted_safe() @ models[j]
            for j in range(len(models))]


def export_skeleton(armature_obj, bones, parents):
    out = struct.pack("<I", len(bones))
    rest_locals = local_matrices(model_matrices(armature_obj, bones, pose=False), parents)
    for bone, parent, local in zip(bones, parents, rest_locals):
        out += pack_str(bone.name) + struct.pack("<i", parent) + pack_transform(local)[0]
    return out


def armature_actions(armature_obj):
    """Actions that can animate the armature, with the slot to use (Blender 4.4+ slotted
    actions; older versions have no slots)."""
    result = []
    for action in bpy.data.actions:
        slots = getattr(action, "slots", None)
        if slots is None:
            result.append((action, None))
            continue
        slot = next((s for s in slots if s.target_id_type == "OBJECT"), None)
        if slot is not None:
            result.append((action, slot))
    return result


def export_animations(armature_obj, bones, parents):
    scene = bpy.context.scene
    fps = scene.render.fps / scene.render.fps_base
    anim_data = armature_obj.animation_data or armature_obj.animation_data_create()
    saved_action = anim_data.action
    saved_frame = scene.frame_current

    actions = armature_actions(armature_obj)
    out = struct.pack("<I", len(actions))
    try:
        for action, slot in actions:
            anim_data.action = action
            if slot is not None:
                anim_data.action_slot = slot
            start, end = (int(round(f)) for f in action.frame_range)
            frame_count = end - start + 1
            if frame_count < 2:
                raise ValueError(f"action '{action.name}' needs at least 2 frames")

            out += pack_str(action.name) + struct.pack("<fI", fps, frame_count)
            previous = [None] * len(bones)
            for frame in range(start, end + 1):
                scene.frame_set(frame)
                for j, local in enumerate(local_matrices(model_matrices(armature_obj, bones, pose=True), parents)):
                    data, previous[j] = pack_transform(local, previous[j])
                    out += data
            print(f"    action '{action.name}': {frame_count} frames @ {fps:g} fps", file=sys.stderr)
    finally:
        anim_data.action = saved_action
        scene.frame_set(saved_frame)
    return out, len(actions)


def skinned_mesh_objects(armature_obj):
    return [o for o in bpy.context.scene.objects
            if o.type == "MESH" and any(m.type == "ARMATURE" and m.object == armature_obj for m in o.modifiers)]


def export_mesh(armature_obj, bones):
    joint_of_bone = {b.name: j for j, b in enumerate(bones)}
    vertices = []  # (position, normal, joints, weights, color)
    lookup = {}
    indices = []
    unweighted = 0

    mesh_objects = skinned_mesh_objects(armature_obj)
    if not mesh_objects:
        raise ValueError("no mesh is skinned to the armature (Armature modifier)")

    for obj in mesh_objects:
        # Evaluate in rest pose: with the Armature modifiers off, other modifiers still apply.
        armature_modifiers = [m for m in obj.modifiers if m.type == "ARMATURE"]
        saved = [m.show_viewport for m in armature_modifiers]
        for m in armature_modifiers:
            m.show_viewport = False
        depsgraph = bpy.context.evaluated_depsgraph_get()
        depsgraph.update()
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        try:
            world = to_engine(obj.matrix_world)
            normal_matrix = world.to_3x3().inverted_safe().transposed()
            group_names = {g.index: g.name for g in obj.vertex_groups}
            colors = [material_color(slot.material) for slot in obj.material_slots] or [material_color(None)]

            # Per vertex: up to 4 strongest bone influences, normalized.
            influences = []
            for v in mesh.vertices:
                pairs = [(g.weight, joint_of_bone[group_names[g.group]]) for g in v.groups
                         if g.weight > 0.0 and group_names.get(g.group) in joint_of_bone]
                pairs.sort(reverse=True)
                pairs = pairs[:MAX_INFLUENCES]
                total = sum(w for w, _ in pairs)
                if total <= 0.0:
                    unweighted += 1
                    pairs, total = [(1.0, 0)], 1.0  # follows the root joint
                pairs += [(0.0, 0)] * (MAX_INFLUENCES - len(pairs))
                influences.append((tuple(j for _, j in pairs), tuple(w / total for w, _ in pairs)))

            mesh.calc_loop_triangles()
            corner_normals = mesh.corner_normals
            for tri in mesh.loop_triangles:
                corners = []
                color = colors[min(tri.material_index, len(colors) - 1)]
                for loop_index in tri.loops:
                    vertex_index = mesh.loops[loop_index].vertex_index
                    position = tuple(world @ C @ mesh.vertices[vertex_index].co)
                    normal = tuple((normal_matrix @ C.to_3x3() @ corner_normals[loop_index].vector).normalized())
                    corners.append((position, normal) + influences[vertex_index] + (color,))
                for corner in (corners[0], corners[2], corners[1]):  # CCW -> CW
                    index = lookup.get(corner)
                    if index is None:
                        index = len(vertices)
                        lookup[corner] = index
                        vertices.append(corner)
                    indices.append(index)
        finally:
            evaluated.to_mesh_clear()
            for m, show in zip(armature_modifiers, saved):
                m.show_viewport = show

    if unweighted:
        print(f"    warning: {unweighted} vertices without bone weights follow the root joint", file=sys.stderr)

    out = struct.pack("<I", len(vertices))
    out += b"".join(struct.pack("<3f3f4H4f4B", *p, *n, *j, *w, *c) for p, n, j, w, c in vertices)
    out += struct.pack("<I", len(indices)) + struct.pack(f"<{len(indices)}I", *indices)
    return out, len(vertices), len(indices) // 3


def export_inverse_binds(armature_obj, bones):
    out = b""
    for model in model_matrices(armature_obj, bones, pose=False):
        inverse = model.inverted_safe()
        out += struct.pack("<12f", *(inverse[r][c] for r in range(3) for c in range(4)))
    return out


def pack(dst_path):
    armature_obj = find_armature()
    bones = depth_first(armature_obj.data.bones)
    index_of = {b.name: j for j, b in enumerate(bones)}
    parents = [index_of[b.parent.name] if b.parent else -1 for b in bones]

    skeleton = export_skeleton(armature_obj, bones, parents)
    animations, animation_count = export_animations(armature_obj, bones, parents)
    mesh, vertex_count, triangle_count = export_mesh(armature_obj, bones)
    inverse_binds = export_inverse_binds(armature_obj, bones)

    dst_dir = os.path.dirname(dst_path)
    if dst_dir:
        os.makedirs(dst_dir, exist_ok=True)
    tmp_path = dst_path + ".tmp"
    with open(tmp_path, "wb") as f:
        f.write(struct.pack("<4sI", MAGIC, VERSION))
        f.write(skeleton + animations + mesh + inverse_binds)
    os.replace(tmp_path, dst_path)  # never leave a half-written file behind

    return f"{len(bones)} joints, {animation_count} animations, {vertex_count} vertices, {triangle_count} triangles"


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(args) != 1:
        raise SystemExit("usage: blender -b <input.blend> --python pack_skeletal.py -- <output.skraw>")
    summary = pack(args[0])
    # stderr: cook.bat hides Blender's stdout chatter.
    print(f"  {os.path.basename(bpy.data.filepath)} -> {summary}", file=sys.stderr)


main()
