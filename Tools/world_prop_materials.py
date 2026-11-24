"""Rebuild opaque materials from bundled GLB textures before stamping world props."""

import hashlib
import json
from pathlib import Path
import re
import runpy

WORLD = "/Game/Art/Crossroads/World"
NAMES = ("WarehouseDoorwayShell", "WeatheredShelf", "ScavengeCrate", "FuelPumpSolid",
         "OfficeCooler", "OfficeDrawer", "OfficeFiles", "OfficeWorkstation")
FORMATS = {"image/jpeg": ".jpg", "image/png": ".png"}
read_glb = runpy.run_path(str(Path(__file__).with_name("prepare_sidearm_glb.py")))["read_glb"]


def surfaces(model):
    indices = dict.fromkeys(primitive["material"] for mesh in model["meshes"]
                            for primitive in mesh["primitives"] if "material" in primitive)
    for index in indices:
        material = model["materials"][index]
        pbr = material["pbrMetallicRoughness"]
        roles = {"Color": pbr["baseColorTexture"]}
        if "normalTexture" in material:
            roles["Normal"] = material["normalTexture"]
        if "metallicRoughnessTexture" in pbr:
            roles["Packed"] = pbr["metallicRoughnessTexture"]
        yield index, material["name"], pbr, roles


def package_base(source, model, binary):
    digest = hashlib.sha256(binary)
    digest.update(json.dumps(model, sort_keys=True).encode("utf-8"))
    return f"{WORLD}/PropFinishes/{source.stem[3:]}/V1_{digest.hexdigest()[:12]}"


def required_packages(source_directory):
    for name in NAMES:
        source = source_directory / f"SM_{name}.glb"
        model, binary = read_glb(source)
        package = package_base(source, model, binary)
        for index, _, _, roles in surfaces(model):
            yield f"{package}/M_Surface_{index}"
            for role in roles:
                yield f"{package}/T_Surface_{index}_{role}"
        if name == "ScavengeCrate":
            yield f"{package}/M_SecurityCrate"


def apply(source, mesh, scratch, prepare_art, assets):
    import unreal

    model, binary = read_glb(source)
    package = package_base(source, model, binary)
    scratch = scratch.joinpath(*package.split("/")[-2:])
    scratch.mkdir(parents=True, exist_ok=True)
    sampler = unreal.MaterialSamplerType
    prop = unreal.MaterialProperty

    def load_texture(index, role, info):
        if info.get("texCoord", 0) != 0 or "KHR_texture_transform" in info.get("extensions", {}):
            raise ValueError(f"Unsupported texture coordinates for {source.name} {role}.")
        image = model["images"][model["textures"][info["index"]]["source"]]
        name = f"T_Surface_{index}_{role}"
        path = scratch / (name + FORMATS[image["mimeType"]])
        if not assets.does_asset_exist(f"{package}/{name}"):
            view = model["bufferViews"][image["bufferView"]]
            offset = view.get("byteOffset", 0)
            path.write_bytes(binary[offset:offset+view["byteLength"]])
        texture = prepare_art["import_asset"](path, package, unreal.Texture2D)
        settings = unreal.TextureCompressionSettings
        texture.set_editor_property("compression_settings", {
            "Color": settings.TC_DEFAULT, "Normal": settings.TC_NORMALMAP, "Packed": settings.TC_MASKS,
        }[role])
        texture.set_editor_property("srgb", role == "Color")
        texture.set_editor_property("virtual_texture_streaming", False)
        if role == "Normal":
            texture.set_editor_property("flip_green_channel", True)
        if not assets.save_loaded_asset(texture, only_if_is_dirty=True):
            raise RuntimeError(f"Could not save {texture.get_path_name()}.")
        return texture

    materials = {}
    military = None
    for index, name, pbr, images in surfaces(model):
        textures = {role: load_texture(index, role, info) for role, info in images.items()}
        samples = [(textures["Color"], sampler.SAMPLERTYPE_COLOR, "RGB", prop.MP_BASE_COLOR)]
        if "Normal" in textures:
            samples.append((textures["Normal"], sampler.SAMPLERTYPE_NORMAL, "RGB", prop.MP_NORMAL))
        scalars = []
        for key, channel, output in (("roughnessFactor", "G", prop.MP_ROUGHNESS),
                                     ("metallicFactor", "B", prop.MP_METALLIC)):
            factor = pbr.get(key, 1.0)
            if "Packed" in textures and factor != 0:
                samples.append((textures["Packed"], sampler.SAMPLERTYPE_MASKS, channel, output))
            else:
                scalars.append((factor, output))
        color = pbr.get("baseColorFactor", [1, 1, 1, 1])
        material = prepare_art["make_material"](
            package, f"M_Surface_{index}", samples, tint=unreal.LinearColor(*color) if color != [1, 1, 1, 1] else None,
            scalars=scalars)
        materials[name] = material
        if source.stem == "SM_ScavengeCrate":
            military = prepare_art["make_material"](
                package, "M_SecurityCrate", samples, tint=unreal.LinearColor(.38, .47, .31, 1), scalars=scalars)

    def normalized(name):
        return re.sub(r"[^a-z0-9]", "", str(name).lower())

    slots = mesh.get_editor_property("static_materials")
    by_name = {normalized(name): material for name, material in materials.items()}
    bindings = []
    for index, slot in enumerate(slots):
        existing = mesh.get_material(index)
        candidates = [normalized(slot.get_editor_property("material_slot_name"))]
        if existing:
            candidates.append(normalized(existing.get_name()))
        matches = {item for key, item in by_name.items() if any(key in name for name in candidates)}
        bindings.append(next(iter(matches)) if len(matches) == 1 else None)

    if len(materials) == 1:
        bindings = [next(iter(materials.values()))] * len(slots)
    elif any(material is None for material in bindings):
        # Interchange preserves primitive order when it has to invent a slot name.
        ordered = list(materials.values())
        if len(ordered) != len(bindings) or any(
                bound is not None and bound != expected for bound, expected in zip(bindings, ordered)):
            raise RuntimeError(f"Could not match imported material slots for {source.name}.")
        bindings = ordered
    for index, material in enumerate(bindings):
        if mesh.get_material(index) != material:
            mesh.set_material(index, material)
    if not slots or not assets.save_loaded_asset(mesh, only_if_is_dirty=True):
        raise RuntimeError(f"Could not save repaired materials for {source.name}.")
    return military
