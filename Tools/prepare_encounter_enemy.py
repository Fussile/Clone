"""Prepare the bundled single-rig encounter models for Unreal Interchange."""
import json
import struct


def prepare_encounter_enemy(source, destination, role):
    payload = source.read_bytes()
    magic, version, size, length, chunk = struct.unpack_from("<4sIII4s", payload)
    if (magic, version, size, chunk) != (b"glTF", 2, len(payload), b"JSON"):
        raise ValueError(f"Expected a GLB 2.0 file in {source}")
    model = json.loads(payload[20:20+length])
    nodes = [node for node in model["nodes"] if "mesh" in node]
    if len(model["skins"]) != 1 or not nodes or any(node.get("skin") != 0 for node in nodes):
        raise ValueError(f"Expected a shared skeleton in {source}")
    primitives = []
    for node in nodes:
        for primitive in model["meshes"][node.pop("mesh")]["primitives"]:
            # The toxic source repeats UV0 on ten channels.
            for key in list(primitive["attributes"]):
                if key.startswith("TEXCOORD_") and int(key[9:]) >= 8:
                    del primitive["attributes"][key]
            primitives.append(primitive)
        node.pop("skin")
    nodes[0].update(mesh=0, skin=0, name="SK_"+role)
    model["meshes"] = [{"name": "SK_"+role, "primitives": primitives}]

    clip, = model["animations"]
    clip["name"] = role+"Loop"
    # The hound also animates helper nodes outside its joint hierarchy.
    parents = {child: i for i, node in enumerate(model["nodes"]) for child in node.get("children", [])}
    skeleton = set(model["skins"][0]["joints"])
    for joint in tuple(skeleton):
        while joint in parents:
            joint = parents[joint]
            skeleton.add(joint)
    clip["channels"] = [channel for channel in clip["channels"] if channel["target"]["node"] in skeleton]

    header = json.dumps(model, separators=(",", ":")).encode()
    header += b" " * (-len(header) % 4)
    binary = payload[20+length:]
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(struct.pack("<4sIII4s", b"glTF", 2, 20+len(header)+len(binary),
                                        len(header), b"JSON") + header + binary)
