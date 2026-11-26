"""Prepare source-branch firearm rigs and wearable weapon meshes in the editor."""
import runpy
import zipfile
from pathlib import Path
import os
import gzip
import traceback

import unreal


RIGS = {
    "Makarov": ("makarov_pm_fps_animations.glb", {"Idle": "MK_Idle", "Walk": "MK_walk", "Fire": "MK_Shot", "Reload": "MK_ReloadFull"}),
    "ScrapRifle": ("scrap_sniper_anim.glb", {"Idle": "SRifle_Idle", "Walk": "SRifle_Walk", "Fire": "SRifle_Shot_nosight", "Reload": "SRifle_Reload_Full"}),
    "AK12": ("AK-12Animated.usdz", {}),
    "HK416": ("HK_416_A7_FPS_ANIMATION.usdz", {}),
    "MP7": ("MP7A1_Animations.usdz", {}),
    "Scorpion": ("Scorpion__First_Person_Animations_2026_Remake.usdz", {}),
    "Python357": ("revolver_animated.usdz", {}),
}


WORLD = {
    "Makarov": "makarov_pm_fps_animations.glb", "Python357": "357_python_revolver_riggedgame_ready.glb",
    "AK12": "ak-12.usdz", "HK416": "HK_416_A7_free_download.usdz", "MP7": "HK_MP7_A1_model.usdz",
    "Scorpion": "CZ_Scorpion_Evo3_A1.usdz", "ScrapRifle": "scrap_sniper.glb",
}


def part_layer(tools, generated, name, kind, source):
    binding = tools / "ArsenalBindings" / (name + kind + ".usda.gz")
    relative = Path(os.path.relpath(source, generated)).as_posix()
    layer = generated / (name + kind + ".usda")
    with gzip.open(binding, "rt", encoding="utf-8") as file:
        layer.write_text(file.read().replace("SOURCE_LAYER", relative), encoding="utf-8")
    return layer


def import_world(name, source, directory, generated, glb, flatten, import_asset):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if assets.does_asset_exist(directory + "/World"):
        return
    generated.mkdir(parents=True, exist_ok=True)
    if source.suffix == ".glb":
        path = generated / "World.glb"
        flatten(glb, source, path)
        import_asset(path, directory, unreal.StaticMesh)
        return
    # Expose one component-kind model to USD's mesh collapse translator.
    path = generated / "World.usda"
    reference = Path(os.path.relpath(source, generated)).as_posix()
    overrides = ""
    if name == "AK12":
        for child in reversed(("SkinnedMeshes", "Sketchfab_model", "Root", "metarig", "metarig_rootJoint", "skin0")):
            if child == "skin0":
                overrides = 'def Xform "skin0" {\n over "skeleton" (active = false) {}\n}'
            else:
                overrides = f'over "{child}" {{\n{overrides}\n}}'
    path.write_text('#usda 1.0\n(defaultPrim = "World")\n' +
                    f'def Xform "World" (\n kind = "component"\n references = @{reference}@\n)\n' +
                    '{\n' + overrides + '\n}\n', encoding="utf-8")
    options = unreal.UsdStageImportOptions()
    options.reuse_identical_assets = False
    options.merge_identical_material_slots = False
    options.import_actors = False
    options.import_skeletal_animations = False
    options.kinds_to_collapse = 0x1f  # Include every model kind, including the wrapper component.
    import_asset(path, directory, unreal.StaticMesh, options)


