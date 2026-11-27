"""Offline geometry bake. Run blender -b -P Tools/bake_archery.py from the project root.

The editor embeds textures from the original content packs. Donor hands and
authoring helpers are excluded. The knife swing and crossbow release sample
their source takes, while the three upright bows use a weighted draw joint.
"""
import lzma
from pathlib import Path
import runpy
import tempfile
import zipfile

import bpy
import numpy as np
from mathutils import Matrix, Vector

TOOLS = Path(__file__).resolve().parent
SOURCE = TOOLS.parent / "SourceArt/Crossroads/Archery"
GLB = runpy.run_path(str(TOOLS / "prepare_sidearm_glb.py"))
MODELS = {
    "PrimitiveBow": ("cool_bow_primitive.glb", (0,-1,0), (1,0,0), 1.25),
    "WoodenBow": ("wooden_bow_free.glb", (-1,0,0), (0,0,1), 1.5),
    "CompoundBow": ("compound-bow.zip", (0,-1,1), (0,1,1), 1.05),
    "Crossbow": ("crossbow_rigged_and_animated.glb", (1,0,0), (0,0,1), 1.00),
    "Knife": ("knife-animated_fps1.zip", (0,-1,0), (0,0,1), .32),
}


def bake(name, spec, temp):
    filename, forward, up, length = spec
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source = SOURCE / filename
    if source.suffix == ".zip":
        with zipfile.ZipFile(source) as archive:
            fbx = next(entry for entry in archive.namelist() if entry.lower().endswith(".fbx"))
            source = Path(archive.extract(fbx, temp))
        bpy.ops.import_scene.fbx(filepath=str(source))
    else:
        bpy.ops.import_scene.gltf(filepath=str(source))
    scene = bpy.context.scene
    start = 40 if name == "Knife" else 1
    scene.frame_set(start)
    objects = [o for o in scene.objects if o.type == "MESH" and o.material_slots and
               (name != "Knife" or o.name == "knife") and (name != "Crossbow" or o.name != "Object_7")]
    dg = bpy.context.evaluated_depsgraph_get()
    forward, up = Vector(forward).normalized(), Vector(up).normalized()
    axes = Matrix((up.cross(forward), up, forward)).to_4x4()
    points = [axes @ (o.evaluated_get(dg).matrix_world @ v.co) for o in objects for v in o.evaluated_get(dg).data.vertices]
    low, high = (Vector([fn(p[i] for p in points) for i in range(3)]) for fn in (min, max))
    center = (low + high) * .5
    scale = length / max(high - low)
    basis = Matrix.Scale(scale, 4) @ Matrix.Translation(-center) @ axes
    doc = {"asset":{"version":"2.0"}, "scene":0, "scenes":[{"nodes":[0,1]}],
           "nodes":[{"name":"FieldRoot","children":[]},{"name":name,"mesh":0,"skin":0}],
           "meshes":[{"name":name,"primitives":[]}], "bufferViews":[], "accessors":[], "buffers":[{}],
           "materials":[], "animations":[], "skins":[]}
    binary = bytearray()

    def accessor(rows, kind, dtype="<f4"):
        array = np.asarray(rows, dtype=dtype)
        binary.extend(bytes(-len(binary) % 4))
        view = len(doc["bufferViews"])
        doc["bufferViews"].append(dict(buffer=0, byteOffset=len(binary), byteLength=array.nbytes))
        binary.extend(array.tobytes())
        acc = dict(bufferView=view, componentType={"<f4":5126,"<u2":5123,"<u4":5125}[dtype], count=len(array), type=kind)
        if kind in ("VEC3","SCALAR"):
            acc.update(min=array.reshape(len(array),-1).min(0).tolist(), max=array.reshape(len(array),-1).max(0).tolist())
        doc["accessors"].append(acc)
        return len(doc["accessors"])-1

    joints = [0]
    def joint(label, position=(0,0,0)):
        index = len(doc["nodes"])
        doc["nodes"].append({"name":label,"translation":list(position)})
        doc["nodes"][0]["children"].append(index)
        joints.append(index)
        return len(joints)-1

    # All landmarks are expressed in the same metre frame as the rest geometry.
    span = .30 if name.endswith("Bow") else (.82 if name=="Crossbow" else .24)
    joint("FieldRear", (0,0,-span/2))
    joint("FieldFront", (0,0,span/2))
    joint("FieldTop", (0,.1,-span/2))
    optic = (0,.078,-.10) if name=="Crossbow" else (.032,.095,.035)
    joint("FieldOptic", optic)
    handbow = name.endswith("Bow")
    # Sibling joints carry component-space motion relative to the first frame.
    bindings = {}
    poses = {}
    def bind(label, pose):
        if label not in bindings:
            bindings[label] = joint("Motion_" + label)
            poses[label] = pose
        return bindings[label]

    if handbow:
        draw = joint("StringDraw")
    for o in objects:
        obj = o.evaluated_get(dg)
        mesh = obj.data
        mesh.calc_loop_triangles()
        mw = obj.matrix_world.copy()
        rig = o.find_armature()
        indices_by_part = {}
        vertices = {}
        arrays = [[] for _ in range(5)]
        for tri in mesh.loop_triangles:
            local_points = [mesh.vertices[i].co for i in tri.vertices]
            middle = basis @ (mw @ (sum(local_points, Vector())/3))
            slot = None
            if name == "WoodenBow":
                slot = int(middle.z < -.105)
            if name == "CompoundBow":
                slot = 2 if any(s in o.name for s in ("Wheel","String","Tension","Ring")) else int("Limb" in o.name)
            if name == "Crossbow":
                slot = {"Object_9":2,"Object_11":1,"Object_19":1,"Object_22":1,"Object_13":3,"Object_28":3,"Object_31":3}.get(o.name,0)
            label = "Body" if slot is None else "GunPart" + str(slot)
            indices = indices_by_part.setdefault(label, [])
            for li in tri.loops:
                loop = mesh.loops[li]
                vi = loop.vertex_index
                uv = mesh.uv_layers.active.data[li].uv
                normal = tuple(round(float(v),5) for v in mesh.corner_normals[li].vector)
                key = (vi, tuple(uv), normal)
                if key not in vertices:
                    p = basis @ (mw @ mesh.vertices[vi].co)
                    n = ((basis @ mw).to_3x3().inverted().transposed() @ Vector(normal)).normalized()
                    weights = [(0,1.)]
                    if handbow:
                        is_string = (name=="PrimitiveBow" and o.name=="Object_6" or name=="WoodenBow" and slot==1
                                     or name=="CompoundBow" and "String_" in o.name)
                        tip = min(1.,abs(p.y)/(length*.5))
                        weight = 1-tip if is_string else .16*tip**2
                        weights = [(0,1-weight),(draw,weight)]
                    elif name=="Knife":
                        weights = [(bind("Blade",lambda o=o: o.matrix_world.copy()),1.)]
                    elif rig:
                        weights = []
                        for g in o.data.vertices[vi].groups:
                            bone = o.vertex_groups[g.group].name
                            if g.weight > 1e-5 and bone in rig.pose.bones:
                                pose = lambda rig=rig,bone=bone: rig.matrix_world @ rig.pose.bones[bone].matrix
                                weights.append((bind(bone,pose),g.weight))
                        weights = sorted(weights, key=lambda w:-w[1])[:4] or [(0,1.)]
                    total = sum(w for _,w in weights)
                    ids = [i for i,_ in weights]; values = [w/total for _,w in weights]
                    vertices[key] = len(arrays[0])
                    for array, value in zip(arrays,(tuple(p),tuple(n),(uv.x,1-uv.y),ids+[0]*(4-len(ids)),values+[0.]*(4-len(ids)))):
                        array.append(value)
                indices.append(vertices[key])
            if (basis @ mw).to_3x3().determinant()<0:
                indices[-1],indices[-2] = indices[-2],indices[-1]
        attrs = {key:accessor(rows, kind, dtype) for rows,(key,kind,dtype) in zip(arrays,[
            ("POSITION","VEC3","<f4"),("NORMAL","VEC3","<f4"),("TEXCOORD_0","VEC2","<f4"),
            ("JOINTS_0","VEC4","<u2"),("WEIGHTS_0","VEC4","<f4")])}
        for label, indices in indices_by_part.items():
            mat = {"name":label}
            if mat not in doc["materials"]: doc["materials"].append(mat)
            doc["meshes"][0]["primitives"].append(dict(attributes=attrs,indices=accessor(indices,"SCALAR","<u4"),material=doc["materials"].index(mat)))
    inverse_binds = []
    for j in joints:
        m = Matrix.Translation(-Vector(doc["nodes"][j].get("translation",(0,0,0))))
        inverse_binds.append(np.array(m).T.flatten())
    doc["skins"].append(dict(joints=joints,skeleton=0,inverseBindMatrices=accessor(inverse_binds,"MAT4")))
    clip = {"name":"Action","samplers":[],"channels":[]}
    def channel(node, path, times, values, kind):
        index = len(clip["samplers"])
        clip["samplers"].append(dict(input=times, output=accessor(values,kind), interpolation="LINEAR"))
        clip["channels"].append(dict(sampler=index,target=dict(node=node,path=path)))
    if handbow:
        t = accessor([0.,1.],"SCALAR")
        channel(joints[draw],"translation",t,[(0,0,0),(0,0,-.28)],"VEC3")
    else:
        frames = list(range(40,62)) if name=="Knife" else list(range(1,21))
        times = accessor([(f-frames[0])/scene.render.fps for f in frames],"SCALAR")
        for label, pose in poses.items():
            scene.frame_set(start)
            rest = pose().inverted()
            trans, rot = [], []
            for frame in frames:
                scene.frame_set(frame)
                transform = basis @ pose() @ rest @ basis.inverted()
                q = transform.to_quaternion()
                if rot and sum(a*b for a,b in zip(rot[-1],(q.x,q.y,q.z,q.w)))<0: q.negate()
                rot.append((q.x,q.y,q.z,q.w));trans.append(tuple(transform.translation))
            node = joints[bindings[label]]
            channel(node,"translation",times,trans,"VEC3")
            channel(node,"rotation",times,rot,"VEC4")
    doc["animations"].append(clip)
    binary.extend(bytes(-len(binary)%4))
    output = temp / (name+".glb")
    GLB["write_glb"](output,doc,binary)
    (SOURCE/(name+".glb.xz")).write_bytes(lzma.compress(output.read_bytes()))
    print("Prepared", name, len(binary))

if __name__=="__main__":
    with tempfile.TemporaryDirectory() as folder:
        for name, spec in MODELS.items():
            bake(name,spec,Path(folder))
