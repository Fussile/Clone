"""Turn the five combined donor timelines into independently playable game actions."""
from bisect import bisect_left, bisect_right
import math
from pathlib import Path
import runpy
import struct


# Ready frame, authored shot interval (where present), reload interval, idle period,
# step period and game recoil displacement, in seconds and metres.
TAKES = {
    "AK12": (2.41667, None, (0, 2.41667), 3.1, .76, .034),
    "HK416": (21.8, (21.93, 22.34), (8.6, 14.1), 3.2, .80, .028),
    "MP7": (0, None, (0, 2.8), 2.7, .64, .021),
    "Scorpion": (3.33, None, (0, 2.45), 2.9, .70, .026),
    "Python357": (0, (0, .38), (.60, 7.25), 2.8, .72, .028),
}


def values(doc, binary, index):
    acc = doc["accessors"][index]
    view = doc["bufferViews"][acc["bufferView"]]
    width = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[acc["type"]]
    kind = {5125: "I", 5126: "f"}[acc["componentType"]]
    row = struct.Struct("<" + kind * width)
    start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    return list(row.iter_unpack(memoryview(binary)[start:start + row.size * acc["count"]]))


def append(doc, binary, rows, kind, integer=False):
    dtype = "I" if integer else "f"
    data = b"".join(struct.pack("<" + dtype * len(row), *row) for row in rows)
    binary.extend(bytes(-len(binary) % 4))
    view = len(doc["bufferViews"])
    doc["bufferViews"].append(dict(buffer=0, byteOffset=len(binary), byteLength=len(data)))
    binary.extend(data)
    acc = dict(bufferView=view, componentType=5125 if integer else 5126, count=len(rows), type=kind)
    if kind in ("SCALAR", "VEC3"):
        acc.update(min=list(map(min, zip(*rows))), max=list(map(max, zip(*rows))))
    doc["accessors"].append(acc)
    return len(doc["accessors"]) - 1


def sample(times, keys, t, rotation):
    i = bisect_right(times, t)
    if i == 0 or i == len(times):
        return keys[0] if i == 0 else keys[-1]
    alpha = (t - times[i-1]) / (times[i] - times[i-1])
    a, b = keys[i-1], keys[i]
    if rotation and sum(x*y for x,y in zip(a,b)) < 0:
        b = tuple(-v for v in b)
    result = tuple(x + (y-x)*alpha for x,y in zip(a,b))
    if rotation:
        length = math.sqrt(sum(x*x for x in result))
        return tuple(x/length for x in result)
    return result


# These three donor takes show handling or drawing the weapon. Their magazine
# joints never leave the receiver. Author a reload on the actual weighted joints.
MAGAZINES = {
    # Magazine bones, support elbow, wrist, first joints of the five fingers, grip centre.
    "AK12": ((77,78), 11,12, (17,14,21,25,29), (.115,-.115,-.036)),
    "MP7": ((183,), 34,36, (47,39,43,54,50), (.10,-.158,-.033)),
    "Scorpion": ((1081,1082), 697,699, (715,709,756,783,810), (.175,-.12,-.035)),
}


