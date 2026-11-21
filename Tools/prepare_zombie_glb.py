"""Normalize the supplied zombie GLB to a single skinned mesh for Interchange."""

import json
import struct

CLIPS = {"Idle": "StationZombieIdle", "Walk1": "StationZombieWalk", "Attack1.001": "StationZombieAttack"}
CENTIMETERS_TO_METERS = 0.01


def bake_source_units(model, binary):
    # The Sketchfab wrapper scales the whole centimeter-authored rig by 0.01.
    # Convert each length explicitly so the skeletal importer can omit that wrapper.
    root = model["nodes"][1]["matrix"]
    if (abs(root[0] - CENTIMETERS_TO_METERS) > 1e-8
            or abs(root[6] - CENTIMETERS_TO_METERS) > 1e-8
            or abs(root[9] + CENTIMETERS_TO_METERS) > 1e-8):
        raise ValueError("Unexpected unit transform in the station zombie source.")
    for index in range(12):
        root[index] /= CENTIMETERS_TO_METERS
    for node in model["nodes"]:
        if "translation" in node:
            node["translation"] = [v * CENTIMETERS_TO_METERS for v in node["translation"]]
        if "matrix" in node:
            for index in (12, 13, 14):
                node["matrix"][index] *= CENTIMETERS_TO_METERS

    positions = {primitive["attributes"]["POSITION"]
                 for mesh in model["meshes"] for primitive in mesh["primitives"]}
    translations = {animation["samplers"][channel["sampler"]]["output"]
                    for animation in model["animations"] for channel in animation["channels"]
                    if channel["target"]["path"] == "translation"}
    bind = {skin["inverseBindMatrices"] for skin in model["skins"]}
    for index in positions | translations | bind:
        accessor = model["accessors"][index]
        view = model["bufferViews"][accessor["bufferView"]]
        if accessor["componentType"] != 5126 or "sparse" in accessor:
            raise ValueError("Expected dense float accessors in the zombie rig.")
        component_count, columns = (16, (12, 13, 14)) if index in bind else (3, (0, 1, 2))
        offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        stride = view.get("byteStride", component_count * 4)
        for row in range(accessor["count"]):
            for column in columns:
                pos = offset + row * stride + column * 4
                value, = struct.unpack_from("<f", binary, pos)
                struct.pack_into("<f", binary, pos, value * CENTIMETERS_TO_METERS)
        for bound in ("min", "max"):
            if bound in accessor:
                for column in columns:
                    accessor[bound][column] *= CENTIMETERS_TO_METERS


def prepare_zombie_glb(source, destination):
    payload = source.read_bytes()
    magic, version, size, json_size, json_type = struct.unpack_from("<4sIII4s", payload)
    if (magic, version, size, json_type) != (b"glTF", 2, len(payload), b"JSON"):
        raise ValueError(f"Invalid GLB: {source.name}")
    model = json.loads(payload[20:20 + json_size])
    binary = bytearray(payload[20 + json_size:])
    if struct.unpack_from("<4s", binary, 4)[0] != b"BIN\0":
        raise ValueError("Expected embedded binary data in the zombie GLB.")
    binary_data = bytearray(binary[8:])
    bake_source_units(model, binary_data)
    binary[8:] = binary_data
    nodes = model["nodes"]
    skinned = [index for index, node in enumerate(nodes) if "mesh" in node and "skin" in node]
    if not skinned or len({nodes[index]["skin"] for index in skinned}) != 1:
        raise ValueError("Expected the station zombie parts to share one skin.")

    parents = {child: index for index, node in enumerate(nodes) for child in node.get("children", [])}
    shared_parent = parents[skinned[0]]
    for index in skinned:
        if (parents.get(index) != shared_parent
                or set(nodes[index]) - {"name", "mesh", "skin"}):
            raise ValueError("Expected skinned parts with identical transforms under one parent.")

    # Preserve node indices, joint bindings, textures and all geometry buffers.
    meshes = model["meshes"]
    keep = skinned[0]
    merged = {"name": "SK_StationZombie", "primitives": []}
    for index in skinned:
        part = meshes[nodes[index]["mesh"]]
        if any(part.get("weights", [])):
            raise ValueError("The station zombie has nonzero default morph weights.")
        for primitive in part["primitives"]:
            # Parts have different facial morph sets. Import the base shape and bone animation.
            merged["primitives"].append({key: value for key, value in primitive.items() if key != "targets"})
        if index != keep:
            nodes[index].pop("mesh")
            nodes[index].pop("skin")
    nodes[keep].update(mesh=0, name="SK_StationZombie")
    model["meshes"] = [merged]
    for animation in model["animations"]:
        animation["name"] = CLIPS[animation["name"]]
        animation["channels"] = [channel for channel in animation["channels"] if channel["target"]["path"] != "weights"]

    metadata = json.dumps(model, separators=(",", ":")).encode()
    metadata += b" " * (-len(metadata) % 4)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(
        struct.pack("<4sIII4s", b"glTF", 2, 20 + len(metadata) + len(binary), len(metadata), b"JSON")
        + metadata + binary
    )
