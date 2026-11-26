"""Combine authored glTF mesh nodes into one textured, rigid inspection mesh."""
import math
import struct


def multiply(a, b):
    return [sum(a[k * 4 + row] * b[col * 4 + k] for k in range(4))
            for col in range(4) for row in range(4)]


def matrix(node):
    if "matrix" in node:
        return node["matrix"]
    x, y, z, w = node.get("rotation", (0, 0, 0, 1))
    sx, sy, sz = node.get("scale", (1, 1, 1))
    return [
        sx*(1-2*y*y-2*z*z), sx*(2*x*y+2*w*z), sx*(2*x*z-2*w*y), 0,
        sy*(2*x*y-2*w*z), sy*(1-2*x*x-2*z*z), sy*(2*y*z+2*w*x), 0,
        sz*(2*x*z+2*w*y), sz*(2*y*z-2*w*x), sz*(1-2*x*x-2*y*y), 0,
        *node.get("translation", (0, 0, 0)), 1,
    ]


def flatten(glb, source, destination):
    model, binary = glb["read_glb"](source)
    primitives = []

    def visit(index, parent):
        node = model["nodes"][index]
        transform = multiply(parent, matrix(node))
        if "mesh" in node:
            for original in model["meshes"][node["mesh"]]["primitives"]:
                attrs = {k: v for k, v in original["attributes"].items()
                         if not k.startswith(("JOINTS", "WEIGHTS")) and k != "TANGENT"}
                for key in ("POSITION", "NORMAL"):
                    if key not in attrs:
                        continue
                    accessor = model["accessors"][attrs[key]]
                    view = model["bufferViews"][accessor["bufferView"]]
                    assert accessor["componentType"] == 5126 and accessor["type"] == "VEC3"
                    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
                    points = []
                    for i in range(accessor["count"]):
                        v = struct.unpack_from("<3f", binary, start + i * view.get("byteStride", 12))
                        # Imported prop nodes have orthogonal rotation/scale axes.
                        if key == "NORMAL":
                            v = [v[c] / max(1e-20, sum(transform[4*c+r]**2 for r in range(3))) for c in range(3)]
                        point = [sum(transform[4*c+r]*v[c] for c in range(3)) for r in range(3)]
                        if key == "POSITION":
                            point = [point[r] + transform[12+r] for r in range(3)]
                        else:
                            length = max(1e-20, math.sqrt(sum(x*x for x in point)))
                            point = [x/length for x in point]
                        points.append(point)
                    binary.extend(bytes(-len(binary) % 4))
                    offset = len(binary)
                    for point in points:
                        binary.extend(struct.pack("<3f", *point))
                    model["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(points)*12})
                    attrs[key] = len(model["accessors"])
                    model["accessors"].append({"componentType": 5126, "type": "VEC3", "count": len(points),
                        "bufferView": len(model["bufferViews"])-1, "min": list(map(min, zip(*points))),
                        "max": list(map(max, zip(*points)))})
                primitives.append({**original, "attributes": attrs})
        for child in node.get("children", []):
            visit(child, transform)

    for root in model["scenes"][model.get("scene", 0)]["nodes"]:
        visit(root, matrix({}))
    model["nodes"] = [{"mesh": 0, "name": "World"}]
    model["scenes"] = [{"nodes": [0]}]
    model["scene"] = 0
    model["meshes"] = [{"name": "World", "primitives": primitives}]
    model.pop("animations", None)
    model.pop("skins", None)
    glb["write_glb"](destination, model, binary)


def remove_factory_scope(tools, source, destination):
    import runpy
    from collections import Counter

    helpers = runpy.run_path(str(tools / "prepare_viewmodel_gear.py"))
    glb = helpers["GLB"]
    read = helpers["read_accessor"]
    model, binary = glb["read_glb"](source / "SK_SniperViewmodel.glb")
    scope = next(i for i, node in enumerate(model["skins"][0]["joints"])
                 if model["nodes"][node]["name"] == "Sniper_scope")
    signatures = Counter()

    def faces(model, binary, primitive):
        uv = read(model, binary, primitive["attributes"]["TEXCOORD_0"], 2)
        indices = [v[0] for v in read(model, binary, primitive["indices"], 1)]
        for start in range(0, len(indices), 3):
            triangle = indices[start:start + 3]
            yield tuple(sorted(uv[v] for v in triangle)), triangle

    for primitive in model["meshes"][0]["primitives"]:
        attrs = primitive["attributes"]
        joints = read(model, binary, attrs["JOINTS_0"], 4)
        weights = read(model, binary, attrs["WEIGHTS_0"], 4)
        for signature, triangle in faces(model, binary, primitive):
            if all(joints[v][weights[v].index(max(weights[v]))] == scope for v in triangle):
                signatures[signature] += 1
    if not signatures:
        raise ValueError("Could not identify the factory optic.")
    world, data = glb["read_glb"](source / "SM_CrossroadsSniper.glb")
    for primitive in world["meshes"][0]["primitives"]:
        indices = []
        for signature, triangle in faces(world, data, primitive):
            if signatures[signature]:
                signatures[signature] -= 1
            else:
                indices.extend(triangle)
        component_type = world["accessors"][primitive["indices"]]["componentType"]
        primitive["indices"] = helpers["add_index_accessor"](world, data, indices, component_type)
    if any(signatures.values()):
        raise ValueError("Factory optic differs between the world and first-person models.")
    data.extend(bytes(-len(data) % 4))
    glb["write_glb"](destination, world, data)
