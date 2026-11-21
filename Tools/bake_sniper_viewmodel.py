"""Bake the content branch's sniper FBX onto DJMaesen's original animated hands.

blender -b --python-exit-code 1 --python Tools/bake_sniper_viewmodel.py -- \
    SourceArt/Crossroads/Weapons /path/to/Weapons/sniper
"""
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

weapons, source = map(Path, sys.argv[sys.argv.index('--') + 1:])
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = 24
bpy.ops.import_scene.gltf(filepath=str(weapons / 'sniper_animated.glb'))
rig = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
arms = next(obj for obj in rig.children if obj.type == 'MESH')
parts = ('base', 'scope', 'boltback', 'bolt', 'release', 'trigger', 'mag', 'bullet')
nodes = {name: bpy.data.objects[name] for name in parts}
clip_frames = {
    'SniperHold': (0, 0),
    'SniperFire': range(0, 50),
    'SniperReload': range(49, 94),
}
frames = {}
for frame in range(94):
    scene.frame_set(frame)
    frames[frame] = (
        {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones},
        {name: node.matrix_world.copy() for name, node in nodes.items()},
    )
node_rest = frames[0][1]

# Keep the donor armature and its arms in their authored world placement.
arm_world = rig.matrix_world.copy()
rig.parent = None
rig.matrix_world = arm_world
rig.animation_data_clear()
for obj in list(scene.objects):
    if obj not in (rig, arms):
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.import_scene.fbx(filepath=str(source / 'source' / 'sniper.fbx'))
props = {obj.name: obj for obj in scene.objects if obj.type == 'MESH' and obj != arms}
bpy.context.view_layer.update()

def bbox(obj):
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    return min(p.y for p in corners), max(p.y for p in corners)

# Fit the FBX barrel and stock to the matching model in the animation's frame zero.
low, high = bbox(props['base'])
donor_low, donor_high = -97.2348, 56.2235
factor = (donor_high - donor_low) / (high - low)
align = Matrix.Translation(Vector((0, donor_low - factor * low, 0))) @ Matrix.Scale(factor, 4)
# Scale after translating in donor units.
model_space = Matrix.Scale(1.18 / (donor_high - donor_low), 4) @ Matrix.Translation(Vector((0, -donor_high, 0)))

material = props['base'].data.materials[0]
material.use_nodes = True
material.node_tree.nodes.clear()
principled = material.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
output = material.node_tree.nodes.new('ShaderNodeOutputMaterial')
links = material.node_tree.links
links.new(principled.outputs['BSDF'], output.inputs['Surface'])
for suffix, socket in (('Color', 'Base Color'), ('Normal', 'Normal'), ('Roughness', 'Roughness'), ('Metallic', 'Metallic')):
    node = material.node_tree.nodes.new('ShaderNodeTexImage')
    node.image = bpy.data.images.load(str(source / 'textures' / f'sniper{suffix}.png'))
    if suffix != 'Color':
        node.image.colorspace_settings.name = 'Non-Color'
    node.image.scale(768, 768)
    node.image.pack()
    color = node.outputs['Color']
    if suffix == 'Normal':
        normal = material.node_tree.nodes.new('ShaderNodeNormalMap')
        links.new(color, normal.inputs['Color'])
        color = normal.outputs['Normal']
    links.new(color, principled.inputs[socket])

# The world mesh uses the same geometry and material, at its final metric size.
bpy.ops.object.select_all(action='DESELECT')
for name, prop in props.items():
    if name != 'bullet':
        prop.select_set(True)
for name, prop in props.items():
    if name == 'bullet':
        continue
    copy = prop.copy()
    copy.data = prop.data.copy()
    scene.collection.objects.link(copy)
    transform = model_space @ align @ prop.matrix_world
    transform.translation.y += .34
    transform.translation.z += .035
    copy.data.transform(Matrix.Rotation(1.57079632679, 4, 'Z') @ transform)
    copy.parent = None
    copy.matrix_world = Matrix.Identity(4)
    prop.select_set(False)
    copy.select_set(True)
bpy.context.view_layer.objects.active = next(obj for obj in bpy.context.selected_objects if obj.type == 'MESH')
bpy.ops.object.join()
world = bpy.context.object
world.name = 'SM_CrossroadsSniper'
bpy.ops.export_scene.gltf(filepath=str(weapons / 'SM_CrossroadsSniper.glb'), export_format='GLB',
                         use_selection=True, export_animations=False, export_image_format='JPEG', export_jpeg_quality=83)
bpy.data.objects.remove(world, do_unlink=True)

bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
bones = rig.data.edit_bones
arm_inverse = rig.matrix_world.inverted()
for name in (*parts, 'ViewBreech', 'ViewMuzzle'):
    bone = bones.new(name if name.startswith('View') else 'Sniper_' + name)
    bone.parent = bones['_rootJoint']
    start = (0, 18, 4) if name == 'ViewBreech' else (0, donor_low, 4) if name == 'ViewMuzzle' else (0, 0, 0)
    bone.head = arm_inverse @ Vector(start)
    bone.tail = arm_inverse @ (Vector(start) + Vector((0, 0, 10)))
bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.object.select_all(action='DESELECT')
bpy.context.view_layer.objects.active = arms
arms.select_set(True)
bpy.context.view_layer.update()
arm_mesh_inverse = arms.matrix_world.inverted()
for name in parts:
    prop = props[name]
    prop.data.transform(arm_mesh_inverse @ align @ prop.matrix_world)
    prop.parent = None
    prop.matrix_world = arms.matrix_world.copy()
    group = prop.vertex_groups.new(name='Sniper_' + name)
    group.add(list(range(len(prop.data.vertices))), 1.0, 'REPLACE')
    prop.select_set(True)
bpy.ops.object.join()

animation = rig.animation_data_create()
for clip, sequence in clip_frames.items():
    action = bpy.data.actions.new(clip)
    animation.action = action
    for key, source_frame in enumerate(sequence, 1):
        # Start and end on the idle pose so fire and reload can return to it without a snap.
        if key == 1 or key == len(sequence):
            source_frame = 0
        pose, world_matrices = frames[source_frame]
        for name, basis in pose.items():
            rig.pose.bones[name].matrix_basis = basis
        bpy.context.view_layer.update()
        for name in (*parts, 'ViewBreech', 'ViewMuzzle'):
            prop = 'base' if name.startswith('View') else name
            bone = rig.pose.bones[name if name.startswith('View') else 'Sniper_' + name]
            delta = world_matrices[prop] @ node_rest[prop].inverted()
            bone.matrix = arm_inverse @ delta @ arm_world @ bone.bone.matrix_local
        for bone in rig.pose.bones:
            bone.keyframe_insert(data_path='location', frame=key, group=bone.name)
            bone.keyframe_insert(data_path='rotation_quaternion', frame=key, group=bone.name)
            bone.keyframe_insert(data_path='scale', frame=key, group=bone.name)
    animation.action = None
    track = animation.nla_tracks.new()
    track.name = clip
    track.strips.new(clip, 1, action)
    track.mute = True
for track in animation.nla_tracks:
    track.mute = False
scene.frame_set(1)
rig.select_set(True)
arms.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(weapons / 'SK_SniperViewmodel.glb'), export_format='GLB',
                         use_selection=True, export_animations=True, export_animation_mode='NLA_TRACKS',
                         export_image_format='JPEG', export_jpeg_quality=83)
