"""Creates the crew soldier source assets/animated/soldier.blend with Blender, from the Mixamo
FBX exports in raw/:

    blender --background --factory-startup --python py/create_soldier.py

placeholder_soldier_t_pose.fbx gives the armature and the two skinned meshes; each animation
FBX (same skeleton) contributes its action, renamed (keep the names in sync with
src/game/character/character_view.cpp):

  idle  placeholder_soldier_anim_idle.fbx     (looping)
  run   placeholder_soldier_anim_running.fbx  (looping, in place)
  die   placeholder_soldier_anim_dying.fbx    (one shot, not used by the game yet)

Materials become flat colors that py/pack_skeletal.py bakes into the vertices: the body
(first material) near white, so the per-character tint decides its color; the joints (second
material) near black.

The soldier faces -Y in Blender (-Z in the game), stands on z = 0 and is about 1.8 units
tall (a world unit is about a metre). The FBX armature keeps its import transform (rotated 90
degrees about X, scaled 0.01): pack_skeletal.py bakes object transforms into the skeleton.
"""

import os

import bpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW_DIR = os.path.join(ROOT, "raw")
OUT_PATH = os.path.join(ROOT, "assets", "animated", "soldier.blend")
FPS = 30

MESH_FBX = "placeholder_soldier_t_pose.fbx"
ACTIONS = [
    ("idle", "placeholder_soldier_anim_idle.fbx"),
    ("run",  "placeholder_soldier_anim_running.fbx"),
    ("die",  "placeholder_soldier_anim_dying.fbx"),
]
# The FBX's materials (in its order), linear RGBA.
MATERIAL_COLORS = {
    "Alpha_Body_MAT":   (0.9, 0.9, 0.9, 1.0),   # tinted per character
    "Alpha_Joints_MAT": (0.03, 0.03, 0.03, 1.0),
}


def import_fbx(name):
    """Imports raw/<name> and returns the objects it created."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(RAW_DIR, name))
    return [o for o in bpy.data.objects if o not in before]


def only_armature(objects, name):
    armatures = [o for o in objects if o.type == "ARMATURE"]
    if len(armatures) != 1:
        raise ValueError(f"{name}: expected one armature, found {len(armatures)}")
    return armatures[0]


def set_flat_color(material, color):
    material.diffuse_color = color
    if material.node_tree:
        for node in material.node_tree.nodes:
            if node.type == "BSDF_PRINCIPLED":
                base = node.inputs["Base Color"]
                for link in list(base.links):
                    material.node_tree.links.remove(link)
                base.default_value = color


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.render.fps_base = 1.0

    # Armature + meshes. The t-pose FBX's own (2 frame) action isn't wanted.
    armature = only_armature(import_fbx(MESH_FBX), MESH_FBX)
    armature.name = "soldier"
    if armature.animation_data and armature.animation_data.action:
        bpy.data.actions.remove(armature.animation_data.action)
    armature.animation_data_clear()
    for obj in bpy.data.objects:
        if obj.type == "MESH" and obj.data.users > 0:
            obj.data.name = obj.name

    used = {slot.material.name for o in bpy.data.objects if o.type == "MESH" for slot in o.material_slots if slot.material}
    if used != set(MATERIAL_COLORS):
        raise ValueError(f"expected materials {sorted(MATERIAL_COLORS)}, found {sorted(used)}")
    for name, color in MATERIAL_COLORS.items():
        set_flat_color(bpy.data.materials[name], color)
        print(f"  material '{name}' -> {color}")

    # Actions: take each FBX's action, drop the rest of what it imported.
    for action_name, fbx in ACTIONS:
        objects = import_fbx(fbx)
        source = only_armature(objects, fbx)
        action = source.animation_data.action if source.animation_data else None
        if action is None:
            raise ValueError(f"{fbx}: no action")
        action.name = action_name
        action.use_fake_user = True
        for obj in objects:
            data = obj.data
            bpy.data.objects.remove(obj)
            if data is not None and data.users == 0:
                if isinstance(data, bpy.types.Armature):
                    bpy.data.armatures.remove(data)
                elif isinstance(data, bpy.types.Mesh):
                    bpy.data.meshes.remove(data)
        start, end = action.frame_range
        print(f"  action '{action_name}': frames {start:g}-{end:g}")

    # Show the idle in the viewport.
    anim_data = armature.animation_data_create()
    anim_data.action = bpy.data.actions["idle"]
    slots = getattr(anim_data.action, "slots", None)
    if slots:
        anim_data.action_slot = slots[0]
    scene.frame_start, scene.frame_end = (int(f) for f in anim_data.action.frame_range)

    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=OUT_PATH, compress=True)
    print(f"Wrote {OUT_PATH}")


main()
