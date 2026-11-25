"""Give weapon viewmodel arms separate sleeve and glove material slots."""

import re
import runpy
import struct
from pathlib import Path


GLB = runpy.run_path(str(Path(__file__).with_name("prepare_sidearm_glb.py")))
COMPONENT_FORMATS = {5121: "B", 5123: "H", 5125: "I", 5126: "f"}
HAND_ROOT = re.compile(r"^(Hand_[LR]|[LR]_wrist)_", re.IGNORECASE)


def read_accessor(model, binary, index, width):
    accessor = model["accessors"][index]
    view = model["bufferViews"][accessor["bufferView"]]
    layout = struct.Struct("<" + COMPONENT_FORMATS[accessor["componentType"]] * width)
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", layout.size)
    return [layout.unpack_from(binary, offset + i * stride) for i in range(accessor["count"])]


def find_hand_joints(model, node):
    nodes = model["nodes"]
    skin_joints = model["skins"][node["skin"]]["joints"]
    pending = [index for index in skin_joints if HAND_ROOT.match(nodes[index].get("name", ""))]
    if len(pending) != 2:
        raise ValueError("Expected left and right hand joints in weapon viewmodel.")
    hand_nodes = set()
    while pending:
        index = pending.pop()
        hand_nodes.add(index)
        pending.extend(nodes[index].get("children", []))
    return {i for i, index in enumerate(skin_joints) if index in hand_nodes}


def add_index_accessor(model, binary, indices, component_type):
    binary.extend(b"\0" * (-len(binary) % 4))
    offset = len(binary)
    binary.extend(struct.pack("<" + COMPONENT_FORMATS[component_type] * len(indices), *indices))
    model["bufferViews"].append({
        "buffer": 0, "byteOffset": offset, "byteLength": len(binary) - offset, "target": 34963,
    })
    model["accessors"].append({
        "bufferView": len(model["bufferViews"]) - 1, "componentType": component_type,
        "count": len(indices), "type": "SCALAR", "min": [min(indices)], "max": [max(indices)],
    })
    return len(model["accessors"]) - 1


def split_arms(model, binary):
    node = next(node for node in model["nodes"] if "mesh" in node and "skin" in node)
    hand_joints = find_hand_joints(model, node)
    primitives = model["meshes"][node["mesh"]]["primitives"]
    arm = next(p for p in primitives if model["materials"][p["material"]]["name"] == "arms")
    attrs = arm["attributes"]
    joints = read_accessor(model, binary, attrs["JOINTS_0"], 4)
    weights = read_accessor(model, binary, attrs["WEIGHTS_0"], 4)
    hand_weight = [
        sum(w for j, w in zip(vertex_joints, vertex_weights) if j in hand_joints) / sum(vertex_weights)
        for vertex_joints, vertex_weights in zip(joints, weights)
    ]
    indices = read_accessor(model, binary, arm["indices"], 1)
    if arm.get("mode", 4) != 4 or len(indices) % 3:
        raise ValueError("Expected triangles in weapon viewmodel arms.")
    sleeves, gloves = [], []
    for i in range(0, len(indices), 3):
        triangle = [index for index, in indices[i:i + 3]]
        section = gloves if sum(hand_weight[index] for index in triangle) >= 1.5 else sleeves
        section.extend(triangle)
    if not sleeves or not gloves:
        raise ValueError("Expected triangles in both arm coverage sections.")

    # Keep the original material for uncovered gear slots, with names the game can find.
    component_type = model["accessors"][arm["indices"]]["componentType"]
    for name, section in (("ViewmodelSleeves", sleeves), ("ViewmodelGloves", gloves)):
        material = {**model["materials"][arm["material"]], "name": name}
        model["materials"].append(material)
        primitives.append({
            **arm, "indices": add_index_accessor(model, binary, section, component_type),
            "material": len(model["materials"]) - 1,
        })
    primitives.remove(arm)


def prepare_viewmodel_gear(source, destination, *pistol_sources):
    if pistol_sources:
        GLB["prepare_sidearm_glb"](source, destination, *pistol_sources)
        source = destination
    model, binary = GLB["read_glb"](source)
    split_arms(model, binary)
    binary.extend(b"\0" * (-len(binary) % 4))
    GLB["write_glb"](destination, model, binary)
