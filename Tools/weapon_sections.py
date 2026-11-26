"""Preserve the donor's UVs and animation weights while exposing removable mesh sections."""
from collections import defaultdict
from pathlib import Path
import runpy

helpers = runpy.run_path(str(Path(__file__).with_name("prepare_viewmodel_gear.py")))
read = helpers["read_accessor"]
glb = helpers["GLB"]


def islands(points, faces):
    # Weld UV seams for grouping only. The output retains all original vertices.
    parents = list(range(len(points)))
    seams = {}

    def root(index):
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = parents[index]
        return index

    for i, point in enumerate(points):
        parents[i] = seams.setdefault(tuple(round(float(x), 6) for x in point), i)
    for face in faces:
        first = root(face[0])
        for index in face[1:]:
            parents[root(index)] = first
    groups = defaultdict(list)
    for i, face in enumerate(faces):
        groups[root(face[0])].append(i)
    return list(groups.values())


def part_numbers(points, faces, forward, up, pistol, fixed=None):
    if fixed is not None:
        return [fixed] * len(faces)
    groups = islands(points, faces)
    # Use the position of each complete disconnected piece along this donor's authored axes.
    # A fused body stays with its receiver. Never cut a polygon or an island across a boundary.
    along = [p[abs(forward)-1] * (1 if forward > 0 else -1) for p in points]
    height = [p[abs(up)-1] * (1 if up > 0 else -1) for p in points]
    low, span = min(along), max(along) - min(along)
    bottom, tall = min(height), max(height) - min(height)
    roles = [1] * len(faces)
    for group in groups:
        vertices = {v for face in group for v in faces[face]}
        a, b = min(along[v] for v in vertices), max(along[v] for v in vertices)
        c, d = min(height[v] for v in vertices), max(height[v] for v in vertices)
        x = ((a+b)*.5-low) / max(span, 1e-9)
        z = ((c+d)*.5-bottom) / max(tall, 1e-9)
        if pistol:
            slot = 0 if z < .43 and x < .52 else 2 if x > .58 and z > .45 else 3 if z > .72 else 1
        else:
            slot = 0 if x < .28 else 2 if x > .65 else 3 if z < .37 else 1
        for face in group:
            roles[face] = slot
    return roles


def tag(model, binary, primitive, roles):
    faces = triangles(model, binary, primitive)
    if len(roles) != len(faces):
        raise ValueError("Part layout does not match source triangles")
    sections = defaultdict(list)
    for face, role in zip(faces, roles):
        sections[role].extend(face)
    result = []
    for role, indices in sections.items():
        material = primitive["material"]
        if role is not None:
            source = model["materials"][material]
            name = f"GunPart{role}_{source['name']}"
            material = next((i for i, entry in enumerate(model["materials"]) if entry.get("name") == name), None)
            if material is None:
                material = len(model["materials"])
                model["materials"].append({**source, "name": name})
        component = model["accessors"][primitive["indices"]]["componentType"]
        result.append({**primitive, "indices": helpers["add_index_accessor"](model, binary, indices, component),
                       "material": material})
    return result


def triangles(model, binary, primitive):
    indices = [v for v, in read(model, binary, primitive["indices"], 1)]
    return [indices[i:i+3] for i in range(0, len(indices), 3)]


def split_props(model, binary, name, view=False):
    for mesh in model["meshes"]:
        label = mesh.get("name", "").lower()
        if view and ("arms" in label or "mesh_mat" in label):
            continue
        sections = []
        for index, primitive in enumerate(mesh["primitives"]):
            points = read(model, binary, primitive["attributes"]["POSITION"], 3)
            faces = triangles(model, binary, primitive)
            fixed = None
            if name == "Sidearm":
                # The lower casting includes the magazine well. The slide mesh also includes two grip panels.
                groups = islands(points, faces)
                mapping = ({0: 1, 1: 0} if index == 0 else
                           {0: 3, 1: 3, 2: 3, 3: 3, 4: 3, 5: 3, 8: 0, 9: 0, 10: 0, 11: 2, 14: 2})
                roles = [1] * len(faces)
                for island, group in enumerate(groups):
                    for face in group:
                        roles[face] = mapping.get(island, 1)
            else:
                axis, up = (1, 2) if name == "Sniper" else (3, 2)
                if name == "Python357":
                    axis = 3
                    if "bullet" in label or "shell" in label: fixed = 3
                if name == "ScrapRifle" and not view:
                    axis, up = -2, 3
                    if label.startswith("mag_"): fixed = 3
                    elif label.startswith("grip_"): fixed = 0
                    elif not label.startswith("rifle_"): fixed = 1
                roles = part_numbers(points, faces, axis, up, name in ("Python357", "Makarov"), fixed)
            if name == "Python357" and label.startswith("revolver_"):
                # The world prop has separate grip panels, cylinder and barrel shroud.
                for island, group in enumerate(islands(points, faces)):
                    role = {1: 3, 3: 0, 5: 2, 6: 2, 9: 2, 10: 2}.get(island, 1)
                    for face in group:
                        roles[face] = role
            sections.extend(tag(model, binary, primitive, roles))
        mesh["primitives"] = sections
    binary.extend(bytes(-len(binary) % 4))


def base_models(name, prop, dressed, destination):
    world, data = glb["read_glb"](prop)
    split_props(world, data, name)
    # The dressed rigs carry the same UVs after conversion to the animated coordinate system.
    by_uv = {}
    def signatures(model, binary, primitive):
        uv = read(model, binary, primitive["attributes"]["TEXCOORD_0"], 2)
        return [tuple(sorted(uv[v] for v in face)) for face in triangles(model, binary, primitive)]

    if name == "Sniper":
        rig, rig_data = glb["read_glb"](dressed)
        joints = rig["skins"][0]["joints"]
        scope = next(i for i, node in enumerate(joints) if rig["nodes"][node]["name"] == "Sniper_scope")
        scope_faces = set()
        for prim in rig["meshes"][0]["primitives"]:
            attrs = prim["attributes"]
            bones = read(rig, rig_data, attrs["JOINTS_0"], 4)
            weights = read(rig, rig_data, attrs["WEIGHTS_0"], 4)
            for key, face in zip(signatures(rig, rig_data, prim), triangles(rig, rig_data, prim)):
                if all(bones[v][weights[v].index(max(weights[v]))] == scope for v in face):
                    scope_faces.add(key)
        if not scope_faces:
            raise ValueError("Factory scope has no weighted triangles")
        for mesh in world["meshes"]:
            sections = []
            for prim in mesh["primitives"]:
                roles = [4 if key in scope_faces else None for key in signatures(world, data, prim)]
                sections.extend(tag(world, data, prim, roles))
            mesh["primitives"] = sections
    data.extend(bytes(-len(data) % 4))
    glb["write_glb"](destination / "World.glb", world, data)
    for primitive in world["meshes"][0]["primitives"]:
        role = int(world["materials"][primitive["material"]]["name"][7])
        for key in signatures(world, data, primitive):
            by_uv[key] = role
    model, data = glb["read_glb"](dressed)
    for mesh in model["meshes"]:
        sections = []
        for primitive in mesh["primitives"]:
            material = model["materials"][primitive["material"]]["name"]
            if "Viewmodel" in material:
                sections.append(primitive)
                continue
            roles = [by_uv.get(key) for key in signatures(model, data, primitive)]
            sections.extend(tag(model, data, primitive, roles))
        mesh["primitives"] = sections
    data.extend(bytes(-len(data) % 4))
    glb["write_glb"](destination / dressed.name, model, data)
