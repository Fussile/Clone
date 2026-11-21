"""Keep Cransh's animated arms and mount the game's own pistol at runtime."""

import json
import struct


CLIPS = {
    "Armature|FPS_Pistol_Idle": "ViewmodelIdle",
    "Armature|FPS_Pistol_Walk": "ViewmodelWalk",
    "Armature|FPS_Pistol_Fire": "ViewmodelFire",
    "Armature|FPS_Pistol_Reload_easy": "ViewmodelReload",
    "Armature|FPS_Pistol_Reload_full": "ViewmodelReloadEmpty",
}


def prepare_sidearm_glb(source, destination):
    payload = source.read_bytes()
    magic, version, size, json_size, json_type = struct.unpack_from("<4sIII4s", payload)
    if (magic, version, size, json_type) != (b"glTF", 2, len(payload), b"JSON"):
        raise ValueError(f"Invalid GLB: {source.name}")
    model = json.loads(payload[20:20 + json_size])
    binary = payload[20 + json_size:]
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
    # Retain the binding geometry so Unreal evaluates the gun bone with the arm pose.
    arms["primitives"].extend(model["meshes"][grip_index]["primitives"])
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
    model["materials"] = [material, {
        "name": "GripBinding",
        "alphaMode": "MASK",
        "pbrMetallicRoughness": {"baseColorFactor": [0, 0, 0, 0], "metallicFactor": 0, "roughnessFactor": 1},
    }]
    model.pop("extensionsUsed", None)
    for animation in model["animations"]:
        animation["name"] = CLIPS[animation["name"]]

    metadata = json.dumps(model, separators=(",", ":")).encode()
    metadata += b" " * (-len(metadata) % 4)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(
        struct.pack("<4sIII4s", b"glTF", 2, 20 + len(metadata) + len(binary), len(metadata), b"JSON")
        + metadata + binary
    )
