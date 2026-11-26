"""Fit the player's textured arms to the weapon's authored pose and finger tracks."""
import gzip
import json
import math
import os
from pathlib import Path
import struct


def fitted_sections(folder, name):
    fit = json.loads((folder / "fits.json").read_text())[name]
    sections = json.loads(gzip.decompress((folder / "skin.json.gz").read_bytes()))
    for section, texture, vertices, indices in sections:
        attributes = {key: [] for key in ("POSITION", "NORMAL", "TEXCOORD_0", "JOINTS_0", "WEIGHTS_0")}
        for point, normal, uv, influences in vertices:
            position = [0.] * 3
            direction = [0.] * 3
            joints, weights = [], []
            for bone, weight in influences:
                joint, affine, rotation = fit["bones"][bone]
                joints.append(joint)
                weights.append(weight)
                for k in range(3):
                    position[k] += weight * (sum(a*b for a,b in zip(affine[k], point)) + affine[k][3])
                    direction[k] += weight * sum(a*b for a,b in zip(rotation[k], normal))
            length = math.sqrt(sum(x*x for x in direction))
            attributes["POSITION"].append(position)
            attributes["NORMAL"].append([x/length for x in direction])
            attributes["TEXCOORD_0"].append(uv)
            attributes["JOINTS_0"].append(joints + [0]*(4-len(joints)))
            attributes["WEIGHTS_0"].append(weights + [0.]*(4-len(weights)))
        yield section, folder / texture, attributes, indices


def dress_gltf(folder, name, path, glb):
    model, binary = glb["read_glb"](path)
    model.setdefault("textures", [])
    model.setdefault("images", [])
    textures = {}

    def view(payload):
        binary.extend(bytes(-len(binary) % 4))
        offset = len(binary)
        binary.extend(payload)
        model["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload)})
        return len(model["bufferViews"]) - 1

    def accessor(values, component, kind):
        width = len(values[0])
        pack = struct.Struct("<" + ("H" if component == 5123 else "f") * width)
        stream = view(b"".join(pack.pack(*value) for value in values))
        model["accessors"].append({"bufferView": stream, "componentType": component, "type": kind,
                                   "count": len(values), "min": list(map(min, zip(*values))),
                                   "max": list(map(max, zip(*values)))})
        return len(model["accessors"]) - 1

    candidates = []
    for mesh in model["meshes"]:
        kept = [p for p in mesh["primitives"] if not model["materials"][p["material"]]["name"].startswith("Viewmodel")]
        if len(kept) < len(mesh["primitives"]):
            mesh["primitives"] = kept
            candidates.append(mesh)
    if len(candidates) != 1:
        raise ValueError(f"Expected one animated arms mesh in {name}")
    for section, texture, attrs, indices in fitted_sections(folder, name):
        if texture not in textures:
            model["images"].append({"bufferView": view(texture.read_bytes()), "mimeType": "image/jpeg"})
            textures[texture] = len(model["textures"])
            model["textures"].append({"source": len(model["images"])-1})
        model["materials"].append({"name": section, "doubleSided": True, "pbrMetallicRoughness": {
            "baseColorTexture": {"index": textures[texture]}, "metallicFactor": 0, "roughnessFactor": .62}})
        channels = {key: accessor(values, 5123 if key == "JOINTS_0" else 5126, f"VEC{len(values[0])}")
                    for key, values in attrs.items()}
        candidates[0]["primitives"].append({"attributes": channels, "material": len(model["materials"])-1,
                                             "indices": accessor([(i,) for i in indices], 5123, "SCALAR")})
    binary.extend(bytes(-len(binary) % 4))
    glb["write_glb"](path, model, binary)


def dress_usd(folder, name, rig):
    fit = json.loads((folder / "fits.json").read_text())[name]
    skeleton = fit["skeleton"]
    parent = skeleton.rsplit("/", 1)[0]
    root = skeleton.split("/")[1]
    # Compose one override tree, preserving the source animation, units and transform hierarchy.
    children = {}

    def subtree(path):
        node = children
        for key in path.strip("/").split("/"):
            node = node.setdefault(key, {})
        return node

    for path in fit["hide"]:
        subtree(path)[None] = "inactive"

    def array(values):
        return "[" + ",".join(str(value) if isinstance(value, int) else
                               "(" + ",".join(format(x, ".8g") for x in value) + ")" for value in values) + "]"

    for section, texture, a, indices in fitted_sections(folder, name):
        material = f"/{root}/PlayerArmMaterials/{section}"
        url = Path(os.path.relpath(texture, rig.parent)).as_posix()
        subtree(material)[None] = ("Material", f"""
            token outputs:surface.connect = <{material}/Surface.outputs:surface>
            def Shader "Surface" {{
                uniform token info:id = "UsdPreviewSurface"
                color3f inputs:diffuseColor.connect = <{material}/BaseColor.outputs:rgb>
                float inputs:roughness = 0.62
                token outputs:surface
            }}
            def Shader "BaseColor" {{
                uniform token info:id = "UsdUVTexture"
                asset inputs:file = @{url}@
                token inputs:sourceColorSpace = "sRGB"
                float2 inputs:st.connect = <{material}/UV.outputs:result>
                float3 outputs:rgb
            }}
            def Shader "UV" {{
                uniform token info:id = "UsdPrimvarReader_float2"
                token inputs:varname = "st"
                float2 outputs:result
            }}""")
        subtree(parent + "/" + section)[None] = ("Mesh", f"""
            rel material:binding = <{material}>
            rel skel:skeleton = <{skeleton}>
            matrix4d primvars:skel:geomBindTransform = ((1,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,1))
            point3f[] points = {array(a['POSITION'])}
            normal3f[] normals = {array(a['NORMAL'])} (interpolation = "vertex")
            texCoord2f[] primvars:st = {array([(u,1-v) for u,v in a['TEXCOORD_0']])} (interpolation = "vertex")
            int[] faceVertexCounts = {array([3]*(len(indices)//3))}
            int[] faceVertexIndices = {array(indices)}
            uniform token subdivisionScheme = "none"
            int[] primvars:skel:jointIndices = {[i for row in a['JOINTS_0'] for i in row]} (
                elementSize = 4
                interpolation = "vertex"
            )
            float[] primvars:skel:jointWeights = {[round(w,8) for row in a['WEIGHTS_0'] for w in row]} (
                elementSize = 4
                interpolation = "vertex"
            )""")

    def emit(nodes):
        lines = []
        for key, node in nodes.items():
            spec = node.get(None)
            if spec == "inactive":
                lines.append(f'over "{key}" (active = false) {{}}')
            elif spec:
                kind, body = spec
                apis = ' (prepend apiSchemas = ["SkelBindingAPI", "MaterialBindingAPI"])' if kind == "Mesh" else ""
                lines.append(f'def {kind} "{key}"{apis} {{\n{body}\n}}')
            else:
                specifier = "def Scope" if key == "PlayerArmMaterials" else "over"
                lines.append(f'{specifier} "{key}" {{\n{emit(node)}\n}}')
        return "\n".join(lines)

    # Root layer metadata (in particular timeCodesPerSecond) is not inherited from sublayers.
    header = rig.read_text(encoding="utf-8").split("\n)", 1)[0] + "\n)"
    start = header.index("subLayers = [")
    end = header.index("]", start)
    header = header[:start] + f"subLayers = [@{rig.name}@]" + header[end+1:]
    result = rig.with_name(name + "PlayerArms.usda")
    result.write_text(header + "\n" + emit(children) + "\n", encoding="utf-8")
    return result
