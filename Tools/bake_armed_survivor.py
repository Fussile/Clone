"""Rebuild the bundled survivor weapon clips with Blender 4.3.

blender -b --python-exit-code 1 --python Tools/bake_armed_survivor.py -- SourceArt/Crossroads SourceArt/Crossroads/Survivor/SK_ArmedSurvivor.glb
"""
import bpy, math, sys, os
from mathutils import Matrix, Vector

base=sys.argv[sys.argv.index('--')+1]
out=sys.argv[sys.argv.index('--')+2]
def load(path):
    before=set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(base,path))
    return set(bpy.data.objects)-before

def sample(path, names, frames, clip=None):
    objects = load(path)
    rig = next(obj for obj in objects if obj.type == 'ARMATURE')
    if clip:
        animation = rig.animation_data
        track = next(track for track in animation.nla_tracks if clip in track.name)
        animation.use_nla = False
        animation.action = track.strips[0].action
        start, end = animation.action.frame_range
        frames = [min(start + frame, end) for frame in frames]
        frames[-1] = end
    scene = bpy.context.scene
    positions = []
    for frame in frames:
        scene.frame_set(math.floor(frame), subframe=frame % 1)
        positions.append([rig.pose.bones[name].head.copy() for name in names])
    if clip:
        # The pistol donor is in camera space. Local -Z points forward and -Y up.
        def convert(offset):
            return Vector((offset.x, offset.z, -offset.y)) * .65
    else:
        rifle_transform = rig.matrix_world.to_3x3() * .009

        def convert(offset):
            return rifle_transform @ offset
    for obj in objects:
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.context.view_layer.update()
    return [[convert(value - origin) for value, origin in zip(row, positions[0])] for row in positions]


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps=24
body_objects=load('Survivor/SK_Survivor.glb')
rig=next(x for x in body_objects if x.type=='ARMATURE')
body=next(x for x in body_objects if x.type=='MESH' and x.parent==rig)
for t in rig.animation_data.nla_tracks:
    t.name=t.name.split('_SK_Survivor')[0]
    for st in t.strips:
        st.action.name=t.name

rifle_samples = sample('Weapons/sniper_animated.glb', ['R_wrist_027', 'L_wrist_03'], range(94))
pistol_channels = ['Hand_R_038', 'Hand_L_016', 'Pmag_061', 'PBody_058']
pistol_clips = [
    ('SurvivorPistolHold', 'Pistol_Idle', 48),
    ('SurvivorPistolFire', 'Pistol_Fire', 6),
    ('SurvivorPistolReload', 'Reload_easy', 32),
    ('SurvivorPistolReloadEmpty', 'Reload_full', 40),
]
pistol_samples = {
    name: sample('Weapons/fps_pistol_animations.glb', pistol_channels, range(frame_count), track)
    for name, track, frame_count in pistol_clips
}

def geometry(path, prefix=None):
    meshes = [obj for obj in load(path) if obj.type == 'MESH']
    bpy.context.view_layer.update()
    return sorted((obj for obj in meshes if not prefix or obj.name.startswith(prefix)), key=lambda obj: obj.name)

def mount(meshes, transform, name):
    for obj in meshes:
        world_transform = transform @ obj.matrix_world
        obj.data.transform(world_transform)
        obj.parent = None
        obj.matrix_world = Matrix.Identity(4)
        if world_transform.is_negative:
            for poly in obj.data.polygons:
                poly.flip()
        group = obj.vertex_groups.new(name=name)
        group.add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')
        obj.select_set(True)

def bounds(meshes):
    points = [obj.matrix_world @ vertex.co for obj in meshes for vertex in obj.data.vertices]
    return points, [min(point[i] for point in points) for i in range(3)], [max(point[i] for point in points) for i in range(3)]

rifle_meshes = geometry('Weapons/wooden_bolt_rifle.glb')
points, low, high = bounds(rifle_meshes)
# Blender's world-space source barrel runs along Y, with Z measuring stock depth.
axis = max(range(3), key=lambda i: high[i] - low[i])
assert axis == 1
lo, hi = low[axis], high[axis]
def depth(vertices):
    return max(point.z for point in vertices) - min(point.z for point in vertices)
sign = 1 if depth([v for v in points if v.y < lo + .14]) > depth([v for v in points if v.y > hi - .14]) else -1
center_x, center_z = (low[0] + high[0]) / 2, (low[2] + high[2]) / 2
scale = .98 / (hi - lo)
rifle_transform = Matrix((
    (scale, 0, 0, -center_x * scale),
    (0, -sign * scale, 0, .22 + (lo if sign > 0 else -hi) * scale),
    (0, 0, scale, .015 - center_z * scale),
    (0, 0, 0, 1),
))

