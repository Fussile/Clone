"""Rebuild the authored USD layers using OpenUSD after preparing the local arsenal."""
from pathlib import Path
import gzip

from pxr import Gf, Usd, UsdGeom, UsdShade, UsdSkel

from weapon_sections import islands, part_numbers

TOOLS = Path(__file__).resolve().parent
PROJECT = TOOLS.parent
SOURCE = PROJECT / "SourceArt/Crossroads/Arsenal"
GENERATED = PROJECT / "Saved/GeneratedArt/Arsenal"

# Signed, one-based forward and up axes in each mesh donor's local space.
DONORS = {
    "AK12": ("AK-12Animated.usdz", "ak-12.usdz", (-2, 3), (-2, 3)),
    "HK416": ("HK_416_A7_FPS_ANIMATION.usdz", "HK_416_A7_free_download.usdz", (3, 2), (-1, 3)),
    "MP7": ("MP7A1_Animations.usdz", "HK_MP7_A1_model.usdz", (3, 2), (-1, 2)),
    "Scorpion": ("Scorpion.usda", "CZ_Scorpion_Evo3_A1.usdz", (3, 2), (3, 2)),
    "Python357": ("Python357.usda", None, (3, 2), None),
    "Makarov": (None, "Makarov_PM.usdz", None, (2, -3)),
}
# Mesh and receiver joint followed by rear, muzzle and rail coordinates.
LANDMARKS = {
    "AK12": ("barrel", "n69", (-.15,-.23,1.68), (-.15,-.82,1.68), (-.15,-.23,1.72)),
    "HK416": ("Upper_reciever_low_003_Low_0", "n56", (-.235,.055,.43), (-.235,.055,1.03), (-.235,.108,.43)),
    "MP7": ("mp7a1_Object_Slot_1_0", "n177", (0,.015,-.12), (0,.015,.18), (0,.045,-.12)),
    "Scorpion": ("CZScorpion_CZScorpion_0", "n1072", (-.093,1.524,.36), (-.093,1.524,.77), (-.093,1.57,.36)),
    "Python357": ("Part1", "J0", (0,7,0), (0,7,38), (0,11,0)),
}


def add_landmarks(stage, output, name):
    mesh, bone, *points = LANDMARKS[name]
    prim = next(p for p in stage.Traverse() if p.GetName() == mesh and p.IsA(UsdGeom.Mesh))
    binding = UsdSkel.BindingAPI(prim)
    skeleton = binding.GetInheritedSkeleton()
    geom = binding.GetGeomBindTransformAttr().Get() or Gf.Matrix4d(1)
    joints = list(skeleton.GetJointsAttr().Get())
    bind = list(skeleton.GetBindTransformsAttr().Get())
    rest = list(skeleton.GetRestTransformsAttr().Get())
    parent = next(i for i, key in enumerate(joints) if str(key).split("/")[-1] == bone)
    for key, point in zip(("GunRear", "GunFront", "GunTop"), points):
        matrix = Gf.Matrix4d(1).SetTranslate(geom.Transform(Gf.Vec3d(*point)))
        joints.append(f"{joints[parent]}/{key}")
        rest.append(matrix * bind[parent].GetInverse())
        bind.append(matrix)
    schema = UsdSkel.Skeleton(output.OverridePrim(skeleton.GetPath()))
    schema.CreateJointsAttr(joints)
    schema.CreateRestTransformsAttr(rest)
    schema.CreateBindTransformsAttr(bind)


def excluded(name, label, view, path):
    if "viewer_cube" in label or "skybox" in label:
        return True
    if name == "AK12":
        return label in ("aim", "lense") or (not view and ("hands" in label or "watch" in label))
    return name == "MP7" and ("suppressor" in label or (not view and "/MP7_assembly/" not in path))


