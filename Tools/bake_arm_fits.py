"""Bake survivor-to-firearm bind-pose fits offline (NumPy and OpenUSD)."""
import argparse
import json
import gzip
import io
from PIL import Image
import re
from pathlib import Path

import numpy as np
from pxr import Usd, UsdGeom, UsdSkel

from prepare_viewmodel_gear import GLB, read_accessor


def gltf_skin(path):
    model, data = GLB["read_glb"](path)
    skin = model["skins"][0]
    matrices = np.asarray(read_accessor(model, data, skin["inverseBindMatrices"], 16)).reshape(-1, 4, 4)
    names = [model["nodes"][i]["name"] for i in skin["joints"]]
    return names, np.linalg.inv(matrices)[:, 3, :3]


# Each side lists shoulder, elbow, wrist and the first weighted joint of each finger.
FINGERS = ("Thumb", "Index", "Mid", "Ring", "Pinky")
USD_HANDS = {
    "AK12": ((10, 11, 12, 17, 14, 21, 25, 29), (33, 34, 35, 40, 37, 44, 48, 52)),
    "HK416": ((9, 10, 34, 35, 38, 41, 44, 47), (12, 13, 17, 18, 21, 24, 27, 30)),
    "Python357": ((9, 10, 11, 12, 16, 20, 25, 29), (33, 34, 35, 36, 40, 44, 49, 53)),
    "MP7": ((69, 34, 36, 47, 39, 43, 54, 50), (115, 80, 82, 93, 85, 89, 100, 96)),
    "Scorpion": ((695, 697, 699, 715, 709, 756, 783, 810), (869, 871, 873, 889, 883, 930, 957, 984)),
}


def find(names, prefix):
    return next(i for i, name in enumerate(names) if name.startswith(prefix))


def anchors(names, model, side):
    if model == "Human":
        prefix = f"CC_Base_{side}_"
        keys = ["UpperarmTwist01", "ForearmTwist01", "Hand"]
        keys.extend(f"{finger}{j}" for finger in FINGERS for j in (1, 2, 3))
        return [find(names, prefix + key + "_") for key in keys]
    if model in USD_HANDS:
        upper, elbow, wrist, *digits = USD_HANDS[model][side == "R"]
        numbers = [upper, elbow, wrist] + [n+j for n in digits for j in range(3)]
        return [names.index("n" + str(i)) for i in numbers]
    if model == "Sniper":
        keys = [f"{side}_{key}_" for key in ("arm", "elbow", "wrist")]
        keys.extend(f"{side}_{digit}{j}_" for digit in ("thumb", "point", "middle", "ring", "pink") for j in (1, 2, 3))
    else:
        keys = [f"UpArm_{side}_", f"BoneTwist_03.{side}_", f"Hand_{side}_"]
        keys.extend(f"Bone_{side}.{start+j:03}_" for start in (20, 5, 9, 13, 17) for j in range(3))
    return [find(names, key) for key in keys]


def segment_frames(points):
    # A joint's child sets its length axis. The breadth of the palm fixes roll.
    breadth = points[6] - points[15]
    for i, origin in enumerate(points):
        if i == 2:
            end = points[9]
        elif i >= 3 and (i-3) % 3 == 2:
            end = origin + (origin - points[i-1])
        else:
            end = points[i+1]
        along = end - origin
        forward = along / np.linalg.norm(along)
        across = breadth - forward * (breadth @ forward)
        across /= np.linalg.norm(across)
        radius = np.linalg.norm(breadth)
        result = np.eye(4)
        result[:3, :3] = np.column_stack((along, across * radius, np.cross(forward, across) * radius))
        result[:3, 3] = origin
        yield result


def usd_skin(path, name):
    stage = Usd.Stage.Open(str(path))
    first = f"n{USD_HANDS[name][0][0]}"
    for prim in stage.Traverse():
        if not prim.IsA(UsdSkel.Skeleton):
            continue
        skin = UsdSkel.Skeleton(prim)
        names = [str(j).split("/")[-1] for j in skin.GetJointsAttr().Get()]
        if first not in names:
            continue
        positions = np.array([list(m.ExtractTranslation()) for m in skin.GetBindTransformsAttr().Get()])
        donors = [str(p.GetPath()) for p in stage.Traverse() if p.IsA(UsdGeom.Mesh)
                  and UsdSkel.BindingAPI(p).GetInheritedSkeleton().GetPrim() == prim
                  and any(k in p.GetName().lower() for k in
                          ("hands", "watch", "cloth", "glove", "mesh_body", "ch22_body", "mannequin", "arms_"))]
        if not donors:
            raise ValueError(f"No donor arms identified for {name}")
        return names, positions, {"skeleton": str(prim.GetPath()), "hide": donors}
    raise ValueError(f"No arm skeleton in {path}")


