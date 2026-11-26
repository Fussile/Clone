"""Prepare source-branch firearm rigs and wearable weapon meshes in the editor."""
import runpy
import zipfile
from pathlib import Path
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


WORLD = {"Makarov": "makarov_pm_fps_animations.glb", "ScrapRifle": "scrap_sniper.glb"}


def import_rig(source, directory, clips):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    model_name = directory.split("/")[-2]
    required = ["Rig0", "Presentation0", *clips]
    if all(assets.does_asset_exist(f"{directory}/{name}") for name in required):
        return
    task = unreal.AssetImportTask()
    task.filename = str(source)
    imported_folder = directory + "/Source"
    task.destination_path = imported_folder
    task.replace_existing = True
    task.automated = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    # Interchange can save related assets in subfolders beneath this weapon's import.
    paths = set(assets.list_assets(imported_folder, recursive=True))
    paths.update(task.imported_object_paths)
    objects = {obj.get_path_name(): obj for obj in task.get_objects() if obj}
    for path in sorted(paths):
        obj = assets.load_asset(path)
        if obj:
            objects[obj.get_path_name()] = obj
    rigs = sorted((obj for obj in objects.values() if isinstance(obj, unreal.SkeletalMesh)), key=lambda obj: obj.get_path_name())
    animations = sorted((obj for obj in objects.values() if isinstance(obj, unreal.AnimSequence)), key=lambda obj: obj.get_path_name())
    if directory.endswith("/Unified") and (len(rigs) != 1 or len(animations) != 1):
        raise RuntimeError(f"Expected one combined rig and take in {directory}, found {len(rigs)} and {len(animations)}")
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

    portable = {"AK12", "HK416", "MP7", "Scorpion", "Python357"}
    unpack = runpy.run_path(str(tools / "unpack_firearm.py"))["unpack"]

    def rig_dir(name):
        if name in portable:
            return f"{dest}/{name}/Unified"
        if name in ("Sidearm", "ScrapRifle", "Sniper"):
            return f"{dest}/{name}/FittedArms"
        return f"{dest}/{name}/PlayerArms"

    def prepare_base(name, rig_name, takes, donor, prop, extra):
        directory = rig_dir(name)
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
        if name == "Sniper":
            markers = runpy.run_path(str(tools / "sniper_clearance.py"))
            markers["add_clearance_markers"](output / dressed.name, glb, gear["read_accessor"])
        import_skeletal_pack(directory, (rig_name, *takes), output / dressed.name)

    def prepare_rig(name, filename, clips):
        directory = rig_dir(name)
        if name in portable:
            names = ("Rig0", "Presentation0")
            if not all(assets.does_asset_exist(f"{directory}/{role}") for role in names):
                path = generated / name / "UnifiedRig.glb"
                unpack(source, name, "Rig", folder / filename, path, glb)
                import_rig(path, directory, {})
            return
        rig = folder / filename
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
        arms["dress_gltf"](skin, name, rig, glb)
        import_rig(rig, directory, clips)

    def geometry_dir(name):
        suffix = "UnifiedSurface" if name in portable else "WeaponSurface" if name == "Makarov" else "Assembly/Geometry"
        return f"{dest}/{name}/{suffix}"

    def prepare_world(name):
        geometry = geometry_dir(name)
        if assets.does_asset_exist(geometry + "/World"):
            return
        if name in portable:
            path = generated / name / "Unified" / "World.glb"
            unpack(source, name, "World", folder / RIGS[name][0], path, glb)
            import_asset(path, geometry, unreal.StaticMesh)
            return
        prop = folder / WORLD[name]
        model, binary = glb["read_glb"](prop)
        if name == "Makarov":
            weapon = next(mesh for mesh in model["meshes"] if mesh["name"].lower() == "makarov_base_makarov_material_0")
            model["meshes"] = [weapon]
            model["nodes"] = [{"mesh": 0}]
            model["scenes"] = [{"nodes": [0]}]
            model["scene"] = 0
            model.pop("animations", None)
            model.pop("skins", None)
        section_tools["split_props"](model, binary, name, view=name == "Makarov")
        prop = generated / (name + "Sections.glb")
        glb["write_glb"](prop, model, binary)
        output = generated / name / "World.glb"
        output.parent.mkdir(exist_ok=True, parents=True)
        mesh_tools["flatten"](glb, prop, output)
        import_asset(output, geometry, unreal.StaticMesh)

    for base in bases:
        attempt(base[0], prepare_base, *base)
    for name, (filename, clips) in RIGS.items():
        attempt(name + " world geometry", prepare_world, name)
        attempt(name + " animation", prepare_rig, name, filename, clips)
    for name in [base[0] for base in bases] + list(RIGS):
        directory = f"{dest}/{name}"
        attempt(name + " static mounts", mount_meshes, name, [geometry_dir(name) + "/World"])
        rigs = [f"SK_{name}Viewmodel"] if name in ("Sidearm", "Sniper") else ["Rig0"]
        attempt(name + " animated mounts", mount_meshes, name, [f"{rig_dir(name)}/{rig}" for rig in rigs])
    attempt("attachments", prepare_accessories, source, folder, dest, generated, glb, import_asset)
    if failures:
        raise RuntimeError("Incomplete arsenal preparation: " + "; ".join(failures))
