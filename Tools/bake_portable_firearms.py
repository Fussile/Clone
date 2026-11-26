"""Offline bake of the five USD animation packs to single-skeleton glTF data.

Run with OpenUSD, NumPy and SciPy. The editor only unpacks the bake and embeds
original textures from the source archives. All five use the Makarov importer.
"""

import argparse
import gzip
import zipfile
import json
import lzma
from pathlib import Path
import struct

import numpy as np
from pxr import Usd, UsdGeom, UsdShade, UsdSkel
from scipy.spatial.transform import Rotation

from player_viewmodel_arms import dress_usd

# Distance between authored landmarks in metres and hold time in seconds.
MODELS = {
    "AK12": (0.59, None),
    "HK416": (0.60, 0.5),
    "MP7": (0.30, 0),
    "Scorpion": (0.41, None),
    "Python357": (0.22, 0),
}


class Buffer:
    def __init__(self):
        self.data = bytearray()
        self.doc = {
            "asset": {"version": "2.0"},
            "nodes": [],
            "scenes": [{"nodes": [0]}],
            "scene": 0,
            "meshes": [],
            "materials": [],
            "images": [],
            "textures": [],
            "accessors": [],
            "bufferViews": [],
        }

    def add(self, rows, kind, component=5126):
        value = np.asarray(rows, dtype={5126: "<f4", 5123: "<u2", 5125: "<u4"}[component])
        assert len(value) and np.isfinite(value).all()
        self.data.extend(bytes(-len(self.data) % 4))
        views = self.doc["bufferViews"]
        views.append({"buffer": 0, "byteOffset": len(self.data), "byteLength": value.nbytes})
        self.data.extend(value.tobytes())
        a = {"bufferView": len(views) - 1, "count": len(value), "type": kind, "componentType": component}
        if kind in ("VEC3", "SCALAR"):
            a.update(
                min=value.reshape(len(value), -1).min(0).tolist(),
                max=value.reshape(len(value), -1).max(0).tolist(),
            )
        self.doc["accessors"].append(a)
        return len(self.doc["accessors"]) - 1

    def save(self, file):
        self.doc["buffers"] = [{"byteLength": len(self.data)}]
        header = json.dumps(self.doc, separators=(",", ":"), allow_nan=False).encode()
        file.write_bytes(lzma.compress(struct.pack("<I", len(header)) + header + self.data))


def material_indices(mesh, buffer):
    count = len(mesh.GetFaceVertexCountsAttr().Get())

    def index(prim):
        mat = UsdShade.MaterialBindingAPI(prim).ComputeBoundMaterial()[0]
        if not mat:
            return -1
        name = mat.GetPrim().GetName()
        if not name.startswith(("GunPart", "Viewmodel")):
            return -1
        materials = buffer.doc["materials"]
        for i, item in enumerate(materials):
            if item["name"] == name:
                return i
        surface = mat.ComputeSurfaceSource()[0]
        pbr = {"metallicFactor": 0.25, "roughnessFactor": 0.65}
        result = {"name": name, "pbrMetallicRoughness": pbr, "doubleSided": True}
        for channel, obj, key in (
            ("diffuseColor", pbr, "baseColorTexture"),
            ("normal", result, "normalTexture"),
        ):
            inp = surface.GetInput(channel)
            if inp and inp.HasConnectedSource():
                tex = inp.GetConnectedSource()[0].GetPrim().GetAttribute("inputs:file").Get()
                filename = tex.path.replace("\\", "/").split("/")[-1]
                images = buffer.doc["images"]
                if {"name": filename} not in images:
                    buffer.doc["textures"].append({"source": len(images)})
                    images.append({"name": filename})
                obj[key] = {"index": images.index({"name": filename})}
            elif channel == "diffuseColor" and inp and inp.Get() is not None:
                pbr["baseColorFactor"] = list(inp.Get()) + [1]
        materials.append(result)
        return len(materials) - 1

    indices = np.full(count, index(mesh.GetPrim()), dtype=int)
    for subset in UsdGeom.Subset.GetAllGeomSubsets(mesh):
        indices[np.array(subset.GetIndicesAttr().Get(), dtype=int)] = index(subset.GetPrim())
    return indices


def unit(vec):
    return vec / np.maximum(np.linalg.norm(vec, axis=-1, keepdims=True), 1e-20)


