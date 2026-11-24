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


def dark_materials(prepare_art, palette):
    editing = unreal.MaterialEditingLibrary
    sampler = unreal.MaterialSamplerType
    prop = unreal.MaterialProperty
    made = {}

    def color_texture(source):
        if isinstance(source, unreal.MaterialInstanceConstant):
            for name in editing.get_texture_parameter_names(source):
                if re.search(r"base.?color|albedo|diffuse|^colou?r$|^bc$", str(name), re.I):
                    texture = editing.get_material_instance_texture_parameter_value(source, name)
                    if is_color(texture):
                        return texture
        textures = editing.get_material_used_textures(source)
        return next((texture for texture in textures if is_color(texture)
                     and re.search(r"base.?color|albedo|diffuse|_(?:d|bc|colou?r)(?:_|$)",
                                   texture.get_name(), re.I)), None)

    def is_color(texture):
        return (isinstance(texture, unreal.Texture2D) and texture.get_editor_property("srgb")
                and not re.search(r"emiss|interior|room|light.?map|mask|default|normal",
                                  texture.get_name(), re.I))

    def without_emission(source):
        if source is None:
            return None
        path = source.get_path_name()
        if path in made:
            return made[path]
        # Keep separate glazing slots dark. Simplify the pack's block and
        # baked façade shaders so an unpowered copy stays within SM5 limits.
        names = []
        parent = source
        while parent:
            names.append(parent.get_name().lower())
            parent = (parent.get_editor_property("parent")
                      if isinstance(parent, unreal.MaterialInstanceConstant) else None)
        names = " ".join(names)
        if re.search(r"glass|glazing|window|emiss", names):
            material = palette["glazing"]
        else:
            texture = color_texture(source)
            if texture:
                digest = hashlib.sha1(texture.get_path_name().encode()).hexdigest()[:12]
                sample_type = (sampler.SAMPLERTYPE_VIRTUAL_COLOR
                               if texture.get_editor_property("virtual_texture_streaming")
                               else sampler.SAMPLERTYPE_COLOR)
                material = prepare_art["make_material"](
                    "/Game/Art/Crossroads/World/LandmarkFinishes", f"M_SampleColor_{digest}", (
                        (texture, sample_type, "RGB", prop.MP_BASE_COLOR),
                    ))
            elif re.search(r"roof|metal|iron", names):
                material = palette["dark_trim"]
            else:
                material = palette["classical_civic"]
        made[path] = material
        return material
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
