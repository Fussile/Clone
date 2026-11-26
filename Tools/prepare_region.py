"""Build the default region map from project sources in the Unreal Editor."""
import ast
import hashlib
import importlib
import json
from pathlib import Path
import runpy
import sys

import unreal

LEVEL = "/Game/Maps/L_CrossroadsRelief"


def stage_revision(project, sources, packs=(), shared_recipe=b""):
    digest = hashlib.sha256(shared_recipe)
    for path in sorted(set(sources)):
        digest.update(path.relative_to(project).as_posix().encode())
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024*1024), b""):
                digest.update(chunk)
    for pack in packs:
        for path in sorted((project/"Content"/pack).rglob("*.uasset")):
            digest.update(path.relative_to(project).as_posix().encode())
            stat = path.stat()
            digest.update(f"{stat.st_size}:{stat.st_mtime_ns}".encode())
    return digest.hexdigest()


def prepare_region(tools, ground_material, import_asset, make_material, force_structures=False, force_biomes=False):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    current = editor.get_editor_world()
    exists = assets.does_asset_exist(LEVEL)
    if not current or current.get_path_name().split(".")[0] != LEVEL:
        if not (levels.load_level(LEVEL) if exists else levels.new_level(LEVEL, True)):
            raise RuntimeError(f"Could not open or create {LEVEL}.")
    def load_partition_actors():
        partition = unreal.WorldPartitionBlueprintLibrary
        descriptors = partition.get_actor_descs()
        if descriptors:
            partition.load_actors([actor.guid for actor in descriptors])

    project = tools.parent
    output = project/"Saved"/"WorldBake"
    output.mkdir(parents=True, exist_ok=True)
    world_sources = project/"SourceArt"/"Crossroads"
    structural_tools = [
        *tools.glob("place_region*.py"), *tools.glob("*_layout.py"), tools/"region_route.py",
        tools/"world_layout.json", tools/"city_pavement.py", tools/"place_rail.py",
        tools/"generate_world.py", tools/"local_world_art.py", tools/"survey_access.py",
        tools/"world_prop_materials.py", tools/"warehouse_doors.json",
        tools/"prepare_region.py",
        project/"Source"/"SurvivalWorldEditor"/"Private"/"CrossroadsWorldBake.cpp",
    ]
    structural_tools.remove(tools/"biome_layout.py")
    for folder in ("World", "Building", "ambientCG"):
        structural_tools.extend(p for p in (world_sources/folder).rglob("*") if p.is_file())
    # Only changes to the shared world import helpers invalidate structures. Character,
    # weapon or item import changes in the same module can use the saved city.
    shared = ast.parse((tools/"prepare_crossroads.py").read_text(encoding="utf-8"))
    world_functions = {"import_asset","make_material","import_surfaces","import_building"}
    shared_recipe = "".join(ast.dump(n) for n in shared.body if isinstance(n,ast.FunctionDef)
                            and n.name in world_functions).encode()
    structure_rev = stage_revision(project, structural_tools, ("CitySampleBuildings","OvergrownFactoryENV"), shared_recipe)
    biome_sources = world_sources/"Biomes"
    biome_rev = stage_revision(project,[tools/"biome_layout.py",tools/"place_biomes.py",
                                        *biome_sources.glob("*.glb"),*biome_sources.glob("*.jpg")])
    structure_tag = "CrossroadsStructures_"+structure_rev
    biome_tag = "CrossroadsBiomes_"+biome_rev
    def ready_marker():
        return next((a for a in actors.get_all_level_actors()
                     if a.actor_has_tag(unreal.Name("CrossroadsRegionReady"))),None)

    ready = ready_marker()
    def completed(tag):
        return ready and ready.actor_has_tag(unreal.Name(tag)) and (output/tag).is_file()

    structures_needed = force_structures or not completed(structure_tag)
    biomes_needed = structures_needed or force_biomes or not completed(biome_tag)
    if not biomes_needed:
        unreal.log("Crossroads: reusing saved structures and vegetation.")
        return LEVEL

    if exists:
        load_partition_actors()
    sys.path.insert(0, str(tools))
    try:
        generator = importlib.import_module("generate_world")
        biomes = importlib.import_module("place_biomes")
        plan, height = generator.generate(1729)
    finally:
        sys.path.remove(str(tools))
    (output/"world.json").write_text(json.dumps(plan, indent=2)+"\n",encoding="utf-8")

    raw = output/"crossroads-height.r16"
    terrain = plan["terrain"]
    has_landscape = any(isinstance(a, unreal.Landscape) for a in actors.get_all_level_actors())
    # Existing terrain retains its imported samples. A new map must use the current recipe.
    signature = hashlib.sha256()
    for name in ("generate_world.py","world_layout.json","region_route.py","airfield_layout.py",
                 "quarantine_layout.py","communications_layout.py","civic_landmarks.py","rail_layout.py"):
        signature.update((tools/name).read_bytes())
    revision = signature.hexdigest()
    stamp = output/"Terrain.sha256"
    if (not raw.is_file() or raw.stat().st_size != 2*terrain["resolution"]**2 or
            (not has_landscape and (not stamp.is_file() or stamp.read_text(encoding="utf-8") != revision))):
        generator.write_heightmap(output/"crossroads-height.png",plan,height,raw)
        stamp.write_text(revision,encoding="utf-8")
    if not has_landscape:
        if not unreal.CrossroadsWorldBake.import_terrain(
                str(raw),terrain["resolution"],unreal.Vector(*terrain["xyz_scale_cm"]),ground_material):
            raise RuntimeError("Failed to import the Crossroads terrain.")
        load_partition_actors()

    existing = actors.get_all_level_actors()

    def light(kind, tag, setup):
        if any(actor.actor_has_tag(unreal.Name(tag)) for actor in existing):
            return
        actor = actors.spawn_actor_from_class(kind, unreal.Vector(0, 0, 1800))
        if not actor:
            raise RuntimeError(f"Could not place {tag}.")
        actor.set_actor_label(tag)
        actor.set_editor_property("tags", [unreal.Name(tag)])
        actor.set_editor_property("is_spatially_loaded", False)
        setup(actor)

    def directional(actor, index, intensity):
        component = actor.get_component_by_class(unreal.DirectionalLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("atmosphere_sun_light", True)
        component.set_editor_property("atmosphere_sun_light_index", index)
        component.set_editor_property("intensity", intensity)
        actor.set_actor_rotation(unreal.Rotator(pitch=-40 if index == 0 else 40, yaw=-45), False)

    def sky_light(actor):
        component = actor.get_component_by_class(unreal.SkyLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("real_time_capture", True)

    light(unreal.DirectionalLight, "CrossroadsSun", lambda actor: directional(actor, 0, 6.0))
    light(unreal.DirectionalLight, "CrossroadsMoon", lambda actor: directional(actor, 1, .25))
    light(unreal.SkyLight, "CrossroadsSkyLight", sky_light)
    light(unreal.SkyAtmosphere, "CrossroadsAtmosphere", lambda actor: None)
    def checkpoint(prefix, tag):
        previous = [t for t in ready.tags if not str(t).startswith(prefix)]
        ready.set_editor_property("tags", [*previous,unreal.Name(tag)])
        if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
            raise RuntimeError(f"Could not save the {prefix} stage.")
        (output/tag).write_text("saved",encoding="ascii")

    if structures_needed:
        runpy.run_path(str(tools/"place_region_blockout.py"),run_name="__main__")
        ready = ready_marker()
        if not ready:
            raise RuntimeError("Structural bake finished without its completion marker.")
        # Invalidate vegetation first. A failed landscape pass can then retry from this checkpoint.
        ready.set_editor_property("tags",[t for t in ready.tags if not str(t).startswith("CrossroadsBiomes_")])
        checkpoint("CrossroadsStructures_",structure_tag)
    else:
        unreal.log("Crossroads: reusing saved buildings, roads and interior placements.")
    biomes.place(plan,raw,output,biome_sources,import_asset,make_material)
    checkpoint("CrossroadsBiomes_",biome_tag)
    return LEVEL
