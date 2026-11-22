"""In Unreal Editor, place the baked Crossroads region around an imported Landscape."""
import json
import math
import runpy
from pathlib import Path

import unreal

LEVEL = "/Game/Maps/L_CrossroadsRelief"
def place():
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    plan = json.loads((root / "Saved" / "WorldBake" / "world.json").read_text(encoding="utf-8"))
    if plan.get("version", 0) != 6:
        raise RuntimeError("Rebake Saved/WorldBake/world.json with Tools/generate_world.py before stamping the M4 route.")
    site_tools = runpy.run_path(str(root / "Tools" / "place_region_sites.py"))
    interiors = runpy.run_path(str(root / "Tools" / "place_region_interiors.py"))
    warehouse_doors = json.loads((root / "Tools" / "warehouse_doors.json").read_text(encoding="utf-8"))
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
    bed_mesh, bay_mesh, warehouse_mesh, shelf_mesh, desk_mesh = (
        import_asset(source_art / f"SM_{name}.glb", world_art, unreal.StaticMesh)
        for name in ("HospitalBed", "DowntownWindowBay", "WarehouseDoorwayShell",
                     "WeatheredShelf", "SchoolDesk")
    )
    crate_class = unreal.load_class(None, "/Script/Survival.SurvivalSupplyCrate")
    zombie_class = unreal.load_class(None, "/Script/Survival.SurvivalZombie")
    door_class = unreal.load_class(None, "/Script/Survival.SurvivalWorldDoor")
    if not crate_class or not zombie_class or not door_class:
        raise RuntimeError("Build SurvivalEditor before placing structures and loot.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for mesh in (warehouse_mesh, shelf_mesh, desk_mesh):
        if not unreal.CrossroadsWorldBake.set_static_mesh_collision(mesh):
            raise RuntimeError(f"Could not prepare collision for {mesh.get_name()}.")
        if not assets.save_loaded_asset(mesh, only_if_is_dirty=True):
            raise RuntimeError(f"Could not save {mesh.get_name()}.")
    surface_path = "/Game/Art/Crossroads/Surfaces"
    asphalt = unreal.load_asset(f"{surface_path}/M_Asphalt021")
    concrete = unreal.load_asset(f"{surface_path}/M_Concrete034")

    # Simple blockout colors keep the styles legible while detailed modules are sourced.
    colors = {
        "late_soviet": (.28, .26, .23), "brick_hardwood": (.30, .115, .065),
        "stucco_metal": (.84, .83, .78), "classical_civic": (.65, .61, .53),
        "modern": (.18, .25, .30), "dark_trim": (.036, .036, .035),
        "traffic": (.79, .58, .18), "petrol": (.13, .28, .29),
        "glazing": (.09, .19, .24), "linen": (.47, .44, .39), "soil": (.22, .138, .072),
    }
    style_materials = {
        style: prepare_art["make_material"](world_art, f"M_Region_{style}", (),
                                           tint=unreal.LinearColor(*color, 1.0))
        for style, color in colors.items()
    }

    style_materials["timber"] = unreal.load_asset("/Game/Art/Crossroads/Building/M_CrossroadsTimber")
    style_materials["concrete"] = concrete

    def spawn(kind, key, label, x, y, z, configure, tags=(), always_loaded=False, relocate=False):
        pos = unreal.Vector(west + x*100, north + (depth-y)*100, z_zero + z*100)
        if key in actor_tags:
            actor = actor_tags[key]
            if relocate:
                actor.set_actor_location(pos, False, False)
            return actor
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

    def box(key, label, x, y, z, w, d, h, ground=False, material=None, hidden=False,
            collision=True, rotation=None):
        def configure(actor):
            mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh.set_static_mesh(cube)
            mesh.set_collision_profile_name("BlockAll" if collision else "NoCollision")
            actor.set_actor_scale3d(unreal.Vector(w, d, h))
            if rotation:
                pitch, yaw = rotation
                actor.set_actor_rotation(unreal.Rotator(pitch=pitch, yaw=yaw), False)
            if hidden:
                actor.set_actor_hidden_in_game(True)
        actor = spawn(unreal.StaticMeshActor, key, label, x, y, z + h/2, configure,
                      ("StructuralGround",) if ground else ())
        if material:
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if component.get_material(0) == cube.get_material(0):
                component.set_material(0, material)

    def remove_generated(*keys):
        for key in keys:
            actor = actor_tags.get(key)
            if actor:
                if not actors.destroy_actor(actor):
                    raise RuntimeError(f"Could not replace generated actor {key}.")
                actor_tags.pop(key)

    def remove_blockout(key):
        remove_generated(key + "_shell")

    def ramp(key, label, x, y0, y1, floor, upper, width, material):
        direction = 1 if y1>y0 else -1
        pitch = math.atan2(upper-floor, abs(y1-y0))
        thickness = .26
        y = (y0+y1)/2 + direction*thickness*math.sin(pitch)/2
        z = (floor+upper)/2 - thickness*(1+math.cos(pitch))/2
        box(key, label, x, y, z, math.hypot(y1-y0, upper-floor), width, thickness,
            material=material, rotation=(math.degrees(pitch), -90*direction))

    def sign(key, label, x, y, z, size=38):
        def configure(actor):
            text = actor.get_component_by_class(unreal.TextRenderComponent)
            text.set_text(label)
            text.set_world_size(size)
            text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
            actor.set_actor_rotation(unreal.Rotator(yaw=90), False)
        spawn(unreal.TextRenderActor, key, label, x, y, z, configure)

    def door(key, label, x, y, floor, width, height, *, yaw=0, roll_up=False,
             reverse=False, finish="timber"):
        def configure(actor):
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)
            actor.configure_door(width*100, height*100, roll_up, reverse,
                                 style_materials[finish], style_materials["dark_trim"])
        spawn(door_class, key, label, x, y, floor, configure)

    def treatment_bed(key, label, x, y, floor):
        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(bed_mesh)
            component.set_collision_profile_name("NoCollision")
        spawn(unreal.StaticMeshActor, key, label, x, y, floor, configure)

    def furniture(key, label, kind, x, y, floor):
        mesh = {"shelf": shelf_mesh, "desk": desk_mesh}[kind]

        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(mesh)
            component.set_collision_profile_name("BlockAll")

        spawn(unreal.StaticMeshActor, key, label, x, y, floor, configure)

    def cache(key, label, x, y, floor, *, relocate=False, **loot):
        def configure(actor):
            for prop, amount in {"timber_amount": 0, "stone_amount": 0, **loot}.items():
                actor.set_editor_property(prop, amount)
        spawn(crate_class, key, label, x, y, floor+.41, configure, relocate=relocate)

    def encounter(key, label, x, y, floor, runner=False, relocate=False):
        def configure(actor):
            actor.set_editor_property("aggro_range", 2100.0)
            actor.set_editor_property("is_runner", runner)
        spawn(zombie_class, key, label, x, y, floor+1.03, configure, relocate=relocate)

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
            if "pier_base_z_m" in segment:
                base = segment["pier_base_z_m"]
                box(key+"_pier", "Bridge pier", x, y, base, 1.2, 2.4, z-thickness-base, material=concrete)
            if bridge:
                for side in (-1, 1):
                    offset = side*(width/2-.12)
                    spawn(unreal.StaticMeshActor, f"{key}_rail_{side}", "Bridge parapet",
                          x-dy/horizontal*offset, y-dx/horizontal*offset, z+.45,
                          lambda actor: road_actor(actor, (length+.5, .24, .90), concrete))

    def place_hospital(key, x, y, floor):
        def wall(label, dx, dy, w, d):
            box(f"{key}_{label}", f"Hospital {label}", x+dx, y+dy, floor, w, d, 3.2,
                material=style_materials["stucco_metal"])

        # First wing measures 18 by 14 meters, with a 3 m south entrance.
        wall("west wall", -8.875, 0, .25, 14)
        wall("east wall", 8.875, 0, .25, 14)
        wall("rear wall", 0, 6.875, 17.5, .25)
        for direction, side in ((-1, "west"), (1, "east")):
            wall(f"{side} entrance wall", direction*5.125, -6.875, 7.25, .25)
            for index, (center, length) in enumerate(((-4.75, 4.5), (0, 2), (4.75, 4.5))):
                wall(f"{side} hall wall {index}", direction*2.1, center, .25, length)
            wall(f"{side} room divider", direction*5.425, 0, 6.4, .25)
        box(f"{key}_roof", "Hospital wing roof", x, y, floor+3.2, 18, 14, .3, material=concrete)
        box(key+"_fascia", "Triage fascia", x, y-7.18, floor+2.65, 7.5, .35, .5,
            material=style_materials["petrol"])
        sign(key+"_sign", "TRIAGE", x, y-7.38, floor+3.04)
        for side in (-1, 1):
            box(f"{key}_bench_{side}", "Waiting area bench", x+side*4.5, y-6.3,
                floor, 2.1, .44, .45, material=style_materials["petrol"])
            box(f"{key}_cabinet_{side}", "Treatment room cabinet", x+side*7.86, y+4.6,
                floor, .62, 1.5, 1.08, material=style_materials["dark_trim"])
        box(key+"_reception", "Triage desk", x-.9, y+4.5, floor, 1.64, .66, 1.08,
            material=style_materials["petrol"])

        site_tools["double_door"](door, key+"_entrance", "Triage entrance",
                                  x, y-7.04, floor, 3, 2.35, "petrol")
        for side in (-1, 1):
            for room in (-1, 1):
                door(f"{key}_treatment_{side}_{room}", "Treatment room door",
                     x+side*2.1, y+room*1.75, floor, 1.5, 2.25, yaw=-90*side, finish="petrol")

        for index, (dx, dy) in enumerate(((-5, -3), (5, -3), (-5, 3), (5, 3)), 1):
            treatment_bed(f"{key}_bed_{index}", f"Hospital bed {index}", x+dx, y+dy, floor)

        cache(f"{key}_supplies", "Hospital dressing supplies", x+5, y+5.6, floor,
              cloth_amount=5, dressing_schematic_amount=1)
        remove_blockout(key)

    def storefront_window(key, x, y, floor):
        bounds = bay_mesh.get_bounding_box()
        extent_x, extent_y = bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y
        yaw = 0 if extent_x >= extent_y else 90
        depth = max(abs(bounds.min.y), abs(bounds.max.y)) if yaw == 0 else max(
            abs(bounds.min.x), abs(bounds.max.x))

        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(bay_mesh)
            component.set_collision_profile_name("NoCollision")
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)

        window = spawn(unreal.StaticMeshActor, key, "Storefront window", x, y-depth/100-.015,
                       floor+.63, configure, relocate=True)
        window.set_actor_scale3d(unreal.Vector(1, 1, .74))

    def place_warehouse(key, x, y, floor):
        bounds = warehouse_mesh.get_bounding_box()
        size_x, size_y = bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y
        if size_x < size_y:
            yaw = 90 if bounds.max.x + bounds.min.x > 0 else -90
        else:
            yaw = 0 if bounds.max.y + bounds.min.y > 0 else 180

        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(warehouse_mesh)
            component.set_collision_profile_name("BlockAll")
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)

        spawn(unreal.StaticMeshActor, key+"_doorway_shell", "Warehouse loading hall", x, y, floor, configure)
        remove_generated(key+"_warehouse", key+"_static")
        for opening in warehouse_doors:
            door(f"{key}_door_{opening['id']}", opening["label"], x+opening["x"], y+opening["y"],
                 floor, opening["width"], opening["height"], yaw=opening["yaw"],
                 roll_up=opening.get("roll_up", False), reverse=opening.get("reverse", False),
                 finish="petrol")
        site_tools["warehouse_yard"](box, key, x, y, floor, style_materials)
        box(key+"_signboard", "Warehouse signboard", x, y-3.54, floor+3.26, 5.2, .1, .6,
            material=style_materials["dark_trim"])
        sign(key+"_sign", "LOADING 04", x, y-3.61, floor+3.69)
        cache(key+"_materials", "Warehouse repair materials", x-8, y+1.5, floor,
              relocate=True, stone_amount=30, timber_amount=24)
        cache(key+"_supplies", "Warehouse storage crate", x+8, y+1.5, floor,
              relocate=True, cloth_amount=3, water_amount=2)
        encounter(key+"_runner", "Loading yard runner", x-9.4, y-7.2, floor, runner=True, relocate=True)
        encounter(key+"_walker", "Warehouse walker", x+4, y+.75, floor, relocate=True)

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
        name = site["name"]
        box(key+"_slab", name+" slab", x, y, z-.6, w, d, .8, ground=True)
        # Leave a walkable pad at the front, including the initial spawn.
        if site["id"] == "city-hospital":
            place_hospital(key, x, y, z+.2)
        elif site["id"] == "warehouses":
            place_warehouse(key, x, y, z+.2)
            remove_blockout(key)
        elif site["id"] == "city-nightlife":
            site_tools["nightlife_layout"](box, door, cache, encounter, sign, storefront_window,
                                          key, x, y, z+.2, style_materials)
            remove_generated(*(f"{key}_bay_{index}" for index in range(4)))
            remove_blockout(key)
        elif site["kind"] == "police":
            site_tools["precinct_layout"](box, door, cache, encounter, sign, key, x, y, z+.2,
                                         style_materials, "Crossroads Police" if site["id"]=="city-police" else name)
            remove_generated(key+"_portico_roof", *(f"{key}_portico_{i}" for i in range(4)))
            remove_blockout(key)
        elif site["id"] == "city-parking":
            site_tools["garage_layout"](box, door, ramp, cache, encounter, sign, key, x, y, z+.2, style_materials)
            remove_blockout(key)
        elif site["id"] == "southwest-apartments":
            site_tools["apartment_layout"](box, door, cache, encounter, key, x, y-7, z+.2, plan["seed"])
            interiors["place_site"](box, door, ramp, cache, encounter, sign, storefront_window,
                treatment_bed, furniture, site_tools["building_shell"], site_tools["double_door"], key+"_east_block",
                site, x, y+10, z+.2, 34, 12, style_materials)
            remove_generated(key+"_exterior", key+"_closed")
            remove_blockout(key)
        elif site["kind"] == "services":
            site_tools["station_layout"](box, door, key, x, y+.5, z+.2)
            site_tools["service_forecourt"](box, key, x, y+.5, z+.2, style_materials)
            cache(key+"_provisions", "Station provisions", x+3, y+3, z+.2, food_amount=2, water_amount=3)
            if site["id"] == "south-services":
                cache(key+"_sidearm", "Station sidearm case", x+2, y+2, z+.2, contains_sidearm=True)
                cache(key+"_rifle", "Station rifle locker", x+4.8, y+2.4, z+.2,
                      contains_rifle=True, builder_guide_amount=1)
                cache(key+"_building", "Station building materials", x-4.6, y+1.6, z+.2,
                      timber_amount=24, stone_amount=12)
                for index, offset in enumerate((-4, 0, 4), 1):
                    tag = f"CrossroadsDefenseSpawn{index}"
                    spawn(unreal.TargetPoint, tag, tag, x+offset, y-d*.43, z+1.32,
                          lambda actor: None, always_loaded=True)
            remove_blockout(key)
        else:
            interiors["place_site"](box, door, ramp, cache, encounter, sign, storefront_window,
                treatment_bed, furniture, site_tools["building_shell"], site_tools["double_door"], key,
                site, x, y, z+.2, w, d, style_materials)
            remove_generated(key+"_shell", key+"_trim", key+"_portico_roof",
                             *(f"{key}_portico_{i}" for i in range(4)))
        if site["id"] == "south-services" and not has_start:
            spawn(unreal.PlayerStart, key+"_start", "Start at Southern Crossroads",
                  x, y-d*.31, z+1.3,
                  lambda actor: actor.set_actor_rotation(unreal.Rotator(yaw=-90), False),
                  always_loaded=True)

    for route in plan["m4_route"]:
        place_route(route)

    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}.")
    unreal.log(f"Stamped {LEVEL}. Save external actors with Save All.")


if __name__ == "__main__":
    place()
