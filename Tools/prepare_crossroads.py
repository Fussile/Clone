"""Import the starter art and prepare the editable playtest map in UE5.8."""

import hashlib
import os
from pathlib import Path
import re
import runpy
import traceback

import unreal


TOOLS = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / "Tools"
SOURCE = TOOLS.parent / "SourceArt" / "Crossroads"
ART = "/Game/Art/Crossroads"
SURFACES = ART + "/Surfaces"
PLAYER = ART + "/Player"
BUILDING = ART + "/Building"
ZOMBIE = ART + "/StationZombie"
WEAPON_AUDIO = "/Game/Audio/Weapons"
VIEWMODEL = ART + "/AlignedSidearm"
LEVEL = "/Game/Maps/L_SouthernCrossroads"


def import_asset(source, directory, asset_type, options=None, *, replace_existing=False,
                 refresh_if_source_changed=False):
    name = source.stem
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    path = f"{directory}/{name}"
    asset = assets.load_asset(path) if assets.does_asset_exist(path) else None
    revision = None
    if refresh_if_source_changed:
        revision = hashlib.sha256(source.read_bytes()).hexdigest()
        replace_existing |= asset is not None and assets.get_metadata_tag(asset, "CrossroadsSourceHash") != revision
    if asset is None or replace_existing:
        if not source.is_file():
            raise FileNotFoundError(source)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", directory)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", replace_existing)
        if options is not None:
            task.set_editor_property("options", options)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        matching = [item for item in task.get_objects() if isinstance(item, asset_type)]
        if len(matching) != 1:
            raise RuntimeError(f"Expected one {asset_type.__name__} from {source}.")
        asset = matching[0]
        if asset.get_path_name().split(".")[0] != path:
            if not assets.rename_loaded_asset(asset, path):
                raise RuntimeError(f"Could not save {source} as {path}.")
        if revision is not None:
            assets.set_metadata_tag(asset, "CrossroadsSourceHash", revision)
        if not assets.save_directory(directory, only_if_is_dirty=True, recursive=True):
            raise RuntimeError(f"Could not save imported assets in {directory}.")
    if not isinstance(asset, asset_type):
        raise RuntimeError(f"{path} has an unexpected asset type.")
    return asset


def make_material(directory, name, textures, tint=None, world_repeat_cm=None, *, scalars=()):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    path = f"{directory}/{name}"
    editing = unreal.MaterialEditingLibrary
    if assets.does_asset_exist(path):
        material = assets.load_asset(path)
        # A failed projection can leave a saved asset with a partial graph.
        if world_repeat_cm is None or assets.get_metadata_tag(material, "CrossroadsProjection") == "1":
            return material
        editing.delete_all_material_expressions(material)
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        material = tools.create_asset(name, directory, unreal.Material, unreal.MaterialFactoryNew())
        if material is None:
            raise RuntimeError(f"Could not create {path}.")
    uv = None
    if world_repeat_cm is not None:
        # Pick a projection on each flat face so a 40 m wall and a 1 m jamb
        # use the same physical texel density, including rotated structures.
        pos = editing.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, 0)
        uv = editing.create_material_expression(material, unreal.MaterialExpressionCustom, -620, 0)
        normal = editing.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -900, 160)
        inputs = []
        for pin_name in ("P", "N"):
            pin = unreal.CustomInput()
            pin.set_editor_property("input_name", unreal.Name(pin_name))
            inputs.append(pin)
        uv.set_editor_property("inputs", inputs)
        uv.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
        uv.set_editor_property("code", (
            "float3 n = abs(N); "
            "float2 uv = n.z >= max(n.x,n.y) ? P.xy : "
            "(n.x > n.y ? float2(P.y,-P.z) : float2(P.x,-P.z)); "
            f"return uv / {world_repeat_cm:.1f};"
        ))
        if (not editing.connect_material_expressions(pos, "", uv, "P")
                or not editing.connect_material_expressions(normal, "", uv, "N")):
            raise RuntimeError(f"Could not project textures for {path}.")
    if not textures and tint is not None:
        color = editing.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -300, 0)
        color.set_editor_property("constant", tint)
        if not editing.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError(f"Could not color {path}.")
    for index, (texture, sampler, output, prop) in enumerate(textures):
        sample = editing.create_material_expression(
            material, unreal.MaterialExpressionTextureSample, -320, index * 240
        )
        sample.set_editor_property("texture", texture)
        sample.set_editor_property("sampler_type", sampler)
        if uv and not editing.connect_material_expressions(uv, "", sample, "UVs"):
            raise RuntimeError(f"Could not set texture scale for {path}.")
        expression = sample
        if tint is not None and prop == unreal.MaterialProperty.MP_BASE_COLOR:
            color = editing.create_material_expression(
                material, unreal.MaterialExpressionConstant3Vector, -320, 180
            )
            color.set_editor_property("constant", tint)
            expression = editing.create_material_expression(
                material, unreal.MaterialExpressionMultiply, -125, 0
            )
            if (not editing.connect_material_expressions(sample, output, expression, "A")
                    or not editing.connect_material_expressions(color, "", expression, "B")):
                raise RuntimeError(f"Could not tint {path}.")
            output = ""
        if not editing.connect_material_property(expression, output, prop):
            raise RuntimeError(f"Could not connect {texture.get_name()} to {path}.")
    for index, (value, target) in enumerate(scalars):
        constant = editing.create_material_expression(
            material, unreal.MaterialExpressionConstant, -320, (len(textures) + index) * 240
        )
        constant.set_editor_property("r", value)
        if not editing.connect_material_property(constant, "", target):
            raise RuntimeError(f"Could not set {target} for {path}.")
    editing.recompile_material(material)
    if world_repeat_cm is not None:
        assets.set_metadata_tag(material, "CrossroadsProjection", "1")
    if not assets.save_loaded_asset(material):
        raise RuntimeError(f"Could not save {path}.")
    return material


