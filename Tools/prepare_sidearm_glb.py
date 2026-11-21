"""Bind the bundled pistol mesh to Cransh's animated grip joint."""

import json
import struct


CLIPS = {
    "Armature|FPS_Pistol_Idle": "ViewmodelIdle",
    "Armature|FPS_Pistol_Walk": "ViewmodelWalk",
    "Armature|FPS_Pistol_Fire": "ViewmodelFire",
    "Armature|FPS_Pistol_Reload_easy": "ViewmodelReload",
    "Armature|FPS_Pistol_Reload_full": "ViewmodelReloadEmpty",
}


def read_glb(path):
    payload = path.read_bytes()
    magic, version, size, json_size, json_type = struct.unpack_from("<4sIII4s", payload)
    if (magic, version, size, json_type) != (b"glTF", 2, len(payload), b"JSON"):
        raise ValueError(f"Invalid GLB: {path.name}")
    model = json.loads(payload[20:20 + json_size])
    binary_size, binary_type = struct.unpack_from("<I4s", payload, 20 + json_size)
    if binary_type != b"BIN\0" or binary_size != len(payload) - (28 + json_size):
        raise ValueError(f"Missing binary data in {path.name}")
    return model, bytearray(payload[28 + json_size:])


def add_front_sight(model, binary, parent, bind_point):
    skin = model["skins"][0]
    grip_index = skin["joints"].index(parent)
    accessor = model["accessors"][skin["inverseBindMatrices"]]
    view = model["bufferViews"][accessor["bufferView"]]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", 64)
    grip_inverse = list(struct.unpack_from("<16f", binary, start + grip_index * stride))
    local_point = [sum(grip_inverse[axis + column * 4] * bind_point[column] for column in range(3))
                   + grip_inverse[axis + 12] for axis in range(3)]

    nodes = model["nodes"]
    nodes[parent]["children"].append(len(nodes))
    skin["joints"].append(len(nodes))
    nodes.append({"name": "FrontSight", "translation": local_point})
    for axis in range(3):
        grip_inverse[axis + 12] -= local_point[axis]
    combined = bytearray()
    for index in range(accessor["count"]):
        offset = start + index * stride
        combined.extend(binary[offset:offset + 64])
    combined.extend(struct.pack("<16f", *grip_inverse))
    model["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(combined)})
    binary.extend(combined)
    skin["inverseBindMatrices"] = len(model["accessors"])
    model["accessors"].append({
        "bufferView": len(model["bufferViews"]) - 1, "componentType": 5126,
        "count": len(skin["joints"]), "type": "MAT4",
    })


def mount_pistol(model, binary, arms, frame, pistol_source):
    pistol, geometry = read_glb(pistol_source)
    primitives = pistol["meshes"][0]["primitives"]
    positions = [pistol["accessors"][part["attributes"]["POSITION"]] for part in primitives]
    original = model["accessors"][frame["attributes"]["POSITION"]]
    front = max(item["max"][0] for item in positions)
    rear = min(item["min"][0] for item in positions)
    top = max(item["max"][1] for item in positions)
    scale = (original["max"][2] - original["min"][2]) / (front - rear)

    offsets = {key: len(model[key]) for key in
               ("bufferViews", "accessors", "images", "samplers", "textures", "materials")}
    geometry_offset = len(binary)
    binary.extend(geometry)
    for view in pistol["bufferViews"]:
        view["byteOffset"] = view.get("byteOffset", 0) + geometry_offset
    for accessor in pistol["accessors"]:
        accessor["bufferView"] += offsets["bufferViews"]
    for image in pistol["images"]:
        image["bufferView"] += offsets["bufferViews"]
    for texture in pistol["textures"]:
        texture["source"] += offsets["images"]
        texture["sampler"] += offsets["samplers"]
    for material in pistol["materials"]:
        locations = (material, material["pbrMetallicRoughness"])
        for location in locations:
            for key, info in location.items():
                if key.endswith("Texture"):
                    info["index"] += offsets["textures"]
    for key in offsets:
        model[key].extend(pistol[key])

    joint = next(i for i, node in enumerate(model["nodes"]) if node.get("name") == "PBody_058")
    # The pistol is rigidly bound to a child of the animated grip. Its origin marks the front sight.
    front_sight_joint = len(model["skins"][0]["joints"])
    sight = (0.0, float("-inf"), original["max"][2])

    def add_vertex_stream(data, count, component_type):
        view = len(model["bufferViews"])
        model["bufferViews"].append({
            "buffer": 0, "byteOffset": len(binary), "byteLength": len(data), "target": 34962,
        })
        binary.extend(data)
        index = len(model["accessors"])
        model["accessors"].append({
            "bufferView": view, "componentType": component_type, "count": count, "type": "VEC4",
        })
        return index

    for part in primitives:
        attributes = part["attributes"]
        for semantic, accessor_index in attributes.items():
            if semantic not in ("POSITION", "NORMAL", "TANGENT"):
                continue
            accessor = pistol["accessors"][accessor_index]
            view = model["bufferViews"][accessor["bufferView"]]
            vector = struct.Struct("<ffff" if semantic == "TANGENT" else "<fff")
            start = view["byteOffset"] + accessor.get("byteOffset", 0)
            stride = view.get("byteStride", vector.size)
            transformed = []
            for i in range(accessor["count"]):
                offset = start + i * stride
                x, y, z, *handedness = vector.unpack_from(binary, offset)
                value = (-z, y, x)
                if semantic == "POSITION":
                    value = (-z * scale, original["max"][1] + (y - top) * scale,
                             original["max"][2] + (x - front) * scale)
                    if value[2] >= original["max"][2] - 0.04 and abs(value[0]) < 0.01:
                        if (value[1], value[2]) > (sight[1], sight[2]):
                            sight = (0.0, value[1], value[2])
                vector.pack_into(binary, offset, *value, *handedness)
                transformed.append((*value, *handedness))
            accessor["min"] = [min(column) for column in zip(*transformed)]
            accessor["max"] = [max(column) for column in zip(*transformed)]

        count = pistol["accessors"][attributes["POSITION"]]["count"]
        skin_joints = add_vertex_stream(struct.pack("<4H", front_sight_joint, 0, 0, 0) * count, count, 5123)
        skin_weights = add_vertex_stream(struct.pack("<4f", 1, 0, 0, 0) * count, count, 5126)
        arms["primitives"].append({
            "attributes": {**{key: value + offsets["accessors"] for key, value in attributes.items()},
                           "JOINTS_0": skin_joints, "WEIGHTS_0": skin_weights},
            "indices": part["indices"] + offsets["accessors"],
            "material": part["material"] + offsets["materials"],
            "mode": part["mode"],
        })

    if sight[1] == float("-inf"):
        raise ValueError("Could not locate vertices near the pistol front sight.")
    add_front_sight(model, binary, joint, sight)


def prepare_sidearm_glb(source, destination, pistol_source):
    model, binary = read_glb(source)
    nodes = model["nodes"]
    armature_index = next(i for i, node in enumerate(nodes) if node.get("name") == "Armature")
    armature = nodes[armature_index]
    if armature.get("scale") != [100.0] * 3 or abs(nodes[1]["matrix"][0] - 0.01) > 1e-8:
        raise ValueError("Unexpected units in the source pistol rig.")
    # The Sketchfab wrapper and armature scales cancel. Import the rig directly in meters.
    armature.pop("scale")
    model["scenes"] = [{"nodes": [armature_index]}]
    model["scene"] = 0
    arms_index = next(i for i, mesh in enumerate(model["meshes"]) if mesh["name"].startswith("arms_"))
    grip_index = next(i for i, mesh in enumerate(model["meshes"]) if mesh["name"] == "xd_frame_Material_0")
    arms = model["meshes"][arms_index]
    arms["name"] = "SK_SidearmViewmodel"
    frame = model["meshes"][grip_index]["primitives"][0]
    for node in nodes:
        if node.get("mesh") == arms_index:
            node.update(mesh=0, name="SK_SidearmViewmodel")
        elif "mesh" in node:
            node.pop("mesh")
            node.pop("skin", None)
    model["meshes"] = [arms]
    material = model["materials"][0]
    legacy = material.pop("extensions")["KHR_materials_pbrSpecularGlossiness"]
    material["pbrMetallicRoughness"] = {
        "baseColorTexture": legacy["diffuseTexture"],
        "baseColorFactor": legacy["diffuseFactor"],
        "metallicFactor": 0.0,
        "roughnessFactor": 0.63,
    }
    model["materials"] = [material]
    mount_pistol(model, binary, arms, frame, pistol_source)
    model.pop("extensionsUsed", None)
    model.pop("extensionsRequired", None)
    model["buffers"][0]["byteLength"] = len(binary)
    for animation in model["animations"]:
        animation["name"] = CLIPS[animation["name"]]

    metadata = json.dumps(model, separators=(",", ":")).encode()
    metadata += b" " * (-len(metadata) % 4)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(
        struct.pack("<4sIII4s", b"glTF", 2, 28 + len(metadata) + len(binary), len(metadata), b"JSON")
        + metadata + struct.pack("<I4s", len(binary), b"BIN\0") + binary
    )
