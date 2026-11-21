"""In Unreal Editor, place the baked Crossroads region around an imported Landscape."""
import json
import math
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
    if plan.get("version", 0) < 4:
        raise RuntimeError("Rebake Saved/WorldBake/world.json with Tools/generate_world.py before stamping the M4 route.")
    site_tools = runpy.run_path(str(root / "Tools" / "place_region_sites.py"))
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
    prepare_art = runpy.run_path(str(root / "Tools" / "prepare_crossroads.py"))
    import_asset = prepare_art["import_asset"]
    world_art = "/Game/Art/Crossroads/World"
    source_art = root / "SourceArt" / "Crossroads" / "World"
    bed_mesh, bay_mesh, warehouse_mesh, housing_mesh = (
        import_asset(source_art / f"SM_{name}.glb", world_art, unreal.StaticMesh)
        for name in ("HospitalBed", "DowntownWindowBay", "WarehouseExterior", "PanelHousingExterior")
    )
    crate_class = unreal.load_class(None, "/Script/Survival.SurvivalSupplyCrate")
    prefab_class = unreal.load_class(None, "/Script/Survival.SurvivalStructurePrefab")
    zombie_class = unreal.load_class(None, "/Script/Survival.SurvivalZombie")
    if not crate_class or not prefab_class or not zombie_class:
        raise RuntimeError("Build SurvivalEditor before placing structures and loot.")
    surface_path = "/Game/Art/Crossroads/Surfaces"
    asphalt = unreal.load_asset(f"{surface_path}/M_Asphalt021")
    concrete = unreal.load_asset(f"{surface_path}/M_Concrete034")

    # Simple blockout colors keep the styles legible while detailed modules are sourced.
    colors = {
        "late_soviet": (.28, .26, .23), "brick_hardwood": (.30, .115, .065),
        "stucco_metal": (.84, .83, .78), "classical_civic": (.65, .61, .53),
        "modern": (.18, .25, .30), "dark_trim": (.036, .036, .035),
    }
    style_materials = {
        style: prepare_art["make_material"](world_art, f"M_Region_{style}", (),
                                           tint=unreal.LinearColor(*color, 1.0))
        for style, color in colors.items()
    }

    def spawn(kind, key, label, x, y, z, configure, tags=(), always_loaded=False):
        if key in actor_tags:
            return actor_tags[key]
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
        return actor

    def box(key, label, x, y, z, w, d, h, ground=False, material=None, hidden=False):
        def configure(actor):
            mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh.set_static_mesh(cube)
            mesh.set_collision_profile_name("BlockAll")
            actor.set_actor_scale3d(unreal.Vector(w, d, h))
            if hidden:
                actor.set_actor_hidden_in_game(True)
        actor = spawn(unreal.StaticMeshActor, key, label, x, y, z + h/2, configure,
                      ("StructuralGround",) if ground else ())
        if material:
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if component.get_material(0) == cube.get_material(0):
                component.set_material(0, material)

    def remove_blockout(key):
        if old_shell := actor_tags.get(key + "_shell"):
            if not actors.destroy_actor(old_shell):
                raise RuntimeError(f"Could not replace the old shell at {key}.")

    def exterior(key, label, mesh_asset, x, y, floor):
        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(mesh_asset)
            component.set_collision_profile_name("NoCollision")
        spawn(unreal.StaticMeshActor, key, label, x, y, floor, configure)

    def cache(key, label, x, y, floor, **loot):
        def configure(actor):
            for prop, amount in {"timber_amount": 0, "stone_amount": 0, **loot}.items():
                actor.set_editor_property(prop, amount)
        spawn(crate_class, key, label, x, y, floor+.41, configure)

    def encounter(key, label, x, y, floor, runner=False):
        def configure(actor):
            actor.set_editor_property("aggro_range", 2100.0)
            actor.set_editor_property("is_runner", runner)
        spawn(zombie_class, key, label, x, y, floor+1.03, configure)

    def place_route(route):
        width = route["width_m"]
        for index, segment in enumerate(route["segments"]):
            ax, ay, az = segment["start"]
            bx, by, bz = segment["end"]
            dx, dy, dz = bx-ax, ay-by, bz-az
            horizontal = math.hypot(dx, dy)
            length = math.hypot(horizontal, dz)
            yaw = math.degrees(math.atan2(dy, dx))
            pitch = math.degrees(math.atan2(dz, horizontal))
            bridge = segment["bridge"]
            thickness = .50 if bridge else .28
            material = concrete if bridge else asphalt
            key = f'CrossroadsRoute_{route["id"]}_{index}'
            x, y, z = (ax+bx)/2, (ay+by)/2, (az+bz)/2

            def road_actor(actor, scale, road_material):
                component = actor.get_component_by_class(unreal.StaticMeshComponent)
                component.set_static_mesh(cube)
                component.set_collision_profile_name("BlockAll")
                if road_material:
                    component.set_material(0, road_material)
                actor.set_actor_scale3d(unreal.Vector(*scale))
                actor.set_actor_rotation(unreal.Rotator(pitch=pitch, yaw=yaw), False)

            spawn(unreal.StaticMeshActor, key, "Bridge deck" if bridge else "Scavenging route",
                  x, y, z-thickness/2, lambda actor: road_actor(actor, (length+.5, width, thickness), material),
                  ("StructuralGround",))
            if bridge:
                for side in (-1, 1):
                    offset = side*(width/2-.12)
                    spawn(unreal.StaticMeshActor, f"{key}_rail_{side}", "Bridge parapet",
                          x-dy/horizontal*offset, y-dx/horizontal*offset, z+.45,
                          lambda actor: road_actor(actor, (length+.5, .24, .90), concrete))

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

        cache(f"{key}_supplies", "Hospital dressing supplies", x+5, y+5.6, floor,
              cloth_amount=5, dressing_schematic_amount=1)
        remove_blockout(key)

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
        elif site["id"] == "warehouses":
            # The exterior is separate from the working bay so damage clears the bay's actual opening.
            exterior(key+"_exterior", "Warehouse exterior", warehouse_mesh, x, y+8, z+.2)
            box(key+"_closed", "Warehouse background collision", x, y+8, z+.2, 26, 5.8, 4.2, hidden=True)
            spawn(prefab_class, key+"_loading_bay", "Destructible warehouse loading bay", x, y-6, z+.2,
                  lambda actor: site_tools["warehouse_structure"](actor, key+"_loading_bay"))
            cache(key+"_materials", "Loading bay repair materials", x+5.6, y-8, z+.2,
                  stone_amount=30, timber_amount=24)
            encounter(key+"_runner", "Loading yard runner", x-9, y-6, z+.2, runner=True)
            encounter(key+"_walker", "Warehouse walker", x+10, y+2, z+.2)
            remove_blockout(key)
        elif site["id"] == "southwest-apartments":
            exterior(key+"_exterior", "Panel apartment exterior", housing_mesh, x, y+11, z+.2)
            box(key+"_closed", "Panel apartment background collision", x, y+11, z+.2, 34, 8, 10, hidden=True)
            site_tools["apartment_layout"](box, cache, encounter, key, x, y-7, z+.2, plan["seed"])
            remove_blockout(key)
        elif site["kind"] == "services":
            site_tools["station_layout"](box, key, x, y+.5, z+.2)
            cache(key+"_provisions", "Station provisions", x+3, y+3, z+.2, food_amount=2, water_amount=3)
            if site["id"] == "south-services":
                cache(key+"_sidearm", "Station sidearm case", x+2, y+2, z+.2, contains_sidearm=True)
            remove_blockout(key)
        else:
            style = site["architecture"]
            box(key+"_shell", name+" blockout", x, y+d*.12, z+.2, w*.85, d*.59, height,
                material=style_materials[style])
            if style == "stucco_metal":
                box(key+"_trim", "Dark metal roof trim", x, y+d*.12, z+.2+height,
                    w*.87, d*.61, .2, material=style_materials["dark_trim"])
            if style == "classical_civic":
                frontage = y+d*.12-d*.59/2
                for index in range(4):
                    box(f"{key}_portico_{index}", "Civic portico pier", x+w*.07*(index-1.5),
                        frontage-2.2, z+.2, .9, .9, 4.2, material=style_materials[style])
                box(key+"_portico_roof", "Civic portico entablature", x, frontage-1.2,
                    z+4.4, w*.30, 3.4, .45, material=style_materials[style])
        if site["id"] == "city-nightlife":
            dress_downtown(key, x, y + d*.12 - d*.59/2, z+.2)
        if site["id"] == "south-services" and not has_start:
            spawn(unreal.PlayerStart, key+"_start", "Start at Southern Crossroads",
                  x, y-d*.31, z+1.3, lambda actor: None, always_loaded=True)

    for route in plan["m4_route"]:
        place_route(route)

    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}.")
    unreal.log(f"Stamped {LEVEL}. Save external actors with Save All.")


place()
