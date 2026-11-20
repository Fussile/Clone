"""Normalize the supplied zombie GLB to a single skinned mesh for Interchange."""

import json
import struct

CLIPS = {"Idle": "StationZombieIdle", "Walk1": "StationZombieWalk", "Attack1.001": "StationZombieAttack"}


def prepare_zombie_glb(source, destination):
    payload = source.read_bytes()
    magic, version, size, json_size, json_type = struct.unpack_from("<4sIII4s", payload)
    if (magic, version, size, json_type) != (b"glTF", 2, len(payload), b"JSON"):
        raise ValueError(f"Invalid GLB: {source.name}")
    model = json.loads(payload[20:20 + json_size])
    binary = payload[20 + json_size:]
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