def magazine_reload(doc, tracks, name, ready):
    math3d = runpy.run_path(str(Path(__file__).with_name("arsenal_world_mesh.py")))
    nodes = [node.copy() for node in doc["nodes"]]
    for target,times,keys in tracks:
        path = target["path"]
        nodes[target["node"]][path] = sample(times,keys,ready,path=="rotation")
    parents = {child:i for i,node in enumerate(nodes) for child in node.get("children",())}
    world = {}
    def pose(i):
        if i not in world:
            local = math3d["matrix"](nodes[i])
            world[i] = math3d["multiply"](pose(parents[i]),local) if i in parents else local
        return world[i]
    def local_vector(transform, v):
        # Invert the parent's linear transform, including nonuniform donor scale.
        def cross(a,b):
            return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
        def dot(a,b):
            return sum(x*y for x,y in zip(a,b))
        a,b,c = transform[:3],transform[4:7],transform[8:11]
        rows = (cross(b,c),cross(c,a),cross(a,b))
        det = dot(a,rows[0])
        return tuple(dot(row,v)/det for row in rows)
    mag,elbow,wrist,digits,grip = MAGAZINES[name]
    indices = {n["name"]:i for i,n in enumerate(nodes)}
    def bone(number):
        return indices[f"R0_n{number}"]
    # Factors for reaching the prop and for carrying it out and back.
    factors = {bone(n):(0.,1.) for n in mag}
    factors[bone(elbow)] = (.48,.48)
    for n in [wrist,*(digit+j for digit in digits for j in range(3))]:
        factors[bone(n)] = (1.,1.)
    reach = [x-y for x,y in zip(grip,pose(bone(wrist))[12:15])]
    pull = (-.025,-.19,-.045)
    clock = [0.,.32,.51,.82,1.05,1.32,1.46,1.83]
    phases = [(0,0),(.88,0),(1,0),(1,1),(1,.94),(1,.10),(1,0),(0,0)]
    channels = {}
    for i,(hand,prop) in factors.items():
        parent = parents[i]
        ancestor = parent
        while ancestor in parents and ancestor not in factors:
            ancestor = parents[ancestor]
        h,p = factors.get(ancestor,(0.,0.))
        local_hand = local_vector(pose(parent),[v*(hand-h) for v in reach])
        local_prop = local_vector(pose(parent),[v*(prop-p) for v in pull])
        base = nodes[i].get("translation",(0.,0.,0.))
        channels[i] = [tuple(x+a*y+b*z for x,y,z in zip(base,local_hand,local_prop)) for a,b in phases]
    return clock,channels


def build_actions(doc, binary, name):
    ready, shot, reload_range, breath, step, kick = TAKES[name]
    source, = doc["animations"]
    tracks = []
    timelines = {}
    for channel in source["channels"]:
        sampler = source["samplers"][channel["sampler"]]
        idx = sampler["input"]
        if idx not in timelines:
            timelines[idx] = [t for t, in values(doc,binary,idx)]
        tracks.append((channel["target"], timelines[idx], values(doc,binary,sampler["output"])))
    reload_clock, reload_keys = magazine_reload(doc,tracks,name,ready) if name in MAGAZINES else ([],{})
    root = doc["skins"][0]["skeleton"]
    if any(target["node"] == root for target, _, _ in tracks):
        raise ValueError("Expected a separate presentation root")
    result = []
    for role, period, interval in (("Idle",breath,None), ("Walk",step,None),
                                  ("Fire",.16,shot), ("Reload",0.,reload_range)):
        clip = {"name":"Game"+role,"channels":[],"samplers":[]}
        clocks = {}
        def add(target, times, keys):
            key = tuple(times)
            if key not in clocks:
                clocks[key] = append(doc,binary,[(t,) for t in times],"SCALAR")
            path = target["path"]
            out = append(doc,binary,keys,"VEC4" if path=="rotation" else "VEC3")
            clip["channels"].append(dict(target=target, sampler=len(clip["samplers"])))
            clip["samplers"].append(dict(input=clocks[key],output=out,interpolation="LINEAR"))
        custom_reload = role == "Reload" and bool(reload_keys)
        for target,times,keys in tracks:
            if custom_reload and target["path"] == "translation" and target["node"] in reload_keys:
                continue
            quat = target["path"]=="rotation"
            hold = sample(times,keys,ready,quat)
            if interval is None or custom_reload:
                add(target,[0,reload_clock[-1] if custom_reload else period],[hold,hold])
                continue
            start,end = interval
            # Join both ends of an isolated action to the same idle grip.
            lead = .16 if role == "Reload" else .012
            tail = .16 if role == "Reload" else .035
            inner = times[bisect_right(times,start + 1e-5):bisect_left(times,end - 1e-5)]
            samples = [start,*inner,end]
            motion = [hold,*(sample(times,keys,t,quat) for t in samples),hold]
            if quat:
                for i in range(1,len(motion)):
                    if sum(x*y for x,y in zip(motion[i-1],motion[i]))<0:
                        motion[i]=tuple(-v for v in motion[i])
            clock = [0,*(lead+t-start for t in samples),lead+end-start+tail]
            add(target,clock,motion)
        if custom_reload:
            for node,keys in reload_keys.items():
                add(dict(node=node,path="translation"),reload_clock,keys)
        if interval is None:
            if role == "Fire":
                clock = [0.,.032,.074,period]
                offsets = [(0,0,0),(-kick,.008,0),(-kick*.42,.003,0),(0,0,0)]
                pitch = [0,.026,.009,0]
            else:
                clock = [period*i/16 for i in range(17)]
                cycles = [2*math.pi*i/16 for i in range(17)]
                walk = role=="Walk"
                offsets = [(0,(.009 if walk else .0016)*math.sin((2 if walk else 1)*x),
                            (.009 if walk else .0009)*math.sin(x)) for x in cycles]
                pitch = [(.013 if walk else .0015)*math.sin(x) for x in cycles]
            add(dict(node=root,path="translation"),clock,offsets)
            add(dict(node=root,path="rotation"),clock,[(0,0,math.sin(x/2),math.cos(x/2)) for x in pitch])
        result.append(clip)
    doc["animations"] = result
    # An FPS rig ends at the forearm. Its open upper-arm cap can cross the eye.
    for mesh in doc["meshes"]:
        mesh["primitives"] = [p for p in mesh["primitives"]
                              if doc["materials"][p["material"]]["name"] != "ViewmodelUpperSleeves"]


