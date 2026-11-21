"""In Unreal Editor, place the baked Crossroads region around an imported Landscape."""
import json
from pathlib import Path

import unreal

LEVEL = "/Game/Maps/L_CrossroadsRegion"
MASS_HEIGHT = {
    "hospital": 22, "highrise": 58, "garage": 14, "civic": 17, "police": 11,
    "industry": 10, "airport": 12, "shops": 5, "clinic": 5, "services": 5,
    "suburbs": 6, "ranch": 5, "village": 5,
}


def place():
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    plan = json.loads((root / "Saved" / "WorldBake" / "world.json").read_text(encoding="utf-8"))
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor.get_editor_world()
    if not world or world.get_path_name().split(".")[0] != LEVEL:
        raise RuntimeError(f"Open {LEVEL} before placing the region.")
    existing = actors.get_all_level_actors()
    landscapes = [actor for actor in existing if isinstance(actor, unreal.LandscapeProxy)]
    landscape_root = next((actor for actor in landscapes if isinstance(actor, unreal.Landscape)), None)
    if not landscape_root:
        raise RuntimeError("Import the baked heightmap before placing the region.")

    # Derive the top-left heightmap sample from the loaded Landscape bounds.
    bounds = [actor.get_actor_bounds(False) for actor in landscapes]
    west = min(origin.x - extent.x for origin, extent in bounds)
    north = min(origin.y - extent.y for origin, extent in bounds)
    east = max(origin.x + extent.x for origin, extent in bounds)
    south = max(origin.y + extent.y for origin, extent in bounds)
    width, depth = plan["size_m"]
    if abs(east - west - width*100) > 400 or abs(south - north - depth*100) > 400:
        raise RuntimeError("Load the entire Landscape in the World Partition editor and check its XY scale.")
    z_zero = landscape_root.get_actor_location().z
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if not cube:
        raise RuntimeError("Could not load the engine blockout cube.")

    actor_tags = {str(tag) for actor in existing for tag in actor.get_editor_property("tags")}

    def spawn(kind, key, label, x, y, z, configure, tags=(), always_loaded=False):
        if key in actor_tags:
            return
        pos = unreal.Vector(west + x*100, north + (depth-y)*100, z_zero + z*100)
        actor = actors.spawn_actor_from_class(kind, pos)
        if not actor:
            raise RuntimeError(f"Could not place {label}.")
        try:
            configure(actor)
            actor.set_actor_label(label)
            actor.set_editor_property("tags", [unreal.Name(name) for name in (key, *tags)])
            if always_loaded:
                actor.set_editor_property("is_spatially_loaded", False)
        except Exception:
            actors.destroy_actor(actor)
            raise
        actor_tags.add(key)

    def box(key, label, x, y, z, w, d, h, ground=False):
        def configure(actor):
            mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh.set_static_mesh(cube)
            mesh.set_collision_profile_name("BlockAll")
            actor.set_actor_scale3d(unreal.Vector(w, d, h))
        spawn(unreal.StaticMeshActor, key, label, x, y, z + h/2, configure,
              ("StructuralGround",) if ground else ())

    has_start = any(isinstance(actor, unreal.PlayerStart) for actor in existing)
    for landscape in landscapes:
        tags = list(landscape.get_editor_property("tags"))
        ground = unreal.Name("StructuralGround")
        if ground not in tags:
            landscape.set_editor_property("tags", [*tags, ground])

    sx, sy = plan["downtown_xy_m"]
    for tag, y in (("CrossroadsClimateNorth", depth), ("CrossroadsClimateCity", sy), ("CrossroadsClimateSouth", 0)):
        spawn(unreal.TargetPoint, tag, tag, sx, y, 0, lambda actor: None, always_loaded=True)

    for site in plan["sites"]:
        key = f'CrossroadsRegion_{site["id"]}'
        if "footprint_m" not in site:
            continue
        x, y = site["plot_xy_m"]
        w, d = site["footprint_m"]
        z = site["foundation_z_m"]
        height = MASS_HEIGHT[site["kind"]]
        name = site["name"]
        box(key+"_slab", name+" slab", x, y, z-.6, w, d, .8, ground=True)
        # Leave a walkable pad at the front, including the initial spawn.
        box(key+"_shell", name+" blockout", x, y+d*.12, z+.2, w*.85, d*.59, height)
        if site["id"] == "south-services" and not has_start:
            spawn(unreal.PlayerStart, key+"_start", "Start at Southern Crossroads",
                  x, y-d*.31, z+1.3, lambda actor: None, always_loaded=True)

    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}.")
    unreal.log(f"Stamped {LEVEL}. Save external actors with Save All.")


place()
