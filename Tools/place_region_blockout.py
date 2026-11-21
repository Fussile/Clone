"""In Unreal Editor, place the baked Crossroads region around an imported Landscape."""
import json
import runpy
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

    actor_tags = {str(tag): actor for actor in existing for tag in actor.get_editor_property("tags")}
    import_asset = runpy.run_path(str(root / "Tools" / "prepare_crossroads.py"))["import_asset"]
    bed_mesh = import_asset(root / "SourceArt" / "Crossroads" / "World" / "SM_HospitalBed.glb",
                            "/Game/Art/Crossroads/World", unreal.StaticMesh)
    bay_mesh = import_asset(root / "SourceArt" / "Crossroads" / "World" / "SM_DowntownWindowBay.glb",
                            "/Game/Art/Crossroads/World", unreal.StaticMesh)
    crate_class = unreal.load_class(None, "/Script/Survival.SurvivalSupplyCrate")
    if not crate_class:
        raise RuntimeError("Build SurvivalEditor before placing hospital loot.")

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
        actor_tags[key] = actor

    def box(key, label, x, y, z, w, d, h, ground=False):
        def configure(actor):
            mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh.set_static_mesh(cube)
            mesh.set_collision_profile_name("BlockAll")
            actor.set_actor_scale3d(unreal.Vector(w, d, h))
        spawn(unreal.StaticMeshActor, key, label, x, y, z + h/2, configure,
              ("StructuralGround",) if ground else ())

    def place_hospital(key, x, y, floor):
        def wall(label, dx, dy, w, d):
            box(f"{key}_{label}", f"Hospital {label}", x+dx, y+dy, floor, w, d, 3.2)

        # First wing measures 18 by 14 meters, with a 3 m south entrance.
        wall("west wall", -8.875, 0, .25, 14)
        wall("east wall", 8.875, 0, .25, 14)
        wall("rear wall", 0, 6.875, 17.5, .25)
        for sign, side in ((-1, "west"), (1, "east")):
            wall(f"{side} entrance wall", sign*5.125, -6.875, 7.25, .25)
            for index, (center, length) in enumerate(((-4.75, 4.5), (0, 2), (4.75, 4.5))):
                wall(f"{side} hall wall {index}", sign*2.1, center, .25, length)
            wall(f"{side} room divider", sign*5.425, 0, 6.4, .25)
        box(f"{key}_roof", "Hospital wing roof", x, y, floor+3.2, 18, 14, .3)

        def furnish_bed(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(bed_mesh)
            component.set_collision_profile_name("NoCollision")

        for index, (dx, dy) in enumerate(((-5, -3), (5, -3), (-5, 3), (5, 3)), 1):
            spawn(unreal.StaticMeshActor, f"{key}_bed_{index}", f"Hospital bed {index}",
                  x+dx, y+dy, floor, furnish_bed)

        def fill_crate(actor):
            for name, amount in (("timber_amount", 0), ("stone_amount", 0), ("cloth_amount", 5),
                                 ("dressing_schematic_amount", 1)):
                actor.set_editor_property(name, amount)

        spawn(crate_class, f"{key}_supplies", "Hospital dressing supplies", x+5, y+5.6, floor+.41, fill_crate)

        # Remove the previous generated mass only after the wing is ready.
        if old_shell := actor_tags.get(f"{key}_shell"):
            if not actors.destroy_actor(old_shell):
                raise RuntimeError("Could not remove the old hospital blockout.")

    def dress_downtown(key, x, y, floor):
        bounds = bay_mesh.get_bounding_box()
        size_x = bounds.max.x - bounds.min.x
        size_y = bounds.max.y - bounds.min.y
        width = max(size_x, size_y) / 100
        yaw = 0 if size_x >= size_y else 90
        # Clear the existing wall so the trim reads from both sides of the street.
        wall_offset = max(abs(bounds.min.y), abs(bounds.max.y)) if yaw == 0 else max(
            abs(bounds.min.x), abs(bounds.max.x))
        street_edge = y - wall_offset / 100 - .02

        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(bay_mesh)
            component.set_collision_profile_name("NoCollision")
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)

        for index in range(4):
            spawn(unreal.StaticMeshActor, f"{key}_bay_{index}", f"Downtown window bay {index + 1}",
                  x + (index - 1.5) * width, street_edge, floor, configure)

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
        if site["id"] == "city-hospital":
            place_hospital(key, x, y, z+.2)
        else:
            box(key+"_shell", name+" blockout", x, y+d*.12, z+.2, w*.85, d*.59, height)
        if site["id"] == "city-nightlife":
            dress_downtown(key, x, y + d*.12 - d*.59/2, z+.2)
        if site["id"] == "south-services" and not has_start:
            spawn(unreal.PlayerStart, key+"_start", "Start at Southern Crossroads",
                  x, y-d*.31, z+1.3, lambda actor: None, always_loaded=True)

    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}.")
    unreal.log(f"Stamped {LEVEL}. Save external actors with Save All.")


place()
