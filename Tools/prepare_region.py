"""Build the default region map from project sources in the Unreal Editor."""
import hashlib
import importlib
import json
from pathlib import Path
import runpy
import sys

import unreal

LEVEL = "/Game/Maps/L_CrossroadsRelief"


def prepare_region(tools, ground_material):
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

    if exists:
        load_partition_actors()

    output = tools.parent / "Saved" / "WorldBake"
    output.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(tools))
    try:
        generator = importlib.import_module("generate_world")
        plan, height = generator.generate(1729)
    finally:
        sys.path.remove(str(tools))
    (output / "world.json").write_text(json.dumps(plan, indent=2)+"\n", encoding="utf-8")

    if not any(isinstance(actor, unreal.Landscape) for actor in actors.get_all_level_actors()):
        raw = output / "crossroads-height.r16"
        terrain = plan["terrain"]
        # Reuse the baked samples on subsequent attempts when the source layout matches.
        signature = hashlib.sha256()
        for name in ("generate_world.py", "world_layout.json", "region_route.py", "airfield_layout.py",
                     "communications_layout.py", "civic_landmarks.py"):
            signature.update((tools / name).read_bytes())
        revision = signature.hexdigest()
        stamp = output / "Terrain.sha256"
        if (not stamp.is_file() or stamp.read_text(encoding="utf-8") != revision
                or not raw.is_file() or raw.stat().st_size != 2 * terrain["resolution"]**2):
            generator.write_heightmap(output / "crossroads-height.png", plan, height, raw)
            stamp.write_text(revision, encoding="utf-8")

        if not unreal.CrossroadsWorldBake.import_terrain(
            str(raw), terrain["resolution"], unreal.Vector(*terrain["xyz_scale_cm"]), ground_material
        ):
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
    runpy.run_path(str(tools / "place_region_blockout.py"), run_name="__main__")
    if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
        raise RuntimeError(f"Could not save terrain or external actors for {LEVEL}.")
    return LEVEL