pistol_meshes = geometry('Weapons/SM_CrossroadsSidearm.glb')
pistol_transform = Matrix(((0, .9, 0, 0), (-.9, 0, 0, -.025), (0, 0, .9, .045), (0, 0, 0, 1)))
mag_meshes = geometry('Weapons/SM_Magazine.glb', 'Kama.Duty.Magazine')
_, mag_low, mag_high = bounds(mag_meshes)
mag_x, mag_y = (mag_low[0] + mag_high[0]) / 2, (mag_low[1] + mag_high[1]) / 2
mag_transform = Matrix(((0, .72, 0, -.72 * mag_y), (-.50, 0, 0, .012 + .50 * mag_x),
                        (0, 0, .85, -.099 - .85 * mag_low[2]), (0, 0, 0, 1)))

bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for name in ('RifleMount', 'PistolMount', 'PistolMagazine'):
    bone = rig.data.edit_bones.new(name)
    bone.head = (0, 0, 0)
    bone.tail = (0, .1, 0)
    bone.parent = rig.data.edit_bones['PistolMount' if name == 'PistolMagazine' else 'Hips']
bpy.ops.object.mode_set(mode='OBJECT')
mount(rifle_meshes, rifle_transform, 'RifleMount')
mount(pistol_meshes, pistol_transform, 'PistolMount')
mount(mag_meshes, mag_transform, 'PistolMagazine')
for image in bpy.data.images:
    if max(image.size) > 384 and image.users:
        factor = 384 / max(image.size)
        image.scale(max(1, round(image.size[0] * factor)), max(1, round(image.size[1] * factor)))
        image.pack()
rig.select_set(False)
body.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.join()

def aim_at(bone,target):
    aim=(target-bone.head).normalized()
    delta=bone.matrix.to_3x3().col[1].normalized().rotation_difference(aim)
    rot=delta.to_matrix().to_4x4() @ bone.matrix
    rot.translation=bone.head
    bone.matrix=rot
    bpy.context.view_layer.update()

def solve(side,wrist):
    upper,lower,hand=(rig.pose.bones[side+x] for x in ('Arm','ForeArm','Hand'))
    origin=upper.head.copy()
    direction=(wrist-origin).normalized()
    length=min((wrist-origin).length,upper.length+lower.length-.004)
    wrist=origin+direction*length
    a=(upper.length**2-lower.length**2+length**2)/(2*length)
    pole=Vector((-.7 if side=='Right' else .7,.18,-1))
    pole=(pole-direction*direction.dot(pole)).normalized()
    elbow=origin+direction*a+pole*math.sqrt(max(0,upper.length**2-a*a))
    aim_at(upper,elbow)
    aim_at(lower,wrist)
    aim_at(hand,hand.head+Vector((0,-1,-.22)))

for t in rig.animation_data.nla_tracks: t.mute=True
clips = [
    ('SurvivorRifleHold', [rifle_samples[0]] * 2),
    ('SurvivorRifleFire', rifle_samples[:50]),
    ('SurvivorRifleReload', rifle_samples[49:93]),
    *pistol_samples.items(),
]
for name, samples in clips:
    action = bpy.data.actions.new(name)
    rig.animation_data.action = action
    pistol = 'Pistol' in name
    for frame, offsets in enumerate(samples, 1):
        for bone in rig.pose.bones:
            bone.rotation_mode = 'QUATERNION'
            bone.matrix_basis = Matrix.Identity(4)
        bpy.context.view_layer.update()
        right, left = offsets[:2]
        if pistol:
            pivot = Vector((-.055, -.32, 1.36)) + right
            solve('Right', Vector((-.10, -.29, 1.33)) + right)
            solve('Left', Vector((.025, -.33, 1.30)) + left)
            rig.pose.bones['PistolMount'].matrix = Matrix.Translation(pivot)
            bpy.context.view_layer.update()
            magazine = offsets[2] - offsets[3]
            # Transfer the original magazine's travel relative to the pistol grip.
            rig.pose.bones['PistolMagazine'].matrix = Matrix.Translation(pivot + magazine)
        else:
            solve('Right', Vector((-.15,-.29,1.33)) + right)
            solve('Left', Vector((-.02,-.38,1.36)) + left)
            offset = left if name == 'SurvivorRifleFire' else right
            rig.pose.bones['RifleMount'].matrix = Matrix.Translation(Vector((-.105,-.32,1.37)) + offset)
        for bone in rig.pose.bones:
            bone.keyframe_insert(data_path='rotation_quaternion', frame=frame, group=bone.name)
            if bone.name in ('RifleMount', 'PistolMount', 'PistolMagazine'):
                bone.keyframe_insert(data_path='location', frame=frame, group=bone.name)
    rig.animation_data.action=None
    track=rig.animation_data.nla_tracks.new()
    track.name=name
    track.strips.new(name,1,action)
    track.mute=True

bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True);body.select_set(True)
bpy.context.view_layer.objects.active=rig
for t in rig.animation_data.nla_tracks: t.mute=False
bpy.ops.export_scene.gltf(filepath=out,export_format='GLB',use_selection=True,export_animations=True,export_animation_mode='NLA_TRACKS',export_image_format='JPEG',export_jpeg_quality=82)