def import_rig(source, directory, clips):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    model_name = directory.split("/")[-2]
    required = ["Rig0", "Presentation0", *clips]
    if model_name in ("HK416", "Python357"):
        required.extend(("Rig1", "Presentation1", "Fire", "Fire1", "Reload", "Reload1"))
    if all(assets.does_asset_exist(f"{directory}/{name}") for name in required):
        return
    task = unreal.AssetImportTask()
    task.filename = str(source)
    imported_folder = directory + "/Source"
    task.destination_path = imported_folder
    task.replace_existing = True
    if source.suffix in (".usdz", ".usda"):
        options = unreal.UsdStageImportOptions()
        options.reuse_identical_assets = False
        options.merge_identical_material_slots = False
        options.import_actors = False
        options.import_skeletal_animations = True
        task.options = options
    task.automated = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    # Interchange and USD populate different task result fields. USD may add a
    # stage-named subfolder. Keep discovery scoped to this weapon's import.
    paths = set(assets.list_assets(imported_folder, recursive=True))
    paths.update(task.imported_object_paths)
    objects = {obj.get_path_name(): obj for obj in task.get_objects() if obj}
    for path in sorted(paths):
        obj = assets.load_asset(path)
        if obj:
            objects[obj.get_path_name()] = obj
    rigs = sorted((obj for obj in objects.values() if isinstance(obj, unreal.SkeletalMesh)), key=lambda obj: obj.get_path_name())
    animations = sorted((obj for obj in objects.values() if isinstance(obj, unreal.AnimSequence)), key=lambda obj: obj.get_path_name())
    if not rigs:
        raise RuntimeError(f"No skeletal mesh imported from {source.name}")
    if not animations:
        raise RuntimeError(f"No animation sequence imported from {source.name}")
    def alias(obj, name):
        target = f"{directory}/{name}"
        if not assets.does_asset_exist(target) and not assets.duplicate_loaded_asset(obj, target):
            raise RuntimeError(f"Could not create {target}")
    for index, rig in enumerate(rigs):
        matches = [clip for clip in animations if clip.get_editor_property("skeleton") == rig.get_editor_property("skeleton")]
        if not matches:
            raise RuntimeError(f"Missing animation for {rig.get_name()}")
        alias(rig, f"Rig{index}")
        roles = {}
        for role, suffix in clips.items():
            roles[role] = next(clip for clip in matches if suffix.lower() in clip.get_name().lower())
            alias(roles[role], role if index == 0 else f"{role}{index}")
        if model_name in ("Python357", "HK416"):
            for role in ("Fire", "Reload"):
                alias(matches[0], role if index == 0 else f"{role}{index}")
        alias(roles.get("Idle", matches[0]), f"Presentation{index}")
    if not assets.save_directory(directory):
        raise RuntimeError(f"Could not save {model_name} animation assets")


def prepare_accessories(source, folder, dest, generated, glb, import_asset):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.does_asset_exist(dest + "/M_RemovedPart"):
        hidden = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_RemovedPart", dest, unreal.Material, unreal.MaterialFactoryNew())
        hidden.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        hidden.set_editor_property("used_with_skeletal_mesh", True)
        mask = unreal.MaterialEditingLibrary.create_material_expression(hidden, unreal.MaterialExpressionConstant, -200, 0)
        mask.set_editor_property("r", 0.0)
        unreal.MaterialEditingLibrary.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
        unreal.MaterialEditingLibrary.recompile_material(hidden)
        if not assets.save_loaded_asset(hidden):
            raise RuntimeError("Could not save removed-part material")
    for name, filename, parent in (
        ("SuppressorPistol", "suppressors_10_pack.glb", "Ti-RANT45_1"),
        ("SuppressorRifle", "suppressors_10_pack.glb", "Socom_4"),
        ("MuzzleBrake", "free_weapon_accessories_pack.glb", "Compensator 1"),
        ("VariableScope", "lpvo_orion_scope.glb", "upper_02"),
        ("ReflexSight", "free_weapon_accessories_pack.glb", "Sight 1"),
        ("IRSight", "reap-ir.glb", "Reap_low"),
    ):
        if assets.does_asset_exist(f"{dest}/{name}/{name}"):
            continue
        model, binary = glb["read_glb"](folder / filename)
        node = next(n for n in model["nodes"] if n.get("name") == parent)
        children = [model["nodes"][i] for i in node["children"]]
        if any(set(n) - {"name", "mesh", "extras"} for n in children):
            raise ValueError(f"Unexpected local transform in {name}")
        primitives = [p for child in children for p in model["meshes"][child["mesh"]]["primitives"]]
        model["nodes"] = [{"name": name, "mesh": 0}]
        model["meshes"] = [{"name": name, "primitives": primitives}]
        model["scenes"] = [{"nodes": [0]}]
        model["scene"] = 0
        path = generated / (name + ".glb")
        glb["write_glb"](path, model, binary)
        import_asset(path, dest + "/" + name, unreal.StaticMesh)
    for name in ("Bipod", "Flashlight", "Stock"):
        options = unreal.FbxImportUI()
        options.import_as_skeletal = False
        options.automated_import_should_detect_type = False
        options.static_mesh_import_data.combine_meshes = True
        import_asset(source / "Workbench" / (name + ".fbx"), dest + "/" + name, unreal.StaticMesh, options)
    # FBX material references use the original texture filenames.
    for name, archive_name, source_name in (
        ("PEQ15", "peq-15.zip", "source/PEG15_Close.fbx"),
        ("ThermalSight", "holosun-drs-th.zip", "source/DRS-TH.fbx"),
    ):
        if assets.does_asset_exist(f"{dest}/{name}/{name}"):
            continue
        directory = generated / name
        directory.mkdir(exist_ok=True)
        path = directory / (name + ".fbx")
        with zipfile.ZipFile(folder / archive_name) as archive:
            path.write_bytes(archive.read(source_name))
            for entry in archive.namelist():
                if entry.startswith("textures/") and not entry.endswith("/"):
                    (directory / Path(entry).name).write_bytes(archive.read(entry))
        options = unreal.FbxImportUI()
        options.import_as_skeletal = False
        options.automated_import_should_detect_type = False
        options.static_mesh_import_data.combine_meshes = True
        options.import_textures = True
        options.import_materials = True
        import_asset(path, dest + "/" + name, unreal.StaticMesh, options)