def import_surfaces():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    sampler = unreal.MaterialSamplerType
    prop = unreal.MaterialProperty
    for role in ("Ground036", "Asphalt021", "Concrete034"):
        source = SOURCE / "ambientCG" / role
        color = import_asset(source / f"{role}_Color.jpg", SURFACES, unreal.Texture2D)
        normal = import_asset(source / f"{role}_NormalDX.jpg", SURFACES, unreal.Texture2D)
        roughness = import_asset(source / f"{role}_Roughness.jpg", SURFACES, unreal.Texture2D)
        normal.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        normal.set_editor_property("srgb", False)
        roughness.set_editor_property("srgb", False)
        if not assets.save_loaded_asset(normal) or not assets.save_loaded_asset(roughness):
            raise RuntimeError(f"Could not save texture settings for {role}.")
        make_material(SURFACES, f"M_{role}", (
            (color, sampler.SAMPLERTYPE_COLOR, "RGB", prop.MP_BASE_COLOR),
            (normal, sampler.SAMPLERTYPE_NORMAL, "RGB", prop.MP_NORMAL),
            (roughness, sampler.SAMPLERTYPE_LINEAR_COLOR, "R", prop.MP_ROUGHNESS),
        ))


def import_building():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    editing = unreal.MaterialEditingLibrary
    prop = unreal.MaterialProperty

    timber_path = TOOLS.parent / "Saved" / "GeneratedArt" / "T_TimberPlanks.png"
    if not assets.does_asset_exist(f"{BUILDING}/{timber_path.stem}"):
        generate = runpy.run_path(str(TOOLS / "build_timber_texture.py"))
        generate["write_timber_texture"](timber_path)
    texture = import_asset(timber_path, BUILDING, unreal.Texture2D)
    timber = make_material(BUILDING, "M_CrossroadsTimber", (
        (texture, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "RGB", prop.MP_BASE_COLOR),
    ))
    stone = assets.load_asset(f"{SURFACES}/M_Concrete034")

    ghost_path = f"{BUILDING}/M_BuildPlan"
    ghost = assets.load_asset(ghost_path) if assets.does_asset_exist(ghost_path) else None
    if ghost is None:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        ghost = tools.create_asset("M_BuildPlan", BUILDING, unreal.Material, unreal.MaterialFactoryNew())
        if ghost is None:
            raise RuntimeError(f"Could not create {ghost_path}.")
        ghost.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        ghost.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        color = editing.create_material_expression(ghost, unreal.MaterialExpressionConstant3Vector, -300, 0)
        color.set_editor_property("constant", unreal.LinearColor(0.04, 0.65, 1.0, 1.0))
        opacity = editing.create_material_expression(ghost, unreal.MaterialExpressionConstant, -300, 180)
        opacity.set_editor_property("r", 0.32)
        if (not editing.connect_material_property(color, "", prop.MP_EMISSIVE_COLOR)
                or not editing.connect_material_property(opacity, "", prop.MP_OPACITY)):
            raise RuntimeError(f"Could not wire {ghost_path}.")
        editing.recompile_material(ghost)

    for material in (timber, stone, ghost):
        if not isinstance(material, unreal.Material):
            raise RuntimeError("A required construction material could not be loaded.")
        if not material.get_editor_property("used_with_instanced_static_meshes"):
            material.set_editor_property("used_with_instanced_static_meshes", True)
            editing.recompile_material(material)
        if not assets.save_loaded_asset(material):
            raise RuntimeError(f"Could not save {material.get_path_name()}.")

    path = f"{BUILDING}/M_DefenseSteel"
    if not assets.does_asset_exist(path):
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        steel = tools.create_asset("M_DefenseSteel", BUILDING, unreal.Material, unreal.MaterialFactoryNew())
        if steel is None:
            raise RuntimeError(f"Could not create {path}.")
        color = editing.create_material_expression(steel, unreal.MaterialExpressionConstant3Vector, -300, 0)
        color.set_editor_property("constant", unreal.LinearColor(0.075, 0.10, 0.12, 1.0))
        if not editing.connect_material_property(color, "", prop.MP_BASE_COLOR):
            raise RuntimeError(f"Could not wire {path}.")
        editing.recompile_material(steel)
        if not assets.save_loaded_asset(steel):
            raise RuntimeError(f"Could not save {path}.")


