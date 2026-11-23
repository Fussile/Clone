"""Discover locally installed Fab meshes and make dark City Sample material variants."""
import hashlib
import re

import unreal


def pack_meshes(root, folder):
    if not (root / "Content" / folder).is_dir():
        return []
    path = f"/Game/{folder}"
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([path], True)
    return sorted((entry for entry in registry.get_assets_by_path(path, recursive=True) or []
                   if str(entry.asset_class_path.asset_name) == "StaticMesh"),
                  key=lambda entry: str(entry.package_name))


def dark_materials(assets):
    editing = unreal.MaterialEditingLibrary
    made = {}

    def without_emission(source):
        if source is None:
            return None
        path = source.get_path_name()
        if path in made:
            return made[path]
        digest = hashlib.sha1(path.encode()).hexdigest()[:12]
        target = f"/Game/Art/Crossroads/World/Unpowered/{source.get_name()}_{digest}"
        variant = assets.load_asset(target) if assets.does_asset_exist(target) else None
        if variant is None:
            variant = assets.duplicate_loaded_asset(source, target)
        if variant is None:
            raise RuntimeError(f"Could not copy {path}")
        if assets.get_metadata_tag(variant, "CrossroadsUnpowered") != "1":
            if isinstance(variant, unreal.Material):
                unreal.CrossroadsWorldBake.disable_material_emission(variant)
            elif isinstance(variant, unreal.MaterialInstanceConstant):
                parent = without_emission(source.get_editor_property("parent"))
                editing.set_material_instance_parent(variant, parent)
                # Also reset emissive intensities exposed by any nested material functions.
                for name in editing.get_scalar_parameter_names(variant):
                    if re.search(r"emiss|light.?intensity|window.?light", str(name), re.I):
                        editing.set_material_instance_scalar_parameter_value(variant, name, 0.0)
                editing.update_material_instance(variant)
            else:
                raise RuntimeError(f"Could not make an unpowered material from {path}")
            assets.set_metadata_tag(variant, "CrossroadsUnpowered", "1")
            if not assets.save_loaded_asset(variant):
                raise RuntimeError(f"Could not save {target}")
        made[path] = variant
        return variant
    return without_emission


def factory_props(entries):
    patterns = {
        "wall": (r"wall|facade|façade", r"corner|door|wind|roof|pill|decal|broken|hole", (170, 2, 190), (1400, 100, 900)),
        "rack": (r"shel[fv]|shelves|racks?", r"broken|decal", (65, 30, 95), (500, 210, 410)),
        "forklift": (r"fork.?lift", r"wheel|seat|forks_|steer", (140, 70, 120), (520, 245, 430)),
        "drum": (r"barrel|drum|oil.?can", r"lid|fire|decal", (30, 24, 35), (190, 170, 170)),
        "pallet": (r"pallet", r"jack|truck|decal|stack", (55, 40, 6), (220, 180, 52)),
    }
    selected = {}
    for kind, (include, exclude, minimum, maximum) in patterns.items():
        for entry in entries:
            name = str(entry.asset_name)
            if not re.search(include, name, re.I) or re.search(exclude, name, re.I):
                continue
            mesh = entry.get_asset()
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            bounds = mesh.get_bounding_box()
            size = bounds.max-bounds.min
            dims = (max(size.x, size.y), min(size.x, size.y), size.z)
            if all(lo <= value <= hi for value, lo, hi in zip(dims, minimum, maximum)):
                selected[kind] = mesh
                break
    return selected