def prepare(tools, source, art, import_asset, import_skeletal_pack):
    arms = runpy.run_path(str(tools / "player_viewmodel_arms.py"))
    skin = source / "PlayerArms"
    folder = source / "Arsenal"
    dest = art + "/Arsenal"
    generated = tools.parent / "Saved" / "GeneratedArt" / "Arsenal"
    generated.mkdir(parents=True, exist_ok=True)
    glb = runpy.run_path(str(tools / "prepare_sidearm_glb.py"))
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    failures = []

    def attempt(label, operation, *args):
        try:
            operation(*args)
        except Exception as error:
            failures.append(f"{label}: {error}")
            unreal.log_error(f"{label}: {traceback.format_exc()}")

    def mount_meshes(name, paths):
        for path in paths:
            if not assets.does_asset_exist(path):
                continue
            mesh = assets.load_asset(path)
            if assets.get_metadata_tag(mesh, "CrossroadsMountBake") == "1":
                return
            if unreal.CrossroadsWorldBake.prepare_weapon_mounts(mesh, name in ("Sidearm", "Makarov", "Python357")):
                assets.set_metadata_tag(mesh, "CrossroadsMountBake", "1")
                if not assets.save_loaded_asset(mesh):
                    raise RuntimeError(f"Could not save mount sockets on {path}")
                return
        raise RuntimeError(f"No weapon geometry with a receiver in {paths}")

    mesh_tools = runpy.run_path(str(tools / "arsenal_world_mesh.py"))
    section_tools = runpy.run_path(str(tools / "weapon_sections.py"))
    gear = runpy.run_path(str(tools / "prepare_viewmodel_gear.py"))
    weapons = source / "Weapons"
    bases = (
        ("Sidearm", "SK_SidearmViewmodel", ("ViewmodelIdle", "ViewmodelWalk", "ViewmodelFire", "ViewmodelReload", "ViewmodelReloadEmpty"),
         "fps_pistol_animations.glb", "SM_CrossroadsSidearm.glb", (weapons / "SM_CrossroadsSidearm.glb", weapons / "SM_Magazine.glb")),
        ("Sniper", "SK_SniperViewmodel", ("SniperHold", "SniperFire", "SniperReload"),
         "SK_SniperViewmodel.glb", "SM_CrossroadsSniper.glb", ()),
    )

    def prepare_base(name, rig_name, takes, donor, prop, extra):
        directory = f"{dest}/{name}/PlayerArms"
        geometry = f"{dest}/{name}/Assembly/Geometry"
        if assets.does_asset_exist(geometry + "/World") and all(assets.does_asset_exist(f"{directory}/{asset}") for asset in (rig_name, *takes)):
            return
        output = generated / name
        output.mkdir(exist_ok=True)
        dressed = output / "Dressed" / (rig_name + ".glb")
        gear["prepare_viewmodel_gear"](weapons / donor, dressed, *extra)
        section_tools["base_models"](name, weapons / prop, dressed, output)
        import_asset(output / "World.glb", geometry, unreal.StaticMesh)
        arms["dress_gltf"](skin, name, output / dressed.name, glb)
        import_skeletal_pack(directory, (rig_name, *takes), output / dressed.name)

    def prepare_rig(name, filename, clips):
        directory = dest + "/" + name + "/PlayerArms"
        rig = folder / filename
        if name in ("Makarov", "ScrapRifle"):
            model, binary = glb["read_glb"](rig)
            gear["split_arms"](model, binary, "material" if name == "Makarov" else "arms")
            if name == "Makarov":
                # The second mesh is a preinstalled donor accessory, separate from its iron sights.
                for node in model["nodes"]:
                    if node.get("mesh") == 1:
                        del node["mesh"]
                        node.pop("skin", None)
            section_tools["split_props"](model, binary, name, view=True)
            joint_name = "Body_015" if name == "Makarov" else "Body_057"
            joint = next(i for i, node in enumerate(model["nodes"]) if node.get("name") == joint_name)
            markers = ((0,.056,-.06), (0,.056,.10), (0,.077,-.06)) if name == "Makarov" else (
                (0,.055,-.11), (0,.055,.74), (0,.11,-.11))
            for marker, point in zip(("GunRear", "GunFront", "GunTop"), markers):
                glb["add_sight"](model, binary, joint, marker, point)
            binary.extend(bytes(-len(binary) % 4))
            rig = generated / (name + ".glb")
            glb["write_glb"](rig, model, binary)
        if name == "Python357":
            # The source animates the cylinder and hammer as rigid scene nodes.
            # This authored USD layer binds those nodes to a second animated rig.
            with zipfile.ZipFile(folder / "Python357_stage.zip") as archive:
                layer = archive.read("Python357.usda").decode("utf-8")
            rig = generated / "Python357.usda"
            relative = Path(os.path.relpath(folder / filename, generated)).as_posix()
            rig.write_text(layer.replace(f"@{filename}@", f"@{relative}@"), encoding="utf-8")
        if name == "Scorpion":
            # The donor scene includes a skinned skybox. Exclude it before Unreal computes bounds.
            rig = generated / "Scorpion.usda"
            relative = Path(os.path.relpath(folder / filename, generated)).as_posix()
            patch = 'over "Skybox_Baked_Skybox_0" (active = false) {}'
            for parent in reversed("SkinnedMeshes/Sketchfab_model/CZ_Scorpion_Evo3_A1_Anim_fbx/Object_2/RootNode/rig/Object_8/_rootJoint/skin0".split("/")):
                patch = f'over "{parent}" {{\n{patch}\n}}'
            rig.write_text('#usda 1.0\n(defaultPrim = "Scene"\n endTimeCode = 399.6000061035156\n timeCodesPerSecond = 120)\n' +
                           f'def Xform "Scene" (references = @{relative}@) {{\n{patch}\n}}\n', encoding="utf-8")
        if rig.suffix in (".usdz", ".usda"):
            rig = part_layer(tools, generated, name, "Rig", rig)
            rig = arms["dress_usd"](skin, name, rig)
        else:
            arms["dress_gltf"](skin, name, rig, glb)
        import_rig(rig, directory, clips)

    def geometry_dir(name):
        suffix = "WeaponSurface" if name in ("Makarov", "Python357") else "Assembly/Geometry"
        return f"{dest}/{name}/{suffix}"

    def prepare_world(name):
        geometry = geometry_dir(name)
        if assets.does_asset_exist(geometry + "/World"):
            return
        prop = folder / WORLD[name]
        if prop.suffix == ".glb":
            model, binary = glb["read_glb"](prop)
            if name in ("Makarov", "Python357"):
                # The prop scenes include a display stand or loose rounds. Use the
                # actual weapon in bind space, retaining its material and UVs.
                mesh_name = "makarov_base_makarov_material_0" if name == "Makarov" else "revolver_revolver_0"
                weapon = next(mesh for mesh in model["meshes"] if mesh["name"].lower() == mesh_name)
                model["meshes"] = [weapon]
                model["nodes"] = [{"mesh": 0}]
                model["scenes"] = [{"nodes": [0]}]
                model["scene"] = 0
                model.pop("animations", None)
                model.pop("skins", None)
            section_tools["split_props"](model, binary, name, view=name == "Makarov")
            prop = generated / (name + "Sections.glb")
            glb["write_glb"](prop, model, binary)
        else:
            prop = part_layer(tools, generated, name, "World", prop)
        import_world(name, prop, geometry, generated / name, glb, mesh_tools["flatten"], import_asset)

    for base in bases:
        attempt(base[0], prepare_base, *base)
    for name, (filename, clips) in RIGS.items():
        attempt(name + " world geometry", prepare_world, name)
        attempt(name + " animation", prepare_rig, name, filename, clips)
    for name in [base[0] for base in bases] + list(RIGS):
        directory = f"{dest}/{name}"
        attempt(name + " static mounts", mount_meshes, name, [geometry_dir(name) + "/World"])
        rigs = [f"SK_{name}Viewmodel"] if name in ("Sidearm", "Sniper") else ["Rig0", "Rig1"]
        attempt(name + " animated mounts", mount_meshes, name, [f"{directory}/PlayerArms/{rig}" for rig in rigs])
    attempt("attachments", prepare_accessories, source, folder, dest, generated, glb, import_asset)
    if failures:
        raise RuntimeError("Incomplete arsenal preparation: " + "; ".join(failures))