def strip_staged_geometry(name, doc, binary, world, world_binary, islands):
    # The paired bakes preserve identical triangle order and UV seams. Select
    # whole disconnected props in the ready pose, then apply that selection to both rigs.
    def gun_primitives(model):
        return [p for p in model["meshes"][0]["primitives"]
                if model["materials"][p["material"]]["name"].startswith("GunPart")]
    remove = []
    for target, source in zip(gun_primitives(doc),gun_primitives(world),strict=True):
        label = world["materials"][source["material"]]["name"]
        if not ((name=="AK12" and label=="GunPart3_ak12_001") or
                (name=="MP7" and label in ("GunPart1_slot_2","GunPart2_slot_2")) or
                (name=="Scorpion" and label=="GunPart3_CZScorpion")):
            continue
        indices = values(world,world_binary,source["indices"])
        if values(doc,binary,target["indices"]) != indices:
            raise ValueError(f"Unmatched world and held geometry in {name}/{label}")
        points = values(world,world_binary,source["attributes"]["POSITION"])
        faces = [tuple(v for (v,) in indices[i:i+3]) for i in range(0,len(indices),3)]
        keep = []
        for group in islands(points,faces):
            coords = [points[v] for i in group for v in faces[i]]
            # Donor MP7 has a tube optic with solid lens caps and a suppressor.
            # AK and EVO have a second magazine parked away from the gun.
            if (name=="MP7" and label=="GunPart1_slot_2" and min(p[1] for p in coords)>.029
                and max(p[1] for p in coords)>.045) or (
                name=="MP7" and label=="GunPart2_slot_2" and max(p[0] for p in coords)>.5) or (
                name=="Scorpion" and max(p[0] for p in coords)<-.3) or (
                name=="AK12" and max(p[2] for p in coords)<-.08):
                continue
            keep.extend(group)
        if not keep:
            remove.append(target)
        elif len(keep)<len(faces):
            target["indices"] = append(doc,binary,[(v,) for i in sorted(keep) for v in faces[i]],"SCALAR",True)
    for primitive in remove:
        doc["meshes"][0]["primitives"].remove(primitive)