def import_inventory_icons():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    directory = ART + "/Inventory"
    generate = runpy.run_path(str(TOOLS / "build_item_icons.py"))["write_item_icons"]
    for source in generate(TOOLS.parent / "Saved" / "GeneratedArt" / "Inventory"):
        texture = import_asset(source, directory, unreal.Texture2D)
        texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        if not assets.save_loaded_asset(texture):
            raise RuntimeError(f"Could not save {texture.get_path_name()}.")


def import_local_medicine_sheet():
    sheet = TOOLS.parent / "SourceArt" / "Local" / "MedicineSheet.png"
    if sheet.is_file():
        texture = import_asset(sheet, ART + "/Local", unreal.Texture2D, replace_existing=True)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        if not assets.save_loaded_asset(texture):
            raise RuntimeError("Could not save the local medicine icons.")


def import_skeletal_pack(directory, names, source, prepared=None, generator=None, *generator_inputs):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if all(assets.does_asset_exist(f"{directory}/{name}") for name in names):
        return

    if generator is not None:
        runpy.run_path(str(TOOLS / f"{generator}.py"))[generator](source, prepared, *generator_inputs)
        source = prepared
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", directory)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_objects()

    for name in names:
        path = f"{directory}/{name}"
        if assets.does_asset_exist(path):
            continue
        if name == names[0]:
            matching = [item for item in imported if isinstance(item, unreal.SkeletalMesh)]
        else:
            matching = [item for item in imported
                        if isinstance(item, unreal.AnimSequence) and re.search(rf"{re.escape(name)}(?:$|_)", item.get_name(), re.IGNORECASE)]
        if len(matching) != 1 or not assets.rename_loaded_asset(matching[0], path):
            raise RuntimeError(f"Could not resolve {name} from {source.name}.")

    mesh = assets.load_asset(f"{directory}/{names[0]}")
    skeleton = mesh.get_editor_property("skeleton")
    for name in names[1:]:
        clip = assets.load_asset(f"{directory}/{name}")
        if clip.get_editor_property("skeleton") != skeleton:
            raise RuntimeError(f"{name} does not use the imported skeleton.")
    if not assets.save_directory(directory, only_if_is_dirty=True):
        raise RuntimeError(f"Could not save {directory}.")


def apply_blockout_surfaces():
    roles = ("Ground036", "Asphalt021", "Concrete034")
    materials = [(unreal.Name(f"CrossroadsSurface_{role}"), unreal.load_asset(f"{SURFACES}/M_{role}"))
                 for role in roles]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh = component.get_editor_property("static_mesh")
        if not mesh or component.get_material(0) != mesh.get_material(0):
            continue
        for tag, material in materials:
            if material and actor.actor_has_tag(tag):
                component.set_material(0, material)
                break


