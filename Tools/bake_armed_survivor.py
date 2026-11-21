"""Rebuild the bundled survivor rifle clips with Blender 4.3.

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

def sample(path,names,frames,regex=None):
    objects=load(path)
    rig=next(x for x in objects if x.type=='ARMATURE')
    track=None
    if regex:
        for t in rig.animation_data.nla_tracks:
            t.mute=True
            if regex in t.name: track=t
        assert track, [t.name for t in rig.animation_data.nla_tracks]
        track.mute=False
    scene=bpy.context.scene
    positions=[]
    for frame in frames:
        scene.frame_set(frame)
        positions.append([rig.matrix_world.to_3x3() @ rig.pose.bones[n].head for n in names])
    for o in objects:
        bpy.data.objects.remove(o, do_unlink=True)
    return [[(v-origin)*0.009 for v,origin in zip(row,positions[0])] for row in positions]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps=24
body_objects=load('Survivor/SK_Survivor.glb')
rig=next(x for x in body_objects if x.type=='ARMATURE')
body=next(x for x in body_objects if x.type=='MESH' and x.parent==rig)
for t in rig.animation_data.nla_tracks:
    t.name=t.name.split('_SK_Survivor')[0]
    for st in t.strips:
        st.action.name=t.name

samples=sample('Weapons/sniper_animated.glb', ['R_wrist_027','L_wrist_03'], range(94))
rifle_meshes=[o for o in load('Weapons/wooden_bolt_rifle.glb') if o.type=='MESH']
points=[o.matrix_world @ v.co for o in rifle_meshes for v in o.data.vertices]
lo=min(v.z for v in points); hi=max(v.z for v in points)
# The rear is the end with the deeper stock. Stock and muzzle stay aligned to the character forward axis.
def depth(vertices): return max(p.y for p in vertices)-min(p.y for p in vertices)
sign=1 if depth([v for v in points if v.z<lo+.14])>depth([v for v in points if v.z>hi-.14]) else -1
cx=(min(v.x for v in points)+max(v.x for v in points))/2
cy=(min(v.y for v in points)+max(v.y for v in points))/2
rifle_scale=1.02/(hi-lo)
mount_transform = Matrix((
    (rifle_scale, 0, 0, -cx * rifle_scale),
    (0, 0, -sign * rifle_scale, .24 + (lo if sign > 0 else -hi) * rifle_scale),
    (0, rifle_scale, 0, .035 - cy * rifle_scale),
    (0, 0, 0, 1),
))
for o in rifle_meshes:
    transform = mount_transform @ o.matrix_world
    o.data.transform(transform)
    o.parent = None
    o.matrix_world = Matrix.Identity(4)
    if transform.is_negative:
        for poly in o.data.polygons:
            poly.flip()
    for image in bpy.data.images:
        if max(image.size)>384 and image.users:
            f=384/max(image.size)
            image.scale(max(1,round(image.size[0]*f)),max(1,round(image.size[1]*f)))
            image.pack()
bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True); bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode='EDIT')
bone=rig.data.edit_bones.new('RifleMount')
bone.head=(0,0,0);bone.tail=(0,.1,0);bone.parent=rig.data.edit_bones['Hips']
bpy.ops.object.mode_set(mode='OBJECT')
for o in rifle_meshes:
    group=o.vertex_groups.new(name='RifleMount')
    group.add(list(range(len(o.data.vertices))),1,'REPLACE')
    o.select_set(True)
rig.select_set(False)
body.select_set(True)
bpy.context.view_layer.objects.active=body
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
for name,first,last in [('SurvivorRifleHold',0,1),('SurvivorRifleFire',0,49),('SurvivorRifleReload',49,92)]:
    action=bpy.data.actions.new(name)
    rig.animation_data.action=action
    for f in range(first,last+1):
        for bone in rig.pose.bones:
            bone.matrix_basis=Matrix.Identity(4)
        bpy.context.view_layer.update()
        right,left=samples[f]
        if name=='SurvivorRifleHold': right=left=Vector((0,0,0))
        right_goal=Vector((-.15,-.29,1.33))+right
        left_goal=Vector((-.02,-.38,1.36))+left
        solve('Right',right_goal);solve('Left',left_goal)
        weapon=rig.pose.bones['RifleMount']
        # Rifles stay on the support hand while the other hand cycles the action.
        offset=left if name=='SurvivorRifleFire' else right
        weapon.matrix=Matrix.Translation(Vector((-.105,-.32,1.37))+offset)
        for bone in rig.pose.bones:
            bone.rotation_mode='QUATERNION'
            bone.keyframe_insert(data_path='rotation_quaternion',frame=f-first+1,group=bone.name)
            if bone==weapon: bone.keyframe_insert(data_path='location',frame=f-first+1,group=bone.name)
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