def bake(name, layer, dest):
    stage = Usd.Stage.Open(str(layer))
    cache = UsdSkel.Cache()
    for prim in stage.Traverse():
        if prim.IsA(UsdSkel.Root):
            cache.Populate(UsdSkel.Root(prim), Usd.PrimDefaultPredicate)
    skeletons = [UsdSkel.Skeleton(p) for p in stage.Traverse() if p.IsA(UsdSkel.Skeleton)]
    queries = [cache.GetSkelQuery(s) for s in skeletons]
    start, end = stage.GetStartTimeCode(), stage.GetEndTimeCode()
    rate = stage.GetTimeCodesPerSecond()
    length, hold = MODELS[name]
    hold_time = end if hold is None else start + hold * rate
    order, parents, offsets = [], [], {}
    binds = []

    def world_pose(t):
        xf = UsdGeom.XformCache(t)
        return np.concatenate(
            [
                np.asarray(q.ComputeJointSkelTransforms(t))
                @ np.asarray(xf.GetLocalToWorldTransform(s.GetPrim()))
                for s, q in zip(skeletons, queries)
            ]
        )

    for s, q in zip(skeletons, queries):
        offset = len(order)
        offsets[s.GetPath()] = offset
        for path, parent in zip(q.GetJointOrder(), q.GetTopology().GetParentIndices()):
            joint = str(path).split("/")[-1]
            order.append(joint if joint.startswith("Gun") else f"R{offset}_{joint}")
            parents.append(offset + parent if parent >= 0 else -1)
        binds.extend(
            np.asarray(s.GetBindTransformsAttr().Get())
            @ np.asarray(UsdGeom.XformCache().GetLocalToWorldTransform(s.GetPrim()))
        )
    bind = np.asarray(binds)
    hold_pose = world_pose(hold_time)
    rear, front, top = [hold_pose[order.index(k), 3, :3] for k in ("GunRear", "GunFront", "GunTop")]
    f = unit(front - rear)
    z = unit(np.cross(f, top - rear))
    basis = np.stack([f, np.cross(z, f), z], axis=1)
    norm = np.eye(4)
    norm[:3, :3] = basis * (length / np.linalg.norm(front - rear))
    norm[3, :3] = -rear @ norm[:3, :3]
    bind = bind @ norm
    inv_bind = np.linalg.inv(bind)
    # Express the source skinning in rigid, unit-scale bind frames. The source
    # skeleton roots can differ in scale by orders of magnitude on the same gun.
    rest = np.repeat(np.eye(4)[None], len(order), axis=0)
    rest[:, 3, :3] = bind[:, 3, :3]
    animated = Buffer()
    nodes = animated.doc["nodes"]
    nodes.append({"name": "WeaponRoot", "children": []})
    for i, (joint, parent) in enumerate(zip(order, parents)):
        nodes.append(
            {
                "name": joint,
                "translation": (rest[i, 3, :3] - (rest[parent, 3, :3] if parent >= 0 else 0)).tolist(),
            }
        )
        nodes[parent + 1].setdefault("children", []).append(i + 1)
    nodes.append({"name": "WeaponAndArms", "mesh": 0, "skin": 0})
    animated.doc["scenes"][0]["nodes"].append(len(nodes) - 1)
    animated.doc["skins"] = [
        {
            "skeleton": 0,
            "joints": list(range(len(order) + 1)),
            "inverseBindMatrices": animated.add(np.concatenate(([np.eye(4)], np.linalg.inv(rest))), "MAT4"),
        }
    ]
    primitives = []
    rigid = Buffer()
    rigid.doc["nodes"] = [{"name": "World", "mesh": 0}]
    static_primitives = []
    for prim in stage.Traverse():
        if not prim.IsA(UsdGeom.Mesh):
            continue
        mesh = UsdGeom.Mesh(prim)
        roles = material_indices(mesh, animated)
        if not np.any(roles >= 0):
            continue
        skin = cache.GetSkinningQuery(prim)
        skeleton = UsdSkel.BindingAPI(prim).GetInheritedSkeleton()
        assert skin and skeleton, prim.GetPath()
        q = cache.GetSkelQuery(skeleton)
        xf = (
            np.asarray(skin.GetGeomBindTransform())
            @ np.asarray(UsdGeom.XformCache().GetLocalToWorldTransform(skeleton.GetPrim()))
            @ norm
        )
        pos = np.asarray(mesh.GetPointsAttr().Get()) @ xf[:3, :3] + xf[3, :3]
        joints, weights = map(np.asarray, skin.ComputeVaryingJointInfluences(len(pos)))
        joints = joints.reshape(len(pos), -1)
        weights = weights.reshape(joints.shape)
        if skin.GetJointOrder():
            palette = list(q.GetJointOrder())
            joints = np.array([palette.index(j) for j in skin.GetJointOrder()])[joints]
        joints = joints + offsets[skeleton.GetPath()]
        select = np.argsort(-weights, axis=1)[:, :4]
        joints = np.take_along_axis(joints, select, axis=1)
        weights = np.take_along_axis(weights, select, axis=1)
        assert weights.sum(axis=1).min() > 0.99, prim.GetPath()
        weights /= weights.sum(axis=1, keepdims=True)
        joints = np.pad(joints, ((0, 0), (0, 4 - joints.shape[1])))
        weights = np.pad(weights, ((0, 0), (0, 4 - weights.shape[1])))
        assert np.all(np.asarray(mesh.GetFaceVertexCountsAttr().Get()) == 3)
        vi = np.asarray(mesh.GetFaceVertexIndicesAttr().Get())
        uv_var = next(
            (
                v
                for v in UsdGeom.PrimvarsAPI(prim).GetPrimvarsWithValues()
                if v.GetPrimvarName() in ("st", "st0")
            ),
            None,
        )
        uv = np.array(uv_var.ComputeFlattened()) if uv_var else np.zeros((len(pos), 2))
        normals = np.array(mesh.GetNormalsAttr().Get())
        corners = np.arange(len(vi))
        normal_idx = corners if mesh.GetNormalsInterpolation() == "faceVarying" else vi
        if normals.ndim != 2:
            edges = pos[vi].reshape(-1, 3, 3)
            normals = unit(np.cross(edges[:, 1] - edges[:, 0], edges[:, 2] - edges[:, 0]))
            normal_idx = corners // 3
        else:
            normals = unit(normals @ np.linalg.inv(xf[:3, :3]).T)
        uv_idx = corners if uv_var and uv_var.GetInterpolation() == "faceVarying" else vi
        if uv_var and uv_var.GetInterpolation() == "constant":
            uv_idx = np.zeros_like(vi)
        uv = uv.copy()
        uv[:, 1] = 1 - uv[:, 1]
        packed, remap = np.unique(np.stack([vi, normal_idx, uv_idx], axis=1), axis=0, return_inverse=True)
        v, n, u = packed.T
        attrs = {
            "POSITION": animated.add(pos[v], "VEC3"),
            "NORMAL": animated.add(normals[n], "VEC3"),
            "TEXCOORD_0": animated.add(uv[u], "VEC2"),
            "JOINTS_0": animated.add(joints[v] + 1, "VEC4", 5123),
            "WEIGHTS_0": animated.add(weights[v], "VEC4"),
        }
        faces = remap.reshape(-1, 3)
        if (np.linalg.det(xf[:3, :3]) < 0) != (mesh.GetOrientationAttr().Get() == "leftHanded"):
            faces = faces[:, ::-1]
        is_weapon = any(
            animated.doc["materials"][m]["name"].startswith("GunPart") for m in set(roles) if m >= 0
        )
        if is_weapon:
            deform = inv_bind @ (hold_pose @ norm)
            mat = np.sum(deform[joints] * weights[:, :, None, None], axis=1)
            pts = np.einsum("ni,nij->nj", pos, mat[:, :3, :3]) + mat[:, 3, :3]
            ns = unit(np.einsum("ni,nij->nj", normals[n], np.linalg.inv(mat[v, :3, :3]).transpose(0, 2, 1)))
            static_attrs = {
                "POSITION": rigid.add(pts[v], "VEC3"),
                "NORMAL": rigid.add(ns, "VEC3"),
                "TEXCOORD_0": rigid.add(uv[u], "VEC2"),
            }
        for m in np.unique(roles[roles >= 0]):
            elems = faces[roles == m].flatten()
            primitives.append(
                {"attributes": attrs, "material": int(m), "indices": animated.add(elems, "SCALAR", 5125)}
            )
            if is_weapon and animated.doc["materials"][m]["name"].startswith("GunPart"):
                static_primitives.append(
                    {
                        "attributes": static_attrs,
                        "material": int(m),
                        "indices": rigid.add(elems, "SCALAR", 5125),
                    }
                )
    animated.doc["meshes"] = [{"name": "WeaponAndArms", "primitives": primitives}]
    rigid.doc["meshes"] = [{"name": "World", "primitives": static_primitives}]
    for k in ("materials", "images", "textures"):
        rigid.doc[k] = animated.doc[k]
    times = np.linspace(start, end, max(2, round((end - start) / rate * 30) + 1))
    tracks = []
    for t in times:
        absolute = rest @ inv_bind @ (world_pose(float(t)) @ norm)
        local = absolute.copy()
        children = np.array(parents) >= 0
        local[children] = absolute[children] @ np.linalg.inv(absolute[np.array(parents)[children]])
        tracks.append(local)
    matrices = np.asarray(tracks)
    track = {"name": "Presentation0", "samplers": [], "channels": []}
    timeline = animated.add((times - start) / rate, "SCALAR")
    ends = None
    for i in range(len(order)):
        linear = matrices[:, i, :3, :3]
        scales = np.linalg.norm(linear, axis=2)
        scales[:, 0] *= np.sign(np.linalg.det(linear))
        rot = Rotation.from_matrix((linear / scales[:, :, None]).transpose(0, 2, 1)).as_quat()
        for j in range(1, len(rot)):
            if rot[j] @ rot[j - 1] < 0:
                rot[j] *= -1
        for path, data in (("translation", matrices[:, i, 3, :3]), ("rotation", rot), ("scale", scales)):
            kind = "VEC4" if path == "rotation" else "VEC3"
            # Constant tracks can differ from the reference pose.
            if np.max(np.abs(data - data[0])) < 1e-6:
                default = nodes[i + 1].get(path, [0, 0, 0, 1] if path == "rotation" else [1, 1, 1])
                if np.allclose(default, data[0], atol=1e-6):
                    continue
                ids = [0, len(data) - 1]
                if ends is None:
                    ends = animated.add((times[ids] - start) / rate, "SCALAR")
                key = ends
            else:
                ids = slice(None)
                key = timeline
            index = len(track["samplers"])
            track["samplers"].append({"input": key, "output": animated.add(data[ids], kind)})
            track["channels"].append({"sampler": index, "target": {"node": i + 1, "path": path}})
    animated.doc["animations"] = [track]
    dest.mkdir(parents=True, exist_ok=True)
    animated.save(dest / (name + "Rig.xz"))
    rigid.save(dest / (name + "World.xz"))


