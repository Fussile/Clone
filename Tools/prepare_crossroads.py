"""Import the starter art and prepare the editable playtest map in UE5.8."""

import os
from pathlib import Path
import runpy

import unreal


TOOLS = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / "Tools"
SOURCE = TOOLS.parent / "SourceArt" / "Crossroads"
ART = "/Game/Art/Crossroads"
SURFACES = ART + "/Surfaces"
SURVIVOR = ART + "/Survivor"
BUILDING = ART + "/Building"
LEVEL = "/Game/Maps/L_SouthernCrossroads"


def import_asset(source, directory, asset_type, options=None):
    name = source.stem
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    path = f"{directory}/{name}"
    asset = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if asset is None:
        if not source.is_file():
            raise FileNotFoundError(source)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", directory)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
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
        if not assets.save_directory(directory, only_if_is_dirty=True, recursive=False):
            raise RuntimeError(f"Could not save imported assets in {directory}.")
    if not isinstance(asset, asset_type):
        raise RuntimeError(f"{path} has an unexpected asset type.")
    return asset


def make_material(directory, name, textures, tint=None):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    path = f"{directory}/{name}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(name, directory, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"Could not create {path}.")
    editing = unreal.MaterialEditingLibrary
    for index, (texture, sampler, output, prop) in enumerate(textures):
        sample = editing.create_material_expression(
            material, unreal.MaterialExpressionTextureSample, -320, index * 240
        )
        sample.set_editor_property("texture", texture)
        sample.set_editor_property("sampler_type", sampler)
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
    editing.recompile_material(material)
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


def import_survivor():
    source = SOURCE / "Kenney"
    mesh_options = unreal.FbxImportUI()
    mesh_options.set_editor_property("automated_import_should_detect_type", False)
    mesh_options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    mesh_options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    mesh_options.set_editor_property("import_mesh", True)
    mesh_options.set_editor_property("import_as_skeletal", True)
    mesh_options.set_editor_property("import_materials", False)
    mesh_options.set_editor_property("import_textures", False)
    mesh_options.set_editor_property("import_animations", False)
    mesh = import_asset(source / "Model" / "characterMedium.fbx", SURVIVOR, unreal.SkeletalMesh, mesh_options)
    skin = import_asset(source / "Skins" / "survivorMaleB.png", SURVIVOR, unreal.Texture2D)
    material = make_material(SURVIVOR, "M_Survivor", (
        (skin, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "RGB", unreal.MaterialProperty.MP_BASE_COLOR),
    ))
    zombie = make_material(SURVIVOR, "M_StationZombie", (
        (skin, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "RGB", unreal.MaterialProperty.MP_BASE_COLOR),
    ), tint=unreal.LinearColor(0.47, 0.95, 0.38, 1.0))
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for visual in (material, zombie):
        unreal.MaterialEditingLibrary.set_material_usage(visual, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        if not assets.save_loaded_asset(visual):
            raise RuntimeError(f"Could not save {visual.get_path_name()}.")

    materials = mesh.get_editor_property("materials")
    if not materials:
        raise RuntimeError("The survivor mesh has no material slots.")
    for slot in materials:
        slot.set_editor_property("material_interface", material)
    mesh.set_editor_property("materials", materials)
    if not assets.save_loaded_asset(mesh):
        raise RuntimeError("Could not save the survivor material assignment.")

    anim_options = unreal.FbxImportUI()
    anim_options.set_editor_property("automated_import_should_detect_type", False)
    anim_options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_ANIMATION)
    anim_options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    anim_options.set_editor_property("import_mesh", False)
    anim_options.set_editor_property("import_animations", True)
    anim_options.set_editor_property("skeleton", mesh.get_editor_property("skeleton"))
    for clip in ("idle", "run", "jump"):
        anim_options.set_editor_property("override_animation_name", clip)
        import_asset(source / "Animations" / f"{clip}.fbx", SURVIVOR, unreal.AnimSequence, anim_options)


def import_building():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    editing = unreal.MaterialEditingLibrary
    prop = unreal.MaterialProperty

    timber_path = TOOLS.parent / "Saved" / "GeneratedArt" / "T_TimberPlanks.png"
    if not assets.does_asset_exist(f"{BUILDING}/{timber_path.stem}"):
        generate = runpy.run_path(str(TOOLS / "build_timber_texture.py"))
        generate["write_timber_texture"](timber_path)
    texture = import_asset(timber_path, BUILDING, unreal.Texture2D)
    timber = make_material(BUILDING, "M_Timber", (
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
        editing.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
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


def prepare():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    unreal.SystemLibrary.execute_console_command(
        editor.get_editor_world(), "Interchange.FeatureFlags.Import.FBX 0"
    )
    import_surfaces()
    import_survivor()
    import_building()
    sidearm = SOURCE / "Weapons" / "SM_CrossroadsSidearm.glb"
    if sidearm.is_file():
        import_asset(sidearm, BUILDING, unreal.StaticMesh)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if assets.does_asset_exist(LEVEL):
        world = editor.get_editor_world()
        if world.get_path_name().split(".")[0] != LEVEL and not levels.load_level(LEVEL):
            raise RuntimeError(f"Could not open {LEVEL}.")
    else:
        runpy.run_path(str(TOOLS / "build_crossroads.py"), run_name="__main__")
    runpy.run_path(str(TOOLS / "place_crossroads_supplies.py"), run_name="__main__")
    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}.")
    if marker := os.environ.get("SURVIVAL_PREPARED_MARKER"):
        Path(marker).write_text(LEVEL, encoding="utf-8")


prepare()
