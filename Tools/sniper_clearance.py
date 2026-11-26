"""Bind conservative scope and bolt bounds to the original animated parts."""

import itertools


def add_clearance_markers(path, glb, read):
    model, binary = glb["read_glb"](path)
    skin = model["skins"][0]
    mesh = model["meshes"][
        next(node["mesh"] for node in model["nodes"] if node.get("skin") == 0 and "mesh" in node)
    ]
    for part, name in enumerate(("Sniper_scope", "Sniper_bolt", "Sniper_boltback")):
        node = next(i for i, joint in enumerate(model["nodes"]) if joint.get("name") == name)
        joint = skin["joints"].index(node)
        vertices = []
        for primitive in mesh["primitives"]:
            attrs = primitive["attributes"]
            bones = read(model, binary, attrs["JOINTS_0"], 4)
            weights = read(model, binary, attrs["WEIGHTS_0"], 4)
            points = read(model, binary, attrs["POSITION"], 3)
            ids = {v for v, in read(model, binary, primitive["indices"], 1)}
            vertices.extend(points[v] for v in ids if bones[v][weights[v].index(max(weights[v]))] == joint)
        if not vertices:
            raise ValueError(f"Missing skinned geometry for {name}")
        bounds = [(min(c), max(c)) for c in zip(*vertices)]
        for corner, point in enumerate(itertools.product(*bounds)):
            glb["add_sight"](model, binary, node, f"ActionClearance{part * 8 + corner}", point)
    glb["write_glb"](path, model, binary)