def extract_skin(human, dest, names):
    model, data = GLB["read_glb"](human)
    sections = []
    joint_keys = {key.rsplit("_", 1)[0]: key for key in names}
    for material in ("Std_Skin_Arm", "Std_Nails"):
        prim, = [p for mesh in model["meshes"] for p in mesh["primitives"]
                 if model["materials"][p["material"]]["name"] == material]
        attrs = prim["attributes"]
        positions, normals, uvs, joints, weights = [read_accessor(model, data, attrs[key], width)
            for key, width in (("POSITION", 3), ("NORMAL", 3), ("TEXCOORD_0", 2), ("JOINTS_0", 4), ("WEIGHTS_0", 4))]
        vertices = []
        for point, normal, uv, js, ws in zip(positions, normals, uvs, joints, weights):
            influences = {}
            for j, w in zip(js, ws):
                if w <= 0:
                    continue
                key = names[j].rsplit("_", 1)[0]
                if "Forearm" in key or "ElbowShare" in key:
                    key = key[:10] + "ForearmTwist01"
                elif "Upperarm" in key or "Shoulder" in key:
                    key = key[:10] + "UpperarmTwist01"
                if key in joint_keys and re.search(r"_(?:Hand|UpperarmTwist01|ForearmTwist01|(?:Pinky|Ring|Mid|Index|Thumb)[123])$", key):
                    influences[joint_keys[key]] = influences.get(joint_keys[key], 0) + w
            total = sum(influences.values())
            vertices.append([point, normal, uv, [[k, w/total] for k,w in influences.items()]] if total > .5 else None)
        faces = read_accessor(model, data, prim["indices"], 1)
        groups = {}
        for i in range(0, len(faces), 3):
            face = [v for v, in faces[i:i+3]]
            if not all(vertices[v] for v in face):
                continue
            distance = sum(abs(positions[v][0]) for v in face) / 3
            section = "ViewmodelGlovesNails" if material == "Std_Nails" else (
                "ViewmodelGloves" if distance > 71.3 else "ViewmodelUpperSleeves" if distance < 34 else "ViewmodelSleeves")
            groups.setdefault(section, []).extend(face)
        texture = model["materials"][prim["material"]]["extensions"]["KHR_materials_pbrSpecularGlossiness"]["diffuseTexture"]["index"]
        image = model["images"][model["textures"][texture]["source"]]
        view = model["bufferViews"][image["bufferView"]]
        offset = view.get("byteOffset", 0)
        bitmap = Image.open(io.BytesIO(data[offset:offset+view["byteLength"]])).convert("RGB")
        bitmap.thumbnail((1024,1024))
        filename = material + ".jpg"
        bitmap.save(dest / filename, quality=88)
        for name, indices in groups.items():
            used = list(dict.fromkeys(indices))
            remap = {old: new for new, old in enumerate(used)}
            sections.append([name, filename, [vertices[k] for k in used], [remap[k] for k in indices]])
    (dest / "skin.json.gz").write_bytes(gzip.compress(json.dumps(sections, separators=(",", ":")).encode(), mtime=0))


def bake(human, generated, output):
    human_names, human_positions = gltf_skin(human)
    extract_skin(human, output.parent, human_names)
    fits = {}
    for name in ("Sidearm", "Sniper", "Makarov", "ScrapRifle", *USD_HANDS):
        if name in USD_HANDS:
            names, positions, entry = usd_skin(generated / (name + "Rig.usda"), name)
        else:
            path = generated / name / f"SK_{name}Viewmodel.glb" if name in ("Sidearm", "Sniper") else generated / (name + ".glb")
            names, positions = gltf_skin(path)
            entry = {}
        joint_fits = {}
        for side in ("L", "R"):
            source = anchors(human_names, "Human", side)
            target = anchors(names, name, side)
            for i, (src, dst) in enumerate(zip(segment_frames(human_positions[source]), segment_frames(positions[target]))):
                transform = dst @ np.linalg.inv(src)
                normal = np.linalg.inv(transform[:3,:3]).T
                assert np.isfinite(transform).all() and np.linalg.det(transform[:3,:3]) > 0
                joint_fits[human_names[source[i]]] = [target[i], transform[:3].round(9).tolist(), normal.round(9).tolist()]
        entry["bones"] = joint_fits
        fits[name] = entry
    output.write_text(json.dumps(fits, separators=(",", ":")) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("human", "generated", "output"):
        parser.add_argument(argument, type=Path)
    bake(**vars(parser.parse_args()))
