"""In Unreal Editor, place the baked Crossroads region around an imported Landscape."""
import json
import math
import random
import runpy
from pathlib import Path

import unreal

LEVEL = "/Game/Maps/L_CrossroadsRelief"
def place():
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    plan = json.loads((root / "Saved" / "WorldBake" / "world.json").read_text(encoding="utf-8"))
    if plan.get("version", 0) != 8:
        raise RuntimeError("Rebake Saved/WorldBake/world.json with Tools/generate_world.py before stamping the region.")
    site_tools = runpy.run_path(str(root / "Tools" / "place_region_sites.py"))
    interiors = runpy.run_path(str(root / "Tools" / "place_region_interiors.py"))
    communications = runpy.run_path(str(root / "Tools" / "communications_layout.py"))
    downtown = runpy.run_path(str(root / "Tools" / "downtown_layout.py"))
    utility = runpy.run_path(str(root / "Tools" / "utility_layout.py"))
    landmarks = runpy.run_path(str(root / "Tools" / "place_region_landmarks.py"))
    airfield_dimensions = runpy.run_path(str(root / "Tools" / "airfield_layout.py"))
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
    landmark_collision = {
        "AircraftHangar", "CommunityPitch", "RuinedTerminal", "RuinedAirportShop",
        "WeatheredBusStop", "YardTransformer", "PitchEntranceSign", "RadiationSign",
    }
    landmark_names = sorted(landmark_collision | {"RunwayMarkings"})
    landmark_meshes = {
        name: import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Landmarks/{name}", unreal.StaticMesh)
        for name in landmark_names
    }
    crate_mesh, pump_mesh = (
        import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Props/{name}", unreal.StaticMesh)
        for name in ("ScavengeCrate", "FuelPumpSolid")
    )
    communications_meshes = {
        name: import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Communications/{name}", unreal.StaticMesh)
        for name in ("RepeaterMast", "SkalkaTower", "LaitseStation", "CheckpointWall", "TimberWire", "RoadSpikes")
    }
    office_meshes = {
        name: import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Props/{name}", unreal.StaticMesh)
        for name in ("OfficeWorkstation", "OfficeFiles", "OfficeDrawer", "OfficeCooler")
    }
    district_solids = {
        "CornerShop", "SubstationHouse", "SubstationSwitchgear", "TransmissionPylon",
        "BurnedSedan", "BurnedCompact", "RustedTruck", "AbandonedVan",
        "SixTwelveShop", "SixTwelveCanopy",
    }
    district_assets = district_solids | {"StoneFacade", "OfficeFacade", "Storefront", "TransmissionCable"}
    district_meshes = {
        name: import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Region/{name}", unreal.StaticMesh)
        for name in sorted(district_assets)
    }
    district_meshes["YardTransformer"] = landmark_meshes["YardTransformer"]
    crate_class = unreal.load_class(None, "/Script/Survival.SurvivalSupplyCrate")
    zombie_class = unreal.load_class(None, "/Script/Survival.SurvivalZombie")
    door_class = unreal.load_class(None, "/Script/Survival.SurvivalWorldDoor")
    if not crate_class or not zombie_class or not door_class:
        raise RuntimeError("Build SurvivalEditor before placing structures and loot.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for mesh in (warehouse_mesh, shelf_mesh, desk_mesh, bed_mesh, crate_mesh, pump_mesh,
                 *communications_meshes.values(), *office_meshes.values(),
                 *(landmark_meshes[name] for name in sorted(landmark_collision)),
                 *(district_meshes[name] for name in sorted(district_solids))):
        if not unreal.CrossroadsWorldBake.set_static_mesh_collision(mesh):
            raise RuntimeError(f"Could not prepare collision for {mesh.get_name()}.")
        if not assets.save_loaded_asset(mesh, only_if_is_dirty=True):
            raise RuntimeError(f"Could not save {mesh.get_name()}.")
    surface_path = "/Game/Art/Crossroads/Surfaces"
    asphalt = unreal.load_asset(f"{surface_path}/M_Asphalt021")
    gravel = unreal.load_asset(f"{surface_path}/M_Ground036")
    concrete = unreal.load_asset(f"{surface_path}/M_Concrete034")
    if not asphalt or not concrete or not gravel:
        raise RuntimeError("Prepare the Crossroads surface materials before placing the region.")

    # Architectural finishes use world-space repeats. Accents use solid paint.
    colors = {
        "late_soviet": (.28, .26, .23), "brick_hardwood": (.30, .115, .065),
        "stucco_metal": (.84, .83, .78), "classical_civic": (.65, .61, .53),
        "modern": (.18, .25, .30), "dark_trim": (.036, .036, .035),
        "traffic": (.79, .58, .18), "petrol": (.13, .28, .29),
        "glazing": (.09, .19, .24), "linen": (.47, .44, .39), "soil": (.22, .138, .072),
        "grass": (.093, .142, .066),
    }
    style_materials = {
        style: prepare_art["make_material"](world_art, f"M_Region_{style}", (),
                                           tint=unreal.LinearColor(*color, 1.0))
        for style, color in colors.items() if style in {"dark_trim", "traffic", "glazing", "linen", "soil", "grass"}
    }

    color_map = unreal.load_asset(f"{surface_path}/Concrete034_Color")
    roughness = unreal.load_asset(f"{surface_path}/Concrete034_Roughness")
    sampler, prop = unreal.MaterialSamplerType, unreal.MaterialProperty
    def finish(name, texture, repeat, color=None):
        return prepare_art["make_material"](world_art, "M_RegionFinish_"+name, (
            (texture, sampler.SAMPLERTYPE_COLOR, "RGB", prop.MP_BASE_COLOR),
            (roughness, sampler.SAMPLERTYPE_LINEAR_COLOR, "R", prop.MP_ROUGHNESS),
        ), tint=color, world_repeat_cm=repeat)

    for name in ("StationBrick", "ShopTile"):
        tex = import_asset(source_art / "Finishes" / f"T_{name}.jpg", surface_path, unreal.Texture2D)
        style_materials[name] = finish(name, tex, 200 if name == "StationBrick" else 160)
    for style in ("late_soviet", "stucco_metal", "classical_civic", "modern", "petrol"):
        style_materials[style] = finish(style, color_map, 165, unreal.LinearColor(*colors[style], 1.0))
    style_materials["brick_hardwood"] = style_materials["StationBrick"]
    concrete = finish("Concrete", color_map, 200)
    style_materials["timber"] = unreal.load_asset("/Game/Art/Crossroads/Building/M_CrossroadsTimber")
    style_materials["concrete"] = concrete
    style_materials["asphalt"] = asphalt

    def spawn(kind, key, label, x, y, z, configure, tags=(), always_loaded=False, relocate=False,
              reconfigure=False):
        pos = unreal.Vector(west + x*100, north + (depth-y)*100, z_zero + z*100)
        if key in actor_tags:
            actor = actor_tags[key]
            if relocate:
                actor.set_actor_location(pos, False, False)
            if reconfigure:
                configure(actor)
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

    def prop_kind(key, label, health, profile, x, y, z, configure):
        old = actor_tags.get(key)
        if old and isinstance(old, unreal.StaticMeshActor):
            remove_generated(key)

        def setup(actor):
            configure(actor)
            actor.set_editor_property("max_health", health)
            actor.set_editor_property("loot_profile", getattr(unreal.SurvivalLootProfile, profile))

        return spawn(crate_class, key, label, x, y, z, setup, relocate=True, reconfigure=True)

    def fitted_mesh(actor, asset, dimensions, yaw=0, align_long_axis=False):
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        component.set_static_mesh(asset)
        # Previously baked cube props can carry a slot override into their replacement.
        for slot in range(component.get_num_materials()):
            component.set_material(slot, asset.get_material(slot))
        component.set_collision_profile_name("BlockAll")
        bounds = asset.get_bounding_box()
        extent = bounds.max - bounds.min
        if align_long_axis and extent.x < extent.y:
            dimensions = (dimensions[1], dimensions[0], dimensions[2])
            yaw += 90
        pad = [maximum * 100 / span for maximum, span in zip(
            dimensions, (extent.x, extent.y, extent.z), strict=True)]
        center = (bounds.min + bounds.max) * .5
        actor.set_actor_scale3d(unreal.Vector(*pad))
        actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)
        angle = math.radians(yaw)
        dx, dy = center.x*pad[0], center.y*pad[1]
        offset = unreal.Vector(dx*math.cos(angle)-dy*math.sin(angle),
                               dx*math.sin(angle)+dy*math.cos(angle), center.z*pad[2])
        actor.set_actor_location(actor.get_actor_location()-offset, False, False)
        return component

    def communications_art(key, label, name, x, y, floor, dimensions, *, yaw=0, health=None):
        asset = communications_meshes[name]
        z = floor + dimensions[2]/2

        def configure(actor):
            fitted_mesh(actor, asset, dimensions, yaw, align_long_axis=health is not None or name == "LaitseStation")

        if health is not None:
            prop_kind(key, label, health, "BUILDING", x, y, z, configure)
        else:
            spawn(unreal.StaticMeshActor, key, label, x, y, z, configure,
                  relocate=True, reconfigure=True)

    def district_mesh(key, label, name, x, y, floor, dimensions, *, yaw=0, collision=True, health=None,
                      align=True):
        asset = district_meshes[name]
        center_z = floor + dimensions[2]/2

        def configure(actor):
            component = fitted_mesh(actor, asset, dimensions, yaw, align_long_axis=align)
            if not collision:
                component.set_collision_profile_name("NoCollision")

        if health is None:
            spawn(unreal.StaticMeshActor, key, label, x, y, center_z, configure,
                  relocate=True, reconfigure=True)
        else:
            prop_kind(key, label, health, "BUILDING", x, y, center_z, configure)

    def cable(key, a, b):
        asset = district_meshes["TransmissionCable"]
        x,y,z = ((v+w)*.5 for v,w in zip(a,b))
        dx,dy,dz = (b[0]-a[0], a[1]-b[1], b[2]-a[2])
        length = math.hypot(dx,dy,dz)
        yaw = math.degrees(math.atan2(dy,dx))
        pitch = math.degrees(math.atan2(dz, math.hypot(dx,dy)))
        bounds = asset.get_bounding_box()
        long_is_x = bounds.max.x-bounds.min.x > bounds.max.y-bounds.min.y
        sag = utility["cable_sag"](length)
        stretch = unreal.Vector(length, 1, sag) if long_is_x else unreal.Vector(1, length, sag)

        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(asset)
            component.set_collision_profile_name("NoCollision")
            component.set_editor_property("cast_shadow", False)
            actor.set_actor_scale3d(stretch)
            rotation = unreal.Rotator(pitch=pitch,yaw=yaw)
            if not long_is_x:
                rotation = unreal.MathLibrary.compose_rotators(unreal.Rotator(yaw=90), rotation)
            actor.set_actor_rotation(rotation,False)

        spawn(unreal.StaticMeshActor,key,"Overhead transmission conductor",x,y,z,configure,
              relocate=True,reconfigure=True)

    def imported_station(key, x, y, floor):
        asset = district_meshes["SixTwelveShop"]
        bounds = asset.get_bounding_box()
        center = (bounds.min+bounds.max)*.5
        # The packed shop has an asymmetric pivot so the original entrance
        # orientation and coordinates survive either glTF axis convention.
        source_x_is_mesh_x = abs(center.x)>abs(center.y)
        source_x_sign = math.copysign(1, center.x if source_x_is_mesh_x else center.y)
        source_y_sign = math.copysign(1, center.y if source_x_is_mesh_x else center.x)
        yaw = (180 if source_y_sign>0 else 0) if source_x_is_mesh_x else (-90 if source_y_sign>0 else 90)
        angle = math.radians(yaw)
        east = source_x_sign*(math.cos(angle) if source_x_is_mesh_x else -math.sin(angle))
        width, depth, height = 14.0, 12.9, 4.86
        dimensions = (width, depth, height) if source_x_is_mesh_x else (depth, width, height)
        shop_y = y+2.8
        district_mesh(key+"_shop", "6twelve convenience shop", "SixTwelveShop",
                      x, shop_y, floor, dimensions, yaw=yaw, align=False)

        # Door leaves from the source were removed to install working gameplay doors.
        for suffix, sx, sy, bottom, door_width, door_height in (
            ("shop_door", 6.19, 14.25, .13, .94, 2.53),
            ("stockroom_door", 8.40, 24.58, .13, .93, 2.49),
        ):
            door(key+"_"+suffix, "Convenience store door",
                 x+east*(sx-4.21)*width/15, shop_y+(sy-21.43)*depth/16.816,
                 floor+(bottom+.023)*height/5.213, door_width*width/15, door_height*height/5.213,
                 finish="petrol")
        door(key+"_side_door","Store service door",
             x+east*(10.33-4.21)*width/15, shop_y+(27.45-21.43)*depth/16.816,
             floor+.142, .93*depth/16.816, 2.3, yaw=90*east, finish="petrol")
        district_mesh(key+"_canopy", "6twelve fuel canopy", "SixTwelveCanopy",
                      x, y-6.0, floor, (17.0,6.4,5.5))
        site_tools["service_forecourt"](box, key, x, y+2.4, floor, style_materials)
        cache(key+"_stock", "Convenience shop provisions", x+east*3.6, y+4.7, floor+.13, profile="FOOD")
        cache(key+"_yard", "Delivery crate", x-7.6, y+3.1, floor, profile="BUILDING")

    def wreck(key, x, y, z, yaw, name):
        dimensions = {"BurnedSedan": (4.6,1.92,1.34), "BurnedCompact": (4.05,1.73,1.19),
                      "RustedTruck": (6.4,2.37,2.52), "AbandonedVan": (4.44,1.83,1.72)}
        label = "Abandoned vehicle" if name == "AbandonedVan" else "Wrecked vehicle"
        district_mesh(key,label,name,x,y,z,dimensions[name],yaw=yaw,health=240)

    prop_profiles = {
        "Station counter": (95, "GENERAL"),
        "Fuel dispenser": (125, "BUILDING"),
        "Weathered cargo crate": (60, "BUILDING"),
        "Warehouse shelving": (100, "BUILDING"),
        "Shipping pallet": (50, "BUILDING"),
        "Service counter": (95, "FOOD"),
        "Booth seat": (70, "GENERAL"),
        "Reception desk": (90, "OFFICE"),
        "Archive shelving": (80, "OFFICE"),
        "Apartment fallen cabinet": (60, "GENERAL"),
        "Waiting area bench": (65, "GENERAL"),
        "Treatment room cabinet": (75, "MEDICAL"),
        "Triage desk": (80, "MEDICAL"),
        "Bedroom dresser": (65, "GENERAL"),
        "Nightstand": (45, "GENERAL"),
        "Kitchen table": (60, "FOOD"),
        "Checkout counter": (90, "GENERAL"),
        "Wall cabinets": (80, "OFFICE"),
        "Work tables": (75, "OFFICE"),
        "Reception station": (90, "OFFICE"),
        "Industrial storage": (100, "BUILDING"),
        "Crated cargo": (60, "BUILDING"),
        "Generator bank": (150, "BUILDING"),
        "Maintenance workbench": (80, "BUILDING"),
        "Shelved work station": (80, "OFFICE"),
        "Sideline bench": (55, "GENERAL"),
        "Check-in counter": (95, "OFFICE"),
        "Concourse seating": (70, "GENERAL"),
    }

    furniture_slots = {
        **dict.fromkeys(("Shelved work station", "Triage desk", "Reception desk",
                         "Reception station", "Work tables"), ("OfficeWorkstation", (1.95, 1.52, 1.32))),
        **dict.fromkeys(("Archive shelving", "Wall cabinets", "Treatment room cabinet"),
                        ("OfficeFiles", (.73, .54, 1.45))),
        "Bedroom dresser": ("OfficeDrawer", (.81, .44, 1.03)),
        "Water cooler": ("OfficeCooler", (.38, .40, 1.58)),
    }
    prop_profiles["Water cooler"] = (45, "FOOD")

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

        if label in prop_profiles and collision and not ground and not hidden:
            health, profile = prop_profiles[label]
            asset = {"Weathered cargo crate": crate_mesh, "Crated cargo": crate_mesh,
                     "Fuel dispenser": pump_mesh}.get(label)
            dimensions = (w, d, h)
            if label in furniture_slots:
                name, dimensions = furniture_slots[label]
                asset = office_meshes[name]
            if asset:
                def configure_mesh(actor):
                    fitted_mesh(actor, asset, dimensions, align_long_axis=label in furniture_slots)
                prop_kind(key, label, health, profile, x, y, z+dimensions[2]/2, configure_mesh)
            else:
                actor = prop_kind(key, label, health, profile, x, y, z+h/2, configure)
                if material:
                    actor.get_component_by_class(unreal.StaticMeshComponent).set_material(0, material)
            return
        actor = spawn(unreal.StaticMeshActor, key, label, x, y, z + h/2, configure,
                      ("StructuralGround",) if ground else ())
        if material:
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if component.get_material(0) != material:
                component.set_material(0, material)

    def landmark_mesh(key, label, name, x, y, z, *, scale=(1, 1, 1), collision=False,
                      pitch=0, align_long_axis=False, opening_south=False):
        asset = landmark_meshes[name]
        bounds = asset.get_bounding_box()
        swapped = bounds.max.x-bounds.min.x < bounds.max.y-bounds.min.y
        yaw = 90 if align_long_axis and swapped else 0
        if opening_south:
            if swapped:
                yaw = 90 if bounds.max.x+bounds.min.x > 0 else -90
            else:
                yaw = 0 if bounds.max.y+bounds.min.y > 0 else 180
        local_scale = (scale[1], scale[0], scale[2]) if swapped and align_long_axis else scale

        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(asset)
            component.set_collision_profile_name("BlockAll" if collision or name in {"PitchEntranceSign", "RadiationSign"} else "NoCollision")
            if name == "AircraftHangar" and concrete:
                component.set_material(0, concrete)
            actor.set_actor_scale3d(unreal.Vector(*local_scale))
            rotation = unreal.Rotator(yaw=yaw)
            if pitch:
                rotation = unreal.MathLibrary.compose_rotators(rotation, unreal.Rotator(pitch=pitch))
            actor.set_actor_rotation(rotation, False)

        if name in {"WeatheredBusStop", "YardTransformer", "PitchEntranceSign", "RadiationSign"}:
            prop_kind(key, label, 120, "BUILDING", x, y, z, configure)
        else:
            spawn(unreal.StaticMeshActor, key, label, x, y, z, configure)

    def remove_generated(*keys):
        for key in keys:
            actor = actor_tags.get(key)
            if actor:
                if not actors.destroy_actor(actor):
                    raise RuntimeError(f"Could not replace generated actor {key}.")
                actor_tags.pop(key)

    def remove_blockout(key):
        remove_generated(key + "_shell")

    def migrate_landmark(key, new_key, keep=()):
        # Only sweep the generator's reserved tags for this site.
        remove_generated(*(tag for tag in actor_tags if tag.startswith(key+"_")
                           and not tag.startswith(new_key+"_") and tag not in keep))

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
            for slot in range(component.get_num_materials()):
                component.set_material(slot, bed_mesh.get_material(slot))
            component.set_collision_profile_name("BlockAll")
        prop_kind(key, label, 85, "MEDICAL", x, y, floor, configure)

    def furniture(key, label, kind, x, y, floor):
        mesh = {"shelf": shelf_mesh, "desk": desk_mesh}[kind]

        def configure(actor):
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(mesh)
            for slot in range(component.get_num_materials()):
                component.set_material(slot, mesh.get_material(slot))
            component.set_collision_profile_name("BlockAll")

        prop_kind(key, label, 75, "FOOD" if kind == "shelf" else "OFFICE", x, y, floor, configure)

    def cache(key, label, x, y, floor, *, profile=None, chance=100, **loot):
        if profile is None:
            if key.startswith("CrossroadsRegion_south-services_"):
                profile = "AUTHORED"
            elif loot.get("contains_sidearm") or loot.get("contains_rifle") or "rifle_part_slot" in loot:
                profile = "SECURITY"
            elif loot.get("food_amount") or loot.get("water_amount") and not loot.get("cloth_amount"):
                profile = "FOOD"
            elif loot.get("timber_amount") or loot.get("stone_amount"):
                profile = "BUILDING"
            elif loot.get("cloth_amount") or loot.get("dressing_schematic_amount"):
                profile = "MEDICAL"
            else:
                profile = "GENERAL"

        def configure(actor):
            for prop, amount in {"timber_amount": 0, "stone_amount": 0, **loot}.items():
                actor.set_editor_property(prop, amount)
            actor.set_editor_property("loot_profile", getattr(unreal.SurvivalLootProfile, profile))
            actor.set_editor_property("spawn_chance_percent", chance)
            if not (loot.get("contains_sidearm") or loot.get("contains_rifle")):
                fitted_mesh(actor, crate_mesh, (.82, .61, .58))

        # Rebase both existing and new actors before centering the imported mesh.
        spawn(crate_class, key, label, x, y, floor+.30, configure, relocate=True, reconfigure=True)

    def scatter_caches(key, x, y, floor, positions, weights, chance):
        profiles = ("FOOD", "BUILDING", "MEDICAL", "SECURITY", "OFFICE", "GENERAL")
        labels = ("Provisions crate", "Materials crate", "Medical supplies", "Security supplies",
                  "Records and supplies", "Mixed supplies")
        randomizer = random.Random(f"{plan['seed']}:{key}")
        types = randomizer.choices(range(len(profiles)), weights=weights, k=len(positions))
        for index, ((dx, dy), kind) in enumerate(zip(positions, types, strict=True)):
            cache(f"{key}_loot_{index}", labels[kind], x+dx, y+dy, floor,
                  profile=profiles[kind], chance=chance)

    def encounter(key, label, x, y, floor, runner=False, relocate=False):
        def configure(actor):
            actor.set_editor_property("aggro_range", 2100.0)
            actor.set_editor_property("is_runner", runner)
        spawn(zombie_class, key, label, x, y, floor+1.03, configure, relocate=relocate)

    def place_route(route):
        width = route["width_m"]
        road_surface = gravel if route.get("surface") == "gravel" else asphalt
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
            material = concrete if bridge else road_surface
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
              stone_amount=30, timber_amount=24)
        cache(key+"_supplies", "Warehouse storage crate", x+8, y+1.5, floor,
              cloth_amount=3, water_amount=2)
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
        if site["id"] != "stadium":
            box(key+"_slab", name+" slab", x, y, z-.6, w, d, .8, ground=True, material=concrete)
        # Leave a walkable pad at the front, including the initial spawn.
        if site["id"] == "stadium":
            new_key = key+"_community"
            landmarks["pitch"](box, landmark_mesh, new_key, plan["facilities"]["stadium"], style_materials, concrete)
            migrate_landmark(key, new_key)
        elif site["id"] == "airport":
            new_key = key+"_airfield"
            landmarks["airfield"](box, landmark_mesh, door, ramp, sign, storefront_window, cache, encounter,
                                  new_key, plan["facilities"]["airport"], z+.2, style_materials, asphalt, concrete,
                                  airfield_dimensions)
            migrate_landmark(key, new_key, (key+"_slab",))
        elif site["id"] == "hangars":
            new_key = key+"_operations"
            landmarks["aircraft_hangar"](box, landmark_mesh, door, cache, new_key,
                                          x, y, z+.215, concrete)
            migrate_landmark(key, new_key, (key+"_slab",))
        elif site["id"] == "city-hospital":
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
            for row, offset in enumerate((-6.0, 5.8)):
                for index, dx in enumerate((-7.8,-3.3,1.4,6.0)):
                    name = ("AbandonedVan", "BurnedCompact", "BurnedSedan")[(row+index)%3]
                    wreck(f"{key}_parked_{row}_{index}", x+dx, y+offset, z+.2, row*180+90, name)
            for i, dx in enumerate((-7, 1.5, 6)):
                wreck(f"{key}_upper_wreck_{i}", x+dx, y-6, z+3.35, 90,
                      "AbandonedVan" if i%2 else "BurnedCompact")
            remove_blockout(key)
        elif site["id"] == "southwest-apartments":
            site_tools["apartment_layout"](box, door, cache, encounter, key, x, y-7, z+.2, plan["seed"])
            interiors["place_site"](box, door, ramp, cache, encounter, sign, storefront_window,
                treatment_bed, furniture, site_tools["building_shell"], site_tools["double_door"], key+"_east_block",
                site, x, y+10, z+.2, 34, 12, style_materials, scatter_caches)
            remove_generated(key+"_exterior", key+"_closed")
            remove_blockout(key)
        elif site["id"] == "southwest-services":
            new_key = key+"_SixTwelve"
            imported_station(new_key, x, y, z+.2)
            migrate_landmark(key,new_key,(key+"_slab",))
        elif site["kind"] == "services":
            box(key+"_floor_finish", "Station shop floor", x, y+.5, z+.199,
                12, 8.3, .021, ground=True, material=style_materials["ShopTile"])
            site_tools["station_layout"](box, door, key, x, y+.5, z+.22, style_materials)
            remove_generated(*(tag for tag in actor_tags if tag.startswith(key+"_pump_")
                               and tag.endswith("_face")))
            remove_generated(key+"_east_front", key+"_west_front")
            sign(key+"_forecourt_sign", "CROSSROADS  /  PETROL", x, y-7.65, z+3.6, size=34)
            for index, offset in enumerate((-3.8, -1.7)):
                furniture(f"{key}_shop_shelf_{index}", "Station shop shelving", "shelf",
                          x+offset, y-.3, z+.22)
            box(key+"_cooler", "Water cooler", x+4.9, y+3.4, z+.22, .4, .4, 1.58)
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
            scatter_caches(key+"_shop", x, y+.5, z+.22, ((-4.6,-2.4), (4.8,-2.4)),
                           (72, 8, 7, 2, 3, 8), 90)
            remove_blockout(key)
        else:
            interiors["place_site"](box, door, ramp, cache, encounter, sign, storefront_window,
                treatment_bed, furniture, site_tools["building_shell"], site_tools["double_door"], key,
                site, x, y, z+.2, w, d, style_materials, scatter_caches)
            remove_generated(key+"_shell", key+"_trim", key+"_portico_roof",
                             *(f"{key}_portico_{i}" for i in range(4)))
        if site["id"] == "city-towers":
            annex_x = x+19.5
            box(key+"_annex_floor", "Tower annex foundation", annex_x, y, z-.05,
                6.5, 20, .25, ground=True, material=concrete)
            front = site_tools["building_shell"](box, key+"_annex", annex_x, y, z+.2, 6.5, 20,
                                                3.8, 1.65, concrete, asphalt)
            door(key+"_annex_entry", "Corner shop door", annex_x, front-.045, z+.2, 1.65, 2.35)
            scatter_caches(key+"_annex", annex_x, y, z+.2,
                           ((-2.1,-5.5), (2.1,-3), (-2.1,2.3), (2,6.1), (-2,7.7)),
                           (51, 9, 9, 4, 10, 17), 95)
        if site["id"] in {"city-hospital", "city-police", "city-hall", "city-parking", "city-nightlife"}:
            city_cache_layouts = {
                "city-hospital": (((-7.6,-1.5), (7.65,-1.5), (-6.65,5.5), (6,2.4)), (5, 4, 78, 3, 6, 4)),
                "city-police": (((-9.6,-2.6), (9.6,-2.6), (-7.6,6.5), (8.1,6.4)), (7, 7, 12, 60, 8, 6)),
                "city-hall": (((-9,-3.5), (8,1.6), (-10,6.5), (9.6,5.4)), (10, 10, 8, 5, 52, 15)),
                "city-parking": (((-7.5,9.4), (2.2,9.4), (8,9.4), (12.1,8.9)), (10, 51, 9, 8, 7, 15)),
                "city-nightlife": (((-12.8,2.1), (-10.8,-1.4), (2.9,4.4), (4.7,-1.4)), (68, 5, 9, 3, 6, 9)),
            }
            positions, weights = city_cache_layouts[site["id"]]
            scatter_caches(key+"_district", x, y, z+.2, positions, weights, 95)

        if site["id"] in {"winter-village", "hill-town", "north-lowlands"}:
            landmark_mesh(key+"_bus_stop", "Weathered roadside bus shelter", "WeatheredBusStop",
                          x+7.6, y-d/2-2.8, z+.05, collision=True)
        if site["id"] in {"power-southwest", "power-northeast"}:
            landmark_mesh(key+"_yard_transformer", "Substation transformer", "YardTransformer",
                          x+20.0, y-d/2+4.0, z+.2, collision=True)
            landmark_mesh(key+"_yard_warning", "Weathered radiation warning", "RadiationSign",
                          x-3.2, y-d/2+2, z+.2)

        if site["id"] == "south-services" and not has_start:
            spawn(unreal.PlayerStart, key+"_start", "Start at Southern Crossroads",
                  x, y-d*.31, z+1.3,
                  lambda actor: actor.set_actor_rotation(unreal.Rotator(yaw=-90), False),
                  always_loaded=True)

    for route in plan["m4_route"]:
        place_route(route)
    for site in plan["communications"]:
        communications["stamp_compound"](site, box, door, sign, site_tools["building_shell"],
                                         storefront_window, communications_art, cache, encounter, style_materials)
        if site["kind"] == "radio":
            key = "CrossroadsComms_" + site["id"]
            migrate_landmark(key, key+"_Laitse")
    for site in plan["substations"]:
        utility["stamp_substation"](site, box, district_mesh, district_mesh, cache, sign, concrete)
    transmission = plan["grid"]
    for key, pole in transmission["poles"].items():
        district_mesh("CrossroadsPylon_"+key, "High-voltage transmission pylon", "TransmissionPylon",
                      pole["x"],pole["y"],pole["z"],(10.432,2.622,pole["height_m"]),yaw=pole["yaw"])
    for a,b in transmission["spans"]:
        for index,(start,end) in enumerate(zip(transmission["poles"][a]["phases"],
                                             transmission["poles"][b]["phases"], strict=True)):
            cable(f"CrossroadsCable_{a}_{b}_{index}",start,end)
    downtown["stamp_city"](plan["city_lots"],plan["city_barriers"],box,district_mesh,
                           communications_art,door,cache,style_materials)
    for car in plan["wrecks"]:
        wreck("CrossroadsWreck_"+car["id"],car["x"],car["y"],car["z"],car["yaw"],car["model"])
    for checkpoint in plan["checkpoints"]:
        communications["stamp_checkpoint"](checkpoint, communications_art)

    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}.")
    unreal.log(f"Stamped {LEVEL}. Save external actors with Save All.")


if __name__ == "__main__":
    place()