ARCHIVES = {
    "AK12": "AK-12Animated.usdz",
    "HK416": "HK_416_A7_FPS_ANIMATION.usdz",
    "MP7": "MP7A1_Animations.usdz",
    "Scorpion": "Scorpion__First_Person_Animations_2026_Remake.usdz",
    "Python357": "revolver_animated.usdz",
}


def compose(name, sources, skin, folder):
    archive = (sources / ARCHIVES[name]).resolve()
    folder.mkdir(exist_ok=True, parents=True)
    if name == "Python357":
        with zipfile.ZipFile(sources / "Python357_stage.zip") as package:
            text = package.read("Python357.usda").decode()
        donor = folder / "Python357.usda"
        donor.write_text(text.replace(f"@{archive.name}@", f"@{archive.as_posix()}@"), encoding="utf8")
    elif name == "Scorpion":
        donor = folder / "Scorpion.usda"
        # The binding layer addresses /Scene in this donor. Only material-tagged gun
        # surfaces and survivor skin are exported, so the donor skybox is excluded.
        donor.write_text(
            '#usda 1.0\n(defaultPrim = "Scene")\n'
            + f'def Xform "Scene" (references = @{archive.as_posix()}@) {{}}\n',
            encoding="utf8",
        )
    else:
        donor = archive
    rig = folder / (name + "Rig.usda")
    binding = Path(__file__).parent / "ArsenalBindings" / (rig.name + ".gz")
    rig.write_text(
        gzip.decompress(binding.read_bytes()).decode().replace("SOURCE_LAYER", donor.as_posix()),
        encoding="utf8",
    )
    return dress_usd(skin, name, rig)


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    art = root / "SourceArt/Crossroads"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archives", type=Path, default=art / "Arsenal")
    parser.add_argument("--skin", type=Path, default=art / "PlayerArms")
    parser.add_argument("--output", type=Path, default=art / "PortableFirearms")
    args = parser.parse_args()
    for key in MODELS:
        source = compose(key, args.archives, args.skin, root / "Saved/GeneratedArt/PortableFirearms")
        bake(key, source, args.output)