def prepare_legacy_map():
    """Build the original standalone defense test map on explicit request."""
    runpy.run_path(str(TOOLS / "build_crossroads.py"), run_name="__main__")
    runpy.run_path(str(TOOLS / "place_crossroads_supplies.py"), run_name="__main__")
    runpy.run_path(str(TOOLS / "place_crossroads_pois.py"), run_name="__main__")
    runpy.run_path(str(TOOLS / "place_crossroads_structures.py"), run_name="__main__")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}.")

    import_surfaces()
    apply_blockout_surfaces()
    if not levels.save_current_level():
        raise RuntimeError(f"Could not save the surfaces in {LEVEL}.")


def import_sidearm():
    generated = TOOLS.parent / "Saved" / "GeneratedArt"
    sidearm = SOURCE / "Weapons" / "SM_CrossroadsSidearm.glb"
    import_skeletal_pack(VIEWMODEL,
        ("SK_SidearmViewmodel", "ViewmodelIdle", "ViewmodelWalk", "ViewmodelFire", "ViewmodelReload", "ViewmodelReloadEmpty"),
        SOURCE / "Weapons" / "fps_pistol_animations.glb", generated / "AlignedSidearm.glb", "prepare_sidearm_glb",
        sidearm, SOURCE / "Weapons" / "SM_Magazine.glb")
    import_asset(sidearm, BUILDING, unreal.StaticMesh)
    shot = SOURCE / "Audio" / "SW_SidearmShot.wav"
    if shot.is_file():
        import_asset(shot, WEAPON_AUDIO, unreal.SoundWave, replace_existing=True)


def import_rifle():
    sniper = ART + "/SniperSurface"
    import_skeletal_pack(sniper, ("SK_SniperViewmodel", "SniperHold", "SniperFire", "SniperReload"),
                         SOURCE / "Weapons" / "SK_SniperViewmodel.glb")
    world_rifle = SOURCE / "Weapons" / "SM_CrossroadsSniper.glb"
    import_asset(world_rifle, sniper, unreal.StaticMesh)


def import_characters():
    generated = TOOLS.parent / "Saved" / "GeneratedArt"
    import_skeletal_pack(PLAYER, (
        "SK_Survivor", "SurvivorIdle", "SurvivorWalk", "SurvivorRun", "SurvivorJump",
        "SurvivorCrouchIdle", "SurvivorCrouchMove", "SurvivorProneIdle", "SurvivorProneMove",
    ), SOURCE / "Survivor" / "SK_Survivor.glb")
    import_skeletal_pack(ZOMBIE,
        ("SK_StationZombie", "StationZombieIdle", "StationZombieWalk", "StationZombieAttack"),
        SOURCE / "Enemy" / "zombie_rigged__animated.glb", generated / "StationZombie.glb", "prepare_zombie_glb")


def import_interface():
    import_inventory_icons()
    for icon in sorted((SOURCE / "Interface").glob("T_UI_*.png")):
        import_asset(icon, ART + "/Interface", unreal.Texture2D)
    if not os.environ.get("SURVIVAL_PREPARED_MARKER"):
        import_local_medicine_sheet()


def build_region():
    import_surfaces()
    import_building()
    prepare_region = runpy.run_path(str(TOOLS / "prepare_region.py"))["prepare_region"]
    return prepare_region(TOOLS, unreal.load_asset(f"{SURFACES}/M_Ground036"))


def prepare():
    failures = []
    for label, action in (("sidearm", import_sidearm), ("rifle", import_rifle),
                          ("characters", import_characters), ("interface", import_interface)):
        unreal.log(f"Crossroads: importing {label}.")
        try:
            action()
        except Exception as error:
            failures.append(f"{label}: {error}")
            unreal.log_error(traceback.format_exc())
    unreal.log("Crossroads: baking the region and saving its external actors.")
    map_path = build_region()
    if failures:
        raise RuntimeError("Incomplete Crossroads imports: " + ", ".join(failures))
    if marker := os.environ.get("SURVIVAL_PREPARED_MARKER"):
        Path(marker).write_text(map_path, encoding="utf-8")


if __name__ == "__main__":
    prepare()