def roles_for_mesh(name, label, view, points, faces, axes):
    fixed = None
    if name == "AK12":
        fixed = {"stock": 0, "barrel": 2, "bullets": 3, "mag": 3, "mag_001": 3}.get(label, 1)
    if name == "Makarov":
        fixed = [1, 3, 1, 0, 1, 3, 2, 1, 1, 1][int(label.split("_")[-1])]
    if name == "Python357" and label != "part1":
        fixed = 1 if label in ("part11", "part12") else 3
    if name == "MP7" and not view:
        fixed = 1
        if label.startswith("polysurface135_"):
            fixed = 2
        elif label.startswith(("polysurface65_", "polysurface67_", "polysurface136_")):
            fixed = 3
        elif "crossplate" in label or (label.startswith("mp7_assembly_road_cross") and max(v[0] for v in points) > 55):
            fixed = 0
    roles = part_numbers(points, faces, *axes, name in ("Makarov", "Python357"), fixed)
    if name == "Python357" and label == "part1":
        # The donor welds the grip and shroud to the frame. Use the joins in the original casting.
        for i, face in enumerate(faces):
            y = sum(points[v][1] for v in face) / 3
            z = sum(points[v][2] for v in face) / 3
            roles[i] = 0 if y < .4 and z < 4.0 else 2 if z > 13 else 1
    if name == "Scorpion":
        for island, indices in enumerate(islands(points, faces)):
            role = 1
            if island in (5, 6, 7):
                role = 2
            elif 26 <= island <= 30 or island >= 41:
                role = 3
            elif 32 <= island <= 38:
                role = 0
            for i in indices:
                roles[i] = role
    return roles


def bake(name, filename, axes, view):
    source = (GENERATED if filename.endswith(".usda") else SOURCE) / filename
    stage = Usd.Stage.Open(str(source))
    output = Usd.Stage.CreateInMemory()
    root = stage.GetDefaultPrim().GetPath()
    output.SetDefaultPrim(output.OverridePrim(root))
    output.SetTimeCodesPerSecond(stage.GetTimeCodesPerSecond())
    output.SetStartTimeCode(stage.GetStartTimeCode())
    output.SetEndTimeCode(stage.GetEndTimeCode())
    for prim in stage.Traverse():
        if prim.IsA(UsdSkel.Skeleton):
            animation = UsdSkel.BindingAPI(prim).GetInheritedAnimationSource()
            if not animation:
                animation, = [child for child in prim.GetParent().GetChildren() if child.IsA(UsdSkel.Animation)]
            binding = UsdSkel.BindingAPI.Apply(output.OverridePrim(prim.GetPath()))
            binding.CreateAnimationSourceRel().SetTargets([animation.GetPath()])
        if not prim.IsA(UsdGeom.Mesh):
            continue
        label = prim.GetName().lower()
        if excluded(name, label, view, str(prim.GetPath())):
            output.OverridePrim(prim.GetPath()).SetActive(False)
            continue
        if view and any(key in label for key in ("hands", "watch", "cloth", "glove", "mesh_body", "ch22_body", "mannequin", "arms_")):
            continue
        mesh = UsdGeom.Mesh(prim)
        points = mesh.GetPointsAttr().Get()
        flat = mesh.GetFaceVertexIndicesAttr().Get()
        material, _ = UsdShade.MaterialBindingAPI(prim).ComputeBoundMaterial()
        if name == "Python357" and label.startswith("part"):
            material = UsdShade.Material(stage.GetPrimAtPath("/Scene/Materials/revolver"))
        if not material or not flat:
            continue
        if any(n != 3 for n in mesh.GetFaceVertexCountsAttr().Get()):
            raise ValueError(f"Expected triangles in {name}/{label}")
        faces = list(zip(flat[::3], flat[1::3], flat[2::3]))
        roles = roles_for_mesh(name, label, view, points, faces, axes)
        target = output.OverridePrim(prim.GetPath())
        for role in sorted(set(roles)):
            path = root.AppendChild("PartMaterials").AppendChild(f"GunPart{role}_{material.GetPrim().GetName()}")
            section = UsdShade.Material.Define(output, path)
            section.GetPrim().GetReferences().AddInternalReference(material.GetPath())
            binding = UsdShade.MaterialBindingAPI.Apply(target)
            indices = [i for i, value in enumerate(roles) if value == role]
            if len(indices) != len(faces):
                subset = binding.CreateMaterialBindSubset(f"GunPart{role}", indices)
                binding = UsdShade.MaterialBindingAPI.Apply(subset.GetPrim())
            binding.Bind(section)
    if view:
        add_landmarks(stage, output, name)
    text = output.GetRootLayer().ExportToString().replace(
        "    defaultPrim", "    subLayers = [@SOURCE_LAYER@]\n    defaultPrim")
    kind = "Rig" if view else "World"
    (TOOLS / "ArsenalBindings" / f"{name}{kind}.usda.gz").write_bytes(gzip.compress(text.encode(), mtime=0))


if __name__ == "__main__":
    for model, (rig, world, rig_axes, world_axes) in DONORS.items():
        if rig:
            bake(model, rig, rig_axes, True)
        if world:
            bake(model, world, world_axes, False)
