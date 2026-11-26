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
    civic = runpy.run_path(str(root / "Tools" / "civic_landmarks.py"))
    interiors = runpy.run_path(str(root / "Tools" / "place_region_interiors.py"))
    communications = runpy.run_path(str(root / "Tools" / "communications_layout.py"))
    rail_tools = runpy.run_path(str(root / "Tools" / "place_rail.py"))
    pavement = runpy.run_path(str(root / "Tools" / "city_pavement.py"))
    downtown = runpy.run_path(str(root / "Tools" / "downtown_layout.py"))
    utility = runpy.run_path(str(root / "Tools" / "utility_layout.py"))
    landmarks = runpy.run_path(str(root / "Tools" / "place_region_landmarks.py"))
    local_art = runpy.run_path(str(root / "Tools" / "local_world_art.py"))
    airfield_dimensions = runpy.run_path(str(root / "Tools" / "airfield_layout.py"))
    warehouse_depth_scale = 1.85
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
    world_width, world_depth = plan["size_m"]
    if abs(east - west - world_width*100) > 400 or abs(south - north - world_depth*100) > 400:
        raise RuntimeError("Load the entire Landscape in the World Partition editor and check its XY scale.")
    z_zero = landscape_root.get_actor_location().z
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if not cube:
        raise RuntimeError("Could not load the engine blockout cube.")

    actor_tags = {str(tag): actor for actor in existing for tag in actor.get_editor_property("tags")}
    prepare_art = runpy.run_path(str(root / "Tools" / "prepare_crossroads.py"))
    import_asset = prepare_art["import_asset"]
    prop_art = runpy.run_path(str(root / "Tools" / "world_prop_materials.py"))
    world_art = prop_art["WORLD"]
    source_art = root / "SourceArt" / "Crossroads" / "World"
    bed_mesh, bay_mesh, warehouse_mesh, shelf_mesh, desk_mesh = (
        import_asset(source_art / f"SM_{name}.glb", world_art, unreal.StaticMesh,
                     refresh_if_source_changed=name == "WarehouseDoorwayShell")
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
        for name in landmark_names if name not in {"RuinedTerminal", "RuinedAirportShop"}
    }
    crate_mesh, pump_mesh = (
        import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Props/{name}", unreal.StaticMesh)
        for name in ("ScavengeCrate", "FuelPumpSolid")
    )
    communications_meshes = {
        name: import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Communications/{name}", unreal.StaticMesh)
        for name in ("RepeaterMast", "CheckpointWall", "TimberWire", "RoadSpikes")
    }
    # Cut the original scan geometry so rendering and complex collision share the same openings.
    cuts = {
        "LaitseStation": [((cx-.38,cx+.38),(-10,-4.28),(-20,20)) for cx in (-2.4,2.5)]
                         + [((-20,20),(-10,-4.24),(-1.45,-.45))],
        "RuinedTerminal": [((-1.35,1.35),(.15,2.55),(-20,20))],
        "RuinedAirportShop": [((-1.35,1.35),(.15,2.45),(-25,25))],
        "SkalkaTower": [((-.9,.9),(-8,1.5),(-20,20))],
        "PanelHousingExterior": [((-1.2,1.2),(-.1,2.65),(-6,6))],
        "SubstationHouse": [((-.54,.54),(-1,1.62),(-4,4))],
    }
    cut_mesh = runpy.run_path(str(root/"Tools"/"survey_access.py"))["prepare"]
    def survey_mesh(name):
        package = f"{world_art}/SurveyAccess/{name}"
        loaded = unreal.load_asset(f"{package}/SM_{name}") if unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).does_asset_exist(f"{package}/SM_{name}") else None
        if loaded:
            return loaded
        source = cut_mesh(source_art/f"SM_{name}.glb", root/"Saved"/"WorldBake"/"Access", cuts[name])
        return import_asset(source, package, unreal.StaticMesh)
    for name in ("RuinedTerminal", "RuinedAirportShop"):
        landmark_meshes[name] = survey_mesh(name)
    for name in ("SkalkaTower", "LaitseStation"):
        communications_meshes[name] = survey_mesh(name)
    office_meshes = {
        name: import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Props/{name}", unreal.StaticMesh)
        for name in ("OfficeWorkstation", "OfficeFiles", "OfficeDrawer", "OfficeCooler")
    }
    parish_art = {}
    for name in ("TimberLeaf", "GlazedLeaf", "ServiceLeaf", "ParishUpperChurch", "RoofLadder"):
        asset = import_asset(source_art / "Parish" / f"SM_{name}.glb",
                             f"{world_art}/Parish/SM_{name}", unreal.StaticMesh)
        if name.endswith("Leaf") or name == "RoofLadder":
            if not unreal.CrossroadsWorldBake.set_static_mesh_collision(asset):
                raise RuntimeError(f"Could not prepare {name} collision")
        parish_art[name] = asset

    district_solids = {
        "GuardTower", "MilitaryAmbulance", "CornerShop", "SubstationHouse", "SubstationSwitchgear", "TransmissionPylon",
        "BurnedSedan", "BurnedCompact", "RustedTruck", "AbandonedVan",
        "SixTwelveShop", "SixTwelveCanopy",
    }
    district_assets = district_solids | {"QuarantineWire","CoolingTower","StoneFacade", "OfficeFacade", "Storefront", "TransmissionCable"}
    district_meshes = {
        name: import_asset(source_art / f"SM_{name}.glb", f"{world_art}/Region/{name}", unreal.StaticMesh)
        for name in sorted(district_assets - {"AbandonedVan", "SubstationHouse"})
    }
    detail_meshes = {}
    for name in ("QuarantineMine","ShopRegister","ShopCounter","ShopCooler"):
        mesh = import_asset(source_art/"RegionDetail"/f"SM_{name}.glb",
                            f"{world_art}/RegionDetail/{name}", unreal.StaticMesh)
        detail_meshes[name] = mesh
        if name != "QuarantineMine":
            district_meshes[name] = mesh
            district_solids.add(name)
    district_meshes["PanelHousingExterior"] = survey_mesh("PanelHousingExterior")
    district_meshes["SubstationHouse"] = survey_mesh("SubstationHouse")
    district_meshes["AbandonedVan"] = import_asset(source_art / "SM_AbandonedVan.glb",
        f"{world_art}/SurveyDetail/AbandonedVan", unreal.StaticMesh)
    district_meshes["YardTransformer"] = landmark_meshes["YardTransformer"]
    crate_class = unreal.load_class(None, "/Script/Survival.SurvivalSupplyCrate")
    zombie_class = unreal.load_class(None, "/Script/Survival.SurvivalZombie")
    door_class = unreal.load_class(None, "/Script/Survival.SurvivalWorldDoor")
    if not crate_class or not zombie_class or not door_class:
        raise RuntimeError("Build SurvivalEditor before placing structures and loot.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    scratch = root / "Saved" / "WorldBake" / "PropTextures"
    for name, mesh in {"WarehouseDoorwayShell": warehouse_mesh, "WeatheredShelf": shelf_mesh,
                       "FuelPumpSolid": pump_mesh, **office_meshes}.items():
        prop_art["apply"](source_art / f"SM_{name}.glb", mesh, scratch, prepare_art, assets)
    prop_art["apply"](source_art / "Parish" / "SM_ParishUpperChurch.glb", parish_art["ParishUpperChurch"],
                      scratch, prepare_art, assets)
    for name in ("SubstationHouse", "SubstationSwitchgear"):
        prop_art["apply"](source_art / f"SM_{name}.glb", district_meshes[name], scratch, prepare_art, assets)
    military_crate = prop_art["apply"](source_art / "SM_ScavengeCrate.glb", crate_mesh, scratch, prepare_art, assets)
    unreal.log("Crossroads: restored embedded textures for pumps, crates, shelving and office props.")
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
    hangar_finish = finish("HangarConcrete", color_map, 200)
    if not hangar_finish.get_editor_property("two_sided"):
        hangar_finish.set_editor_property("two_sided", True)
        unreal.MaterialEditingLibrary.recompile_material(hangar_finish)
        if not assets.save_loaded_asset(hangar_finish):
            raise RuntimeError("Could not save the hangar interior finish")
    style_materials["concrete"] = concrete
    style_materials["asphalt"] = asphalt

    regenerated = ("CrossroadsPavement_", "CrossroadsStreetBarrier_", "CrossroadsWreck_", "CrossroadsRail_")
    city_route_tags = tuple(f'CrossroadsRoute_{r["id"]}_' for r in plan["m4_route"]
                            if pavement["is_city_route"](r))
    previous_roads = {key for key in actor_tags if key.startswith(regenerated + city_route_tags)}
    touched_roads = set()
    previous_infill = {key for key in actor_tags if key.startswith("CrossroadsInfill_")}
    touched_infill = set()
    # Geometry and dependent doors, furnishings and loot share the same rebake.
    resized_site = None
    site_keys = set()

    def spawn(kind, key, label, x, y, z, configure, tags=(), always_loaded=False, relocate=False,
              reconfigure=False):
        if key.startswith("CrossroadsRegion_"):
            site_keys.add(key)
        if key.startswith("CrossroadsInfill_"):
            touched_infill.add(key)
            relocate = reconfigure = True
        elif key.startswith(regenerated + city_route_tags):
            touched_roads.add(key)
            relocate = reconfigure = True
        elif resized_site and key.startswith(resized_site + "_"):
            relocate = reconfigure = True
        pos = unreal.Vector(west + x*100, north + (world_depth-y)*100, z_zero + z*100)
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
            actor.set_editor_property("loot_name", label.lower())
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
                      align=True, loot_profile="BUILDING"):
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
            prop_kind(key, label, health, loot_profile, x, y, center_z, configure)

    facade_class = unreal.load_class(None, "/Script/Survival.SurvivalFacade")
    if not facade_class:
        raise RuntimeError("Rebuild SurvivalEditor to load the instanced downtown facades.")

    city_panels = []
    city_entries = local_art["pack_meshes"](root, "CitySampleBuildings")
    if city_entries:
        candidates = [(str(a.package_name), a) for a in city_entries
                      if str(a.asset_name).upper().startswith("SM_BLDG_")
                      and "_WALL_" in str(a.asset_name).upper()]
        families = set()
        for path, entry in candidates:
            family = path.rsplit("/", 1)[0]
            if family in families:
                continue
            panel = entry.get_asset()
            if not isinstance(panel, unreal.StaticMesh):
                continue
            bounds = panel.get_bounding_box()
            size = bounds.max - bounds.min
            if (180 <= max(size.x,size.y) <= 2000 and 180 <= size.z <= 1600
                    and 1 < min(size.x,size.y) < max(size.x,size.y)*.35):
                city_panels.append(panel)
                families.add(family)
                if len(city_panels) == 16:
                    break
        unreal.log(f"Crossroads selected {len(city_panels)} City Sample wall modules.")
    unpowered = local_art["dark_materials"](prepare_art, style_materials)
    panel_materials = {}
    factory = local_art["factory_props"](local_art["pack_meshes"](root, "OvergrownFactoryENV"))
    unreal.log("Crossroads Overgrown Factory meshes: " +
               (", ".join(f"{role}={mesh.get_name()}" for role, mesh in factory.items()) or "none"))

    def facade(key, style, x, y, z, width, depth, height, yaw, ground_height, entrance_width, *, bundled=False):
        def configure(actor):
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)
            panel = random.Random(key).choice(city_panels) if city_panels and not bundled else district_meshes[style]
            name = panel.get_path_name()
            if name not in panel_materials:
                fallback = style_materials["classical_civic" if style == "StoneFacade" else "modern"]
                slots = panel.get_editor_property("static_materials")
                original = [slot.get_editor_property("material_interface") for slot in slots]
                panel_materials[name] = ([unpowered(material) or fallback for material in original]
                                         if city_panels and not bundled else original)
            actor.fit_panels(panel, width*100, depth*100, height*100,
                             ground_height*100, entrance_width*100, panel_materials[name])
        spawn(facade_class, key, "City facade panels", x, y, z, configure)

    def campus_art(key,role,x,y,floor,width,depth,height,story,yaw,opening):
        # The Poly Haven mesh contains exactly one three-metre window bay.
        # Its instance transforms follow the same storey elevations as the stairs.
        remove_generated(key+"_sample_main", key+"_sample_ground")
        bounds=bay_mesh.get_bounding_box()
        ext=bounds.max-bounds.min
        wide_x=ext.x>=ext.y
        transforms=[]
        for level in range(round(height/story)):
            for face,span,outset in ((0,width,depth/2),(1,depth,width/2),
                                      (2,width,depth/2),(3,depth,width/2)):
                intervals=((-span/2,-opening/2),(opening/2,span/2)) if face==0 and level==0 else ((-span/2,span/2),)
                normal=unreal.Rotator(yaw=face*90)
                rotation=unreal.Rotator(yaw=face*90+(0 if wide_x else 90))
                for left,right in intervals:
                    count=max(1,round((right-left)/3))
                    bay=(right-left)/count
                    scale=unreal.Vector(bay*100/ext.x if wide_x else 1,
                                        1 if wide_x else bay*100/ext.y,story*100/ext.z)
                    offset=unreal.MathLibrary.transform_location(unreal.Transform(unreal.Vector(),rotation,scale),
                                                                  (bounds.min+bounds.max)*.5)
                    for index in range(count):
                        local=unreal.Vector((left+(index+.5)*bay)*100,outset*100,(level+.5)*story*100)
                        center=unreal.MathLibrary.transform_location(unreal.Transform(unreal.Vector(),normal,unreal.Vector(1,1,1)),local)
                        transforms.append(unreal.Transform(center-offset,rotation,scale))
        def install(actor):
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
            actor.set_static_instances(bay_mesh,transforms)
        spawn(facade_class,key+"_academic_bays","Academic window bays",x,y,floor,install,
              relocate=True,reconfigure=True)
        return True

    hero_folder = "/Game/CitySampleBuildings/Building/Library/Kit_Hero_Bldg/LevelInstance"
    hero_classes = {}

    def hero_class(name):
        if name not in hero_classes:
            package = f"{hero_folder}/BPP_{name}_N1"
            hero_classes[name] = (unreal.EditorAssetLibrary.load_blueprint_class(package)
                                  if assets.does_asset_exist(package) else None)
        return hero_classes[name]

    def hero_art(key, role, x, y, floor, width, depth, height, ground_height, yaw, entrance_width):
        name, _ = civic["HEROES"][role]
        pieces = [("main", name, height, floor, entrance_width)]
        # Fit the authored silhouette once. Independently scaling its Level01
        # and upper copy breaks the vertical alignment of their façades.
        remove_generated(key+"_sample_ground")
        sources = [(suffix, asset_name, hero_class(asset_name), h, bottom, opening)
                   for suffix, asset_name, h, bottom, opening in pieces]
        existing_skins = (key+"_sample_main", key+"_sample_ground")
        def clear_skin():
            remove_generated(*existing_skins, *(tag for tag in actor_tags if tag.startswith(key+"_trim_")))

        missing = [f"BPP_{asset_name}_N1" for _, asset_name, source_class, *_ in sources if source_class is None]
        if missing:
            clear_skin()
            unreal.log_warning(f"Crossroads retained the playable {role} blockout. Missing City Sample packages in "
                               f"{hero_folder}: {', '.join(missing)}.")
            return False

        # Copy the pack's authored instances into native actors and let the gameplay walls
        # own collision. Temporary sample actors never become external actors in the map.
        for suffix, asset_name, source_class, h, bottom, opening in sources:
            tag = f"{key}_sample_{suffix}"
            probe = actors.spawn_actor_from_class(source_class, unreal.Vector(), transient=True)
            if not probe:
                clear_skin()
                unreal.log_warning(f"Crossroads retained the playable {role} blockout. Could not spawn {asset_name}.")
                return False
            try:
                def install(actor):
                    actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)
                    dimensions = unreal.Vector(width*100, depth*100, h*100)
                    finishes = {source: unpowered(source)
                                for source in unreal.CrossroadsWorldBake.get_landmark_materials(probe)}
                    if not unreal.CrossroadsWorldBake.copy_landmark_skin(
                            actor, probe, dimensions, opening*100, 293, finishes, style_materials["classical_civic"]):
                        raise ValueError(f"Crossroads retained the playable {role} blockout. "
                                         f"City Sample {asset_name} has no visible exterior to copy.")

                spawn(facade_class, tag, "City Sample "+role+" exterior", x, y, bottom,
                      install, relocate=True, reconfigure=True)
            except ValueError as error:
                clear_skin()
                unreal.log_warning(str(error))
                return False
            finally:
                actors.destroy_actor(probe)
        remove_generated(*(tag for tag in actor_tags if tag.startswith((key+"_window_", key+"_pilaster_"))))
        unreal.log(f"Crossroads placed {name} as {role}.")
        return True

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
        district_mesh(key, label, name, x, y, z, dimensions[name], yaw=yaw, health=240,
                      loot_profile="VEHICLE")

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
        "Water dispenser": ("OfficeCooler", (.38, .40, 1.58)),
    }
    prop_profiles.update(dict.fromkeys(("Water cooler", "Water dispenser"), (45, "FOOD")))

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
            actor.set_actor_hidden_in_game(False)
            mesh.set_visibility(not hidden)

        if label in prop_profiles and collision and not ground and not hidden:
            health, profile = prop_profiles[label]
            asset = {"Weathered cargo crate": crate_mesh, "Crated cargo": crate_mesh,
                     "Fuel dispenser": pump_mesh, "Warehouse shelving": factory.get("rack", shelf_mesh),
                     "Industrial storage": factory.get("rack", shelf_mesh),
                     "Shipping pallet": factory.get("pallet")}.get(label)
            dimensions = (w, d, h)
            if label in furniture_slots:
                name, dimensions = furniture_slots[label]
                asset = office_meshes[name]
            if asset:
                def configure_mesh(actor):
                    if label == "Warehouse shelving":
                        fitted_mesh(actor, asset, (d, w, h), yaw=90, align_long_axis=True)
                    else:
                        fitted_mesh(actor, asset, dimensions,
                                    align_long_axis=label in furniture_slots or label in {"Industrial storage", "Shipping pallet"})
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
                for slot in range(max(1,component.get_num_materials())):
                    component.set_material(slot, hangar_finish)
            actor.set_actor_scale3d(unreal.Vector(*local_scale))
            rotation = unreal.Rotator(yaw=yaw)
            if pitch:
                rotation = unreal.MathLibrary.compose_rotators(rotation, unreal.Rotator(pitch=pitch))
            actor.set_actor_rotation(rotation, False)

        if name in {"WeatheredBusStop", "YardTransformer", "PitchEntranceSign", "RadiationSign"}:
            prop_kind(key, label, 120, "BUILDING", x, y, z, configure)
        else:
            spawn(unreal.StaticMeshActor, key, label, x, y, z, configure,
                  reconfigure=name in {"AircraftHangar","RuinedTerminal", "RuinedAirportShop"})

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

    def sign(key, label, x, y, z, size=38, yaw=90):
        def configure(actor):
            text = actor.get_component_by_class(unreal.TextRenderComponent)
            text.set_text(label)
            text.set_world_size(size)
            text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)
        spawn(unreal.TextRenderActor, key, label, x, y, z, configure)

    def door(key, label, x, y, floor, width, height, *, yaw=0, roll_up=False,
             reverse=False, finish="timber"):
        def configure(actor):
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)
            actor.configure_door(width*100, height*100, roll_up, reverse,
                                 style_materials[finish], style_materials["dark_trim"])
            if not roll_up:
                kind = "TimberLeaf" if finish == "timber" else "ServiceLeaf"
                if any(word in label.lower() for word in ("lobby", "triage", "clinic", "entrance", "ward")):
                    kind = "GlazedLeaf"
                actor.set_leaf_art(parish_art[kind])
        spawn(door_class, key, label, x, y, floor, configure, reconfigure=True)

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

        if profile == "SECURITY" and any(key.startswith(f"CrossroadsRegion_{site}_")
                                         for site in ("city-police", "north-police", "east-police")):
            profile = "POLICE_ARMORY"

        def configure(actor):
            for prop, amount in {"timber_amount": 0, "stone_amount": 0, **loot}.items():
                actor.set_editor_property(prop, amount)
            actor.set_editor_property("loot_profile", getattr(unreal.SurvivalLootProfile, profile))
            actor.set_editor_property("spawn_chance_percent", chance)
            actor.set_editor_property("loot_name", label.lower())
            if profile == "MAILBOX":
                component = fitted_mesh(actor, cube, (.38, .24, .26))
                component.set_material(0, style_materials["petrol"])
            else:
                component = fitted_mesh(actor, crate_mesh, (.82, .61, .58))
            if profile in {"SECURITY", "POLICE_ARMORY", "MILITARY_ARMORY"} or loot.get("contains_sidearm") or loot.get("contains_rifle"):
                component.set_material(0, military_crate)

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

    def place_ribbon(tag, label, route, surface):
        for index, (origin, edges, lengths, breaks) in enumerate(pavement["pavement_chunks"](route)):
            name, positions, uvs, indices = pavement["mesh_data"](origin, edges, lengths, breaks)
            package = f"{world_art}/Pavement/{name}"
            deck = assets.load_asset(package) if assets.does_asset_exist(package) else None
            if deck is None:
                deck = unreal.CrossroadsWorldBake.create_road_mesh(
                    package, [unreal.Vector(*p) for p in positions],
                    [unreal.Vector2D(*uv) for uv in uvs], indices, surface)
                if not deck or not assets.save_loaded_asset(deck):
                    raise RuntimeError(f"Could not create {package}")

            def paved(actor):
                component = actor.get_component_by_class(unreal.StaticMeshComponent)
                component.set_static_mesh(deck)
                component.set_collision_profile_name("BlockAll")
                component.set_material(0, surface)

            spawn(unreal.StaticMeshActor, f'{tag}_{index}', label, *origin, paved, ("StructuralGround",),
                  relocate=True, reconfigure=True)

    def place_route(route):
        continuous = pavement["is_city_route"](route)
        if continuous:
            place_ribbon(f'CrossroadsPavement_{route["id"]}', "Continuous roadway", route, asphalt)
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

            if not continuous:
                spawn(unreal.StaticMeshActor, key, "Bridge deck" if bridge else "Scavenging route",
                      x, y, z-thickness/2, lambda actor: road_actor(actor, (length+.5, width, thickness), material),
                      ("StructuralGround",), relocate=route["id"].endswith("_access"),
                      reconfigure=route["id"].endswith("_access"))
            if "pier_base_z_m" in segment:
                base = segment["pier_base_z_m"]
                box(key+"_pier", "Bridge pier", x, y, base, 1.2, 2.4, z-thickness-base, material=concrete)
            if continuous and (segment.get("broken_start") or segment.get("broken_end")):
                px, py, pz = segment["start"] if segment.get("broken_start") else segment["end"]
                direction = -1 if segment.get("broken_start") else 1
                for tooth in range(6):
                    offset = width*((tooth+1)/7-.5)
                    box(f"{key}_rebar_{tooth}", "Exposed viaduct reinforcement",
                        px+dy/horizontal*offset+dx/horizontal*direction*.8,
                        py+dx/horizontal*offset-dy/horizontal*direction*.8, pz-.36,
                        1.65+(tooth%3)*.36, .04, .04, material=style_materials["dark_trim"],
                        rotation=(-6*direction,yaw), collision=False)
            if bridge and (not continuous or segment.get("parapet")):
                for side in (-1, 1):
                    offset = side*(width/2-.12)
                    spawn(unreal.StaticMeshActor, f"{key}_rail_{side}", "Bridge parapet",
                          x-dy/horizontal*offset, y-dx/horizontal*offset, z+.45,
                          lambda actor: road_actor(actor, (length if continuous else length+.5, .24, .90), concrete))

    def place_rail(rail):
        art_path = source_art / "Rail"
        rail_meshes = {name: import_asset(art_path / f"SM_{name}.glb", f"{world_art}/Rail/{name}", unreal.StaticMesh)
                       for name in ("FreightTrack", "TramTrack", "SovietLocomotive", "CrossroadsTram")}
        for name in ("SovietLocomotive", "CrossroadsTram"):
            asset = rail_meshes[name]
            if (not unreal.CrossroadsWorldBake.set_static_mesh_collision(asset)
                    or not assets.save_loaded_asset(asset, only_if_is_dirty=True)):
                raise RuntimeError(f"Could not prepare rail vehicle {name}.")

        for track in rail["tracks"]:
            key = f'CrossroadsRail_{track["id"]}'
            freight = track["kind"] == "freight"
            rail_mesh = rail_meshes["FreightTrack" if freight else "TramTrack"]
            bounds = rail_mesh.get_bounding_box()
            extent = bounds.max - bounds.min
            pivot = unreal.Vector((bounds.min.x+bounds.max.x)/2, (bounds.min.y+bounds.max.y)/2, bounds.min.z)
            along_x = extent.x >= extent.y
            span = max(extent.x,extent.y)
            place_ribbon(key+"_ballast", "Rail trackbed and bridge deck", dict(track, segments=[s for s in track["segments"] if not s.get("street")]), gravel)
            segments = track["segments"]
            for i in range(0,len(segments),32):
                ox,oy,oz = segments[i]["start"]
                tiles = []
                for section in segments[i:i+32]:
                    a,b = section["start"],section["end"]
                    dx,dy,dz = b[0]-a[0],a[1]-b[1],b[2]-a[2]
                    pitch = math.degrees(math.atan2(dz, math.hypot(dx,dy)))
                    yaw = math.degrees(math.atan2(dy,dx))
                    rotation = unreal.Rotator(pitch=pitch,yaw=yaw)
                    if not along_x:
                        rotation = unreal.MathLibrary.compose_rotators(unreal.Rotator(yaw=-90),rotation)
                    factor = (100*math.hypot(dx,dy,dz)+4)/span
                    scale = unreal.Vector(factor if along_x else 1, 1 if along_x else factor, 1)
                    center = unreal.Vector(((a[0]+b[0])/2-ox)*100,
                                           (oy-(a[1]+b[1])/2)*100, ((a[2]+b[2])/2-oz)*100)
                    placement = unreal.Transform(unreal.Vector(),rotation,scale)
                    location = center-unreal.MathLibrary.transform_location(placement,pivot)
                    tiles.append(unreal.Transform(location,rotation,scale))
                spawn(facade_class, key+f"_rails_{i//32}", "Instanced rail track", ox,oy,oz,
                      lambda actor: actor.set_static_instances(rail_mesh,tiles), relocate=True,reconfigure=True)

        rail_tools["stamp_infrastructure"](rail,box,sign,style_materials)
        for item in rail["rolling_stock"]:
            x,y,z = item["position"]
            freight = item["kind"]=="freight"
            mesh = rail_meshes["SovietLocomotive" if freight else "CrossroadsTram"]
            dimensions = (12.5,2.93,4.18) if freight else (10.39,2.53,4.06)

            def train(actor):
                fitted_mesh(actor,mesh,dimensions,item["yaw"],align_long_axis=True)
                yaw = actor.get_actor_rotation().yaw
                rotated = unreal.MathLibrary.compose_rotators(
                    unreal.Rotator(yaw=yaw-item["yaw"]),
                    unreal.Rotator(yaw=item["yaw"],pitch=item["pitch"]))
                actor.set_actor_rotation(rotated,False)
            rail_top = .39 if freight else .176
            spawn(unreal.StaticMeshActor, "CrossroadsRail_stock_"+item["id"], item["label"],
                  x,y,z+rail_top+dimensions[2]/2,train,relocate=True,reconfigure=True)

    def place_hospital(key, x, y, floor):
        tower = key+"_medical_tower"
        civic["hall"](tower, x, y, floor, 54, 38, 12, 0, "hospital", "Crossroads Central Hospital",
                     box, door, cache, encounter, sign, hero_art, style_materials)
        departments = ("Emergency", "Imaging", "Surgery", "Recovery", "Intensive Care",
                       "Cardiology", "Neurology", "Orthopedics", "Pediatrics", "Maternity",
                       "Inpatient Ward", "Rehabilitation")
        for level, department in enumerate(departments):
            z = floor+level*3.65
            sign(f"{tower}_directory_{level}", f"{level+1:02}  {department.upper()}",
                 x+2, y+2.6, z+2.25)
            if department in {"Imaging", "Surgery", "Intensive Care"}:
                furnishing(f"{tower}_station_{level}", department+" console", "desk",
                           x-2, y-4, z, (3.1,1.1,1.35), 180)
                furnishing(f"{tower}_storage_{level}", "Clinical equipment cabinet", "files",
                           x+1.5, y-5, z, (1.5,.75,1.9), 90)
            elif department in {"Emergency", "Rehabilitation"}:
                for i in range(3):
                    box(f"{tower}_seats_{level}_{i}", "Patient waiting bench",
                        x-8+i*3.2, y-7, z, 2.6,.62,.46,material=style_materials["timber"])
            else:
                furnishing(f"{tower}_linen_{level}", "Ward linen shelves", "shelf",
                           x-9, y+2, z, (3.2,.8,1.8), 0)
            for i, dx in enumerate((-9,-5,-1)):
                treatment_bed(f"{tower}_bed_{level}_{i}", department+" treatment bay", x+dx, y+6, z)
                box(f"{tower}_screen_{level}_{i}", "Privacy screen", x+dx+2, y+6,
                    z, .12, 3, 1.8, material=style_materials["petrol"])
        cache(tower+"_triage_supplies", "Hospital dressing supplies", x+2, y-7, floor,
              cloth_amount=5, dressing_schematic_amount=1)
        migrate_landmark(key, tower, (key+"_slab",))

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
            component.set_material(0, warehouse_mesh.get_material(0))
            component.set_collision_profile_name("BlockAll")
            scale = (warehouse_depth_scale, 1, 1) if yaw % 180 else (1, warehouse_depth_scale, 1)
            actor.set_actor_scale3d(unreal.Vector(*scale))
            actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)

        spawn(unreal.StaticMeshActor, key+"_doorway_shell", "Warehouse loading hall", x, y, floor, configure,
              relocate=True, reconfigure=True)
        remove_generated(key+"_warehouse", key+"_static")
        for opening in warehouse_doors:
            side_opening = opening["yaw"] % 180 != 0
            door_width = opening["width"] * (warehouse_depth_scale if side_opening else 1)
            door(f"{key}_door_{opening['id']}", opening["label"],
                 x+opening["x"], y+opening["y"]*warehouse_depth_scale,
                 floor, door_width, opening["height"], yaw=opening["yaw"],
                 roll_up=opening.get("roll_up", False), reverse=opening.get("reverse", False),
                 finish="petrol")
        site_tools["warehouse_yard"](box, key, x, y, floor, style_materials, warehouse_depth_scale)
        box(key+"_signboard", "Warehouse signboard", x, y-3.54*warehouse_depth_scale, floor+3.26, 5.2, .1, .6,
            material=style_materials["dark_trim"])
        sign(key+"_sign", "LOADING 04", x, y-3.61*warehouse_depth_scale, floor+3.69)
        cache(key+"_materials", "Warehouse repair materials", x-8, y+1.5, floor,
              stone_amount=30, timber_amount=24)
        cache(key+"_supplies", "Warehouse storage crate", x+8, y+1.5, floor,
              cloth_amount=3, water_amount=2)
        encounter(key+"_runner", "Loading yard runner", x-9.4, y-7.2, floor, runner=True, relocate=True)
        encounter(key+"_walker", "Warehouse walker", x+4, y+.75, floor, relocate=True)

    interior_meshes = {"desk": office_meshes["OfficeWorkstation"], "files": office_meshes["OfficeFiles"],
                       "shelf": shelf_mesh, "bed": bed_mesh}

    def tower(key, x, y, floor, yaw, finish, parts):
        # Batch repeated floors, walls and furniture by mesh into persistent local instances.
        batches = {}
        meshes = dict(interior_meshes, slab=cube, wall=cube)
        for name, (u,v,h), dimensions, pitch, heading in parts:
            bounds = meshes[name].get_bounding_box()
            size = bounds.max-bounds.min
            scale = unreal.Vector(*(m*100/span for m,span in zip(dimensions,(size.x,size.y,size.z))))
            rotation = unreal.Rotator(pitch=pitch,yaw=heading)
            transform = unreal.Transform(unreal.Vector(),rotation,scale)
            center = unreal.Vector(u*100,-v*100,h*100)
            offset = unreal.MathLibrary.transform_location(transform,(bounds.min+bounds.max)*.5)
            batches.setdefault(name,[]).append(unreal.Transform(center-offset,rotation,scale))
        for name, transforms in batches.items():
            def configure(actor):
                actor.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
                actor.set_static_instances(meshes[name],transforms)
                component = actor.get_component_by_class(unreal.InstancedStaticMeshComponent)
                if component is None:
                    raise RuntimeError(f"Tower {key} has no instanced mesh component for {name}.")
                component.set_collision_profile_name("BlockAll")
                if name in {"slab","wall"}:
                    component.set_material(0,style_materials["concrete" if name=="slab" else finish])
            spawn(facade_class,key+"_interior_"+name,"Tower "+name,x,y,floor,configure,
                  relocate=True,reconfigure=True)

    def roof_ladder(key, x, y, floor, roof, yaw):
        span = roof-floor+1
        def configure(actor):
            component = fitted_mesh(actor,parish_art["RoofLadder"],(.75,.18,span),yaw)
            component.set_collision_profile_name("CrossroadsRoofLadder")
        spawn(unreal.StaticMeshActor,key,"Roof ladder",x,y,floor+span/2,configure,
              tags=("CrossroadsRoofLadder",),relocate=True,reconfigure=True)

    def church(key, x, y, floor, yaw, width, depth):
        angle = math.radians(yaw)
        def xy(u,v):
            return x+u*math.cos(angle)+v*math.sin(angle), y-u*math.sin(angle)+v*math.cos(angle)
        def part(tag,label,u,v,z,w,d,h,finish):
            box(key+tag,label,*xy(u,v),floor+z,w,d,h,material=style_materials[finish],rotation=(0,yaw))
        front = -depth/2
        stone = "classical_civic"
        for side in (-1,1):
            part(f"_side_{side}","Nave side wall",side*(width/2-.25),0,0,.5,depth,6.8,stone)
            part(f"_entrywall_{side}","Narthex entrance",side*(width+3.2)/4,front+.25,0,
                 (width-3.2)/2,.5,6.8,stone)
            door(f"{key}_leaf_{side}","Parish oak door",*xy(side*.8,front),floor,1.6,2.65,
                 yaw=yaw,reverse=side>0)
            for i in range(6):
                v=depth*(-.30+i*.095)
                u=side*width*.245
                part(f"_pew_{side}_{i}","Oak pew",u,v,0,width*.28,.65,.46,"timber")
                part(f"_back_{side}_{i}","Pew back",u,v-.29,.45,width*.28,.10,.52,"timber")
            for i in range(3):
                part(f"_pier_{side}_{i}","Nave pier",side*width*.405,depth*(i-1)*.29,
                     0,.75,.75,6.8,stone)
        part("_lintel","Entrance lintel",0,front+.25,2.65,3.2,.5,4.15,stone)
        remove_generated(key+"_masonry")
        for side in (-1, 1):
            for index in range(5):
                part(f"_buttress_{side}_{index}", "Nave masonry buttress",
                     side*width/2, depth*(index-2)*.21, 0, .8, .9, 6.7, stone)
        for side in (-1, 1):
            part(f"_portal_{side}", "Parish entrance pier", side*2.35, front-.72,
                 0, .72, 1.4, 3.8, stone)
        part("_portico", "Parish entrance canopy", 0, front-.75, 3.8, 5.8, 2.1, .38, stone)
        part("_apse","Sanctuary wall",0,depth/2-.25,0,width,.5,6.8,stone)
        part("_ceiling","Nave ceiling",0,0,6.8,width,depth,.28,"timber")
        part("_altar","Stone altar",0,depth*.34,0,3.4,1.3,1.1,stone)
        part("_lectern","Oak lectern",-width*.22,depth*.33,0,.85,.72,1.18,"timber")
        # Keep the textured scan above the occupied nave and both entrance leaves.
        def roof(actor):
            component=fitted_mesh(actor,parish_art["ParishUpperChurch"],(width+.6,depth+.6,21),yaw)
            component.set_collision_profile_name("NoCollision")
        spawn(unreal.StaticMeshActor,key+"_upper","Orthodox church upper exterior",x,y,floor+7+10.5,
              roof,relocate=True,reconfigure=True)
        cache(key+"_vestry","Vestry supplies",*xy(width*.35,depth*.40),floor,profile="GENERAL")
        sign(key+"_name","CENTRAL PARISH",*xy(0,front-.13),floor+3.4,yaw=yaw+90)
        encounter(key+"_walker","Parish walker",*xy(-width*.38,depth*.28),floor,relocate=True)

    def furnishing(key, label, mesh_name, x, y, floor, size, yaw):
        def configure(actor):
            fitted_mesh(actor, interior_meshes[mesh_name], size, yaw, align_long_axis=True)
        spawn(unreal.StaticMeshActor, key, label, x, y, floor+size[2]/2, configure,
              relocate=True, reconfigure=True)

    def dress_industrial(key, x, y, floor, width, depth, warehouse=False, bridge_side=None):
        if warehouse:
            # Rear dispatch apron, beyond the roll-up doors and within the site slab.
            wreck(key+"_dispatch_truck", x-9, y+13, floor, 6, "RustedTruck")
            wreck(key+"_dispatch_van", x+9, y+14, floor, 176, "AbandonedVan")
            for side in (-1, 1):
                furnishing(f"{key}_dispatch_shelf_{side}", "Dispatch shelving", "shelf",
                           x+side*20, y+7, floor, (4.2,1.1,2.1), 90)
                cache(f"{key}_dispatch_load_{side}", "Outbound freight", x+side*20,
                      y+10.5, floor, profile="BUILDING")
        elif bridge_side is not None:
            west_works = "west-factory" in key
            wreck(key+"_shift_car", x-10, y+17.7, floor, 0,
                  "BurnedSedan" if west_works else "BurnedCompact")
            for side in (-1, 1):
                px, py = x+side*width*.21, y-depth*.12
                machine = factory.get("machine")
                if machine:
                    def fit_machine(actor):
                        fitted_mesh(actor, machine, (3.2,1.65,2.15), 90, align_long_axis=True)
                    spawn(unreal.StaticMeshActor, f"{key}_machine_{side}", "Workshop machinery",
                          px, py, floor+1.075, fit_machine, relocate=True, reconfigure=True)
                else:
                    district_mesh(f"{key}_machine_{side}", "Electrical switchgear", "SubstationSwitchgear",
                                  px, py, floor, (3.2,1.65,2.15), yaw=90)
            for index, (kind, dx, dy, dims) in enumerate((
                ("drum",7,12,(.65,.65,.94)), ("drum",7.8,12.2,(.65,.65,.94)),
                ("drum",7.3,13,(.65,.65,.94)), ("debris",12,14,(2.3,1.2,.27)),
                ("spill",width*.21-1,-depth*.12,(2.1,1.3,.018)),
            )):
                if kind in factory:
                    def detail(actor):
                        comp = fitted_mesh(actor,factory[kind],dims,index*31,align_long_axis=True)
                        if kind in {"spill","debris"}:
                            comp.set_collision_profile_name("NoCollision")
                    spawn(unreal.StaticMeshActor,f"{key}_detail_{index}",kind.title(),x+dx,y+dy,
                          floor+.035+dims[2]/2,detail,relocate=True,reconfigure=True)
            cache(key+"_service_parts", "Repair parts", x+width*.18, y+depth*.35,
                  floor, profile="BUILDING")
        desired = set()
        if "wall" in factory:
            skin = factory["wall"]
            bounds = skin.get_bounding_box()
            span = max(bounds.max.x-bounds.min.x, bounds.max.y-bounds.min.y)/100

            def clad(name, cx, cy, length, base, height, yaw=0):
                along_x = yaw % 180 == 0
                count = max(1, round(length/span))
                bay = length/count
                for i in range(count):
                    offset = (i+.5)*bay - length/2
                    tag = f"{key}_fab_{name}_{i}"
                    desired.add(tag)

                    def wall(actor):
                        component = fitted_mesh(actor, skin, (bay, .12, height), yaw, align_long_axis=True)
                        component.set_collision_profile_name("NoCollision")

                    spawn(unreal.StaticMeshActor, tag, "Industrial wall cladding",
                          cx + (offset if along_x else 0), cy + (0 if along_x else offset),
                          floor+base+height/2, wall, relocate=True, reconfigure=True)

            if not warehouse:
                opening = 4.5
                clad("rear", x, y+depth/2+.055, width, 0, 5.66, 180)
                clad("front_upper", x, y-depth/2-.055, width, 7.32, 1.08)
                clad("rear_upper", x, y+depth/2+.055, width, 7.32, 1.08, 180)
                clad("door_head", x, y-depth/2-.055, opening, 3.1, 2.56)
                for side in (-1, 1):
                    clad(f"front_{side}", x+side*(width+opening)/4, y-depth/2-.055,
                         (width-opening)/2, 0, 5.66)
                    if side == bridge_side:
                        continue
                    clad(f"side_{side}", x+side*(width/2+.055), y, depth, 0, 8.4, side*90)
            else:
                # The source shell and its collision define all twelve shutter and staff openings.
                for side in (-1, 1):
                    clad(f"canopy_fascia_{side}", x, y+side*3.67*warehouse_depth_scale, 27.6, 3.39, .36,
                         0 if side<0 else 180)

        for kind, dx, dy, dimensions, health in (
            ("rack", -6, .45, (1.8, .90, 1.95), 110),
            ("forklift", 7, -1.5 if warehouse else -depth*.38, (2.60, 1.26, 2.20), 235),
            ("drum", -5, -1.9, (.60, .60, .85), 75),
            ("pallet", 6, 2.7, (1.21, .96, .15), 50),
        ):
            if kind not in factory:
                continue
            def configure(actor):
                fitted_mesh(actor, factory[kind], dimensions, align_long_axis=True)
            tag = f"{key}_fab_{kind}"
            desired.add(tag)
            prop_kind(tag, f"Industrial {kind}", health, "BUILDING",
                      x+dx, y+dy, floor+dimensions[2]/2, configure)
        remove_generated(*(tag for tag in actor_tags if tag.startswith(key+"_fab_") and tag not in desired))

    housing = {}
    for style in ("Villa", "TimberStone", "European"):
        housing[style] = []
        for source in sorted((source_art / "Housing" / style).glob("*.glb")):
            mesh = import_asset(source, f"{world_art}/Housing/{style}", unreal.StaticMesh,
                                refresh_if_source_changed=True)
            if not unreal.CrossroadsWorldBake.set_static_mesh_collision(mesh):
                raise RuntimeError(f"Could not prepare residential collision: {source.name}")
            housing[style].append(mesh)

    fixtures = {}
    for name in ("Refrigerator", "DocumentSafe", "FloorSafe"):
        fixtures[name] = import_asset(source_art/"Household"/f"SM_{name}.glb",
                                      f"{world_art}/Household", unreal.StaticMesh)
        if not unreal.CrossroadsWorldBake.set_static_mesh_collision(fixtures[name]):
            raise RuntimeError(f"Could not prepare {name} collision")

    housing_styles = {"western-estates":"Villa", "lakehouse-east":"Villa",
                      "middle-suburbs":"TimberStone", "south-suburbs":"European"}
    neighborhoods = [(site["plot_xy_m"], housing_styles[site["id"]]) for site in plan["sites"]
                     if site["id"] in housing_styles]

    def home(key,x,y,z,yaw,style=None):
        if style is None:
            style = min(neighborhoods,key=lambda item: math.dist((x,y),item[0]))[1]
        angle = math.radians(yaw)
        def furnish(tag,label,mesh,u,v,size,profile,health):
            if housing[style]:
                u *= 1.85
                v *= 1.85
            def setup(actor):
                fitted_mesh(actor,mesh,size,yaw)
            prop_kind(key+"_"+tag,label,health,profile,x+u*math.cos(angle)+v*math.sin(angle),
                      y-u*math.sin(angle)+v*math.cos(angle),z+size[2]/2,setup)
        furnish("fridge","Kitchen refrigerator",fixtures["Refrigerator"],1.55,-1.6,
                (.86,.76,1.84),"FOOD",95)
        furnish("dresser","Bedroom drawers",office_meshes["OfficeDrawer"],-2.3,2.0,
                (.95,.5,.84),"GENERAL",65)
        if random.Random(key+"_safe").random() < (.62 if style=="Villa" else .26):
            heavy = style=="Villa"
            furnish("safe","Floor safe" if heavy else "Document safe",
                    fixtures["FloorSafe" if heavy else "DocumentSafe"],1.5,2.4,
                    (.66,.62,.85) if heavy else (.48,.42,.43),"SECURITY" if heavy else "OFFICE",
                    650 if heavy else 300)
        if housing[style]:
            mesh = random.Random(key).choice(housing[style])
            bounds = mesh.get_bounding_box()
            size = bounds.max-bounds.min
            if min(size.x,size.y,size.z) <= 0:
                raise ValueError(f"Empty residential mesh: {mesh.get_name()}")
            width,depth = {"Villa":(36,29),"TimberStone":(31,28),"European":(29,24)}[style]
            factor = min(width/size.x,depth/size.y)
            dims = (size.x*factor,size.y*factor,size.z*factor)
            def install(actor):
                fitted_mesh(actor,mesh,dims,yaw)
            spawn(unreal.StaticMeshActor,key+"_residence",style+" residence",x,y,z+dims[2]/2,
                  install,relocate=True,reconfigure=True)
            return

        # Keep each house traversable until its furnished architectural model is installed.
        angle = math.radians(yaw)
        def oriented(fn):
            def place(tag,label,u,v,floor,*args,**options):
                if fn is box:
                    pitch,heading = options.pop("rotation",(0,0))
                    options["rotation"] = (pitch,heading+yaw)
                elif fn is door:
                    options["yaw"] = options.get("yaw",0)+yaw
                return fn(tag,label,x+u*math.cos(angle)+v*math.sin(angle),
                          y-u*math.sin(angle)+v*math.cos(angle),floor,*args,**options)
            return place
        finish = {"Villa":"stucco_metal","TimberStone":"timber","European":"brick_hardwood"}[style]
        interiors["place_site"](oriented(box),oriented(door),ramp,oriented(cache),oriented(encounter),
            sign, storefront_window,treatment_bed,furniture,site_tools["building_shell"],
            site_tools["double_door"],key,dict(kind="suburbs",id=key,name="Residence",architecture=finish),
            0,0,z,20,15,style_materials,scatter_caches)

    cylinder=unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
    def basin(key,x,y,z,diameter,height,cooling):
        def round_part(tag,label,bottom,width,rise,material):
            def setup(actor):
                component=fitted_mesh(actor,cylinder,(width,width,rise))
                component.set_material(0,material)
            spawn(unreal.StaticMeshActor,key+tag,label,x,y,bottom+rise/2,setup,relocate=True,reconfigure=True)
        if cooling:
            def shell(actor):
                component=fitted_mesh(actor,district_meshes["CoolingTower"],(diameter,diameter,height+5))
                component.set_material(0,hangar_finish)
            spawn(unreal.StaticMeshActor,key+"_shell","Evaporative cooling tower",x,y,z+(height+5)/2,
                  shell,relocate=True,reconfigure=True)
            return
        round_part("_base","Clarifier tank",z,diameter,height,concrete)
        round_part("_surface","Settling basin surface",z+height,diameter*.91,.08,style_materials["glazing"])
        box(key+"_walk","Clarifier service bridge",x,y,z+height+.18,diameter,1,.22,material=concrete)

    has_start = any(isinstance(actor, unreal.PlayerStart) for actor in existing)
    for landscape in landscapes:
        tags = list(landscape.get_editor_property("tags"))
        ground = unreal.Name("StructuralGround")
        if ground not in tags:
            landscape.set_editor_property("tags", [*tags, ground])

    sx, sy = plan["downtown_xy_m"]
    for tag, y in (("CrossroadsClimateNorth", world_depth), ("CrossroadsClimateCity", sy), ("CrossroadsClimateSouth", 0)):
        spawn(unreal.TargetPoint, tag, tag, sx, y, 0, lambda actor: None, always_loaded=True)

    for site in plan["sites"]:
        key = f'CrossroadsRegion_{site["id"]}'
        if "footprint_m" not in site:
            continue
        x, y = site["plot_xy_m"]
        w, d = site["footprint_m"]
        z = site["foundation_z_m"]
        name = site["name"]
        resized_site = key if site["kind"] in {
            "highrise", "shops", "village", "suburbs", "ranch", "clinic", "hospital", "civic", "industry"
        } and site["id"] not in {
            "stadium", "hangars", "city-hospital", "warehouses", "city-nightlife"
        } else None
        if site["kind"] in {"retail_strip", "hardware"} or site["id"] in {
            "airport", "hangars", "warehouses", "airport-services", "south-base"
        }:
            resized_site = key
        if site["id"] not in {"stadium", "university", "city-police", "city-hall"}:
            box(key+"_slab", name+" slab", x, y, z-.6, w, d, .8, ground=True,
                material=style_materials["grass"] if site["kind"] == "suburbs" else concrete)
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
        elif site["id"] in {"south-base","water-treatment","power-northeast","power-southwest"}:
            compound=key+"_compound"
            base=z+.2
            def facility(tag,label,cx,cy,bw,bd,bh):
                front=site_tools["building_shell"](box,compound+tag,cx,cy,base,bw,bd,bh,2.4,
                                                   style_materials["StationBrick"],concrete)
                site_tools["double_door"](door,compound+tag+"_entry",label+" entrance",cx,front-.04,
                                          base,2.4,2.35,"petrol")
                sign(compound+tag+"_name",label,cx,front-.22,base+3.05)
                furnishing(compound+tag+"_desk","Operations desk","desk",cx-bw*.23,cy,base,(2,1,1.2),0)
            if site["id"]=="south-base":
                landmarks["aircraft_hangar"](box,landmark_mesh,door,cache,compound+"_motor_pool",
                                             x-30,y+3,base,concrete,scale=1.35)
                for i,title in enumerate(("BARRACKS","COMMAND","SUPPLY")):
                    facility(f"_block_{i}",title,x+26,y-22+i*22,35,15,4)
                district_mesh(compound+"_medevac","Abandoned military ambulance","MilitaryAmbulance",
                              x-15,y-24,base,(5.4,2.2,2.7))
                sign(compound+"_gate_id","SOUTH GARRISON  /  MAIN GATE",x+9,y-d/2-.6,base+3.3,size=49)
                for i in range(3):
                    for side in (-1,1):
                        furnishing(f"{compound}_bunk_{side}_{i}","Barracks cot","bed",
                                   x+26+side*10,y-27+i*4,base,(2.25,1.05,.68),0)
                communications_art(compound+"_comms","Garrison communications mast","RepeaterMast",
                                   x+4,y+25,base,(3,3,18))
                cache(compound+"_stores","Base stores",x+15,y+23,base,profile="MILITARY_ARMORY")
            else:
                facility("_control","CONTROL ROOM",x-w*.29,y-d*.20,22,14,5)
                if site["id"]=="power-northeast":
                    facility("_turbines","TURBINE HALL",x+12,y,42,24,9)
                    rx,ry,bed,left,right=plan["hydro_intake"]
                    crest=max(left,right)+2
                    box(compound+"_weir","Headwater diversion dam",rx,ry,bed-1,36,5,crest-bed+1,material=concrete)
                    length=math.hypot(x-28-rx,y+12-ry)
                    box(compound+"_conduit","Intake conduit", (x-28+rx)/2,(y+12+ry)/2,
                        min(crest,base)+.2,length,2.7,2,material=concrete,
                        rotation=(0,math.degrees(math.atan2(ry-y-12,x-28-rx))))
                    for i in range(3):
                        box(f"{compound}_intake_{i}","River intake channel",x-28,y+8+i*4,
                            base,30,2.8,1.5,material=concrete)
                elif site["id"]=="water-treatment":
                    facility("_pumps","PUMP HOUSE",x-22,y+12,19,12,4)
                    for i in range(3):
                        basin(compound+f"_tank_{i}",x+2+i*14,y+3,base,12,2,False)
                else:
                    facility("_turbines","GENERATOR HALL",x-25,y+15,48,20,9)
                    core_x,core_y=x+1,y+1
                    def dome(actor):
                        sphere=unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
                        component=fitted_mesh(actor,sphere,(17,17,15))
                        component.set_material(0,concrete)
                    spawn(unreal.StaticMeshActor,compound+"_containment","Reactor containment",
                          core_x,core_y,base+7.5,dome,relocate=True,reconfigure=True)
                    for i in range(2):
                        basin(compound+f"_tower_{i}",x+22,y+(i-.5)*33,base,26,24,True)
                cache(compound+"_service","Maintenance parts",x-w*.29-6,y-d*.20+3,base,profile="BUILDING")
            district_mesh(compound+"_watch","Abandoned watchtower","GuardTower",x-w/2+5,y-d/2+5,
                          base,(6,6,10))
            for side in (-1,1):
                box(f"{compound}_side_{side}","Compound fence",x+side*(w/2-.3),y,base,.6,d,3.2,material=concrete)
                box(f"{compound}_front_{side}","Checkpoint barrier",x+side*(w+8)/4,y-d/2,base,
                    (w-8)/2,.8,3.2,material=concrete)
            box(compound+"_back","Compound rear barrier",x,y+d/2,base,w,.8,3.2,material=concrete)
            migrate_landmark(key,compound,(key+"_slab",))
        elif site["id"] == "air-reserve":
            apron=key+"_squadron"
            field=plan["facilities"]["airport"]
            ty=field["y"]+airfield_dimensions["TAXIWAY_Y"]
            for i in range(3):
                hx=x+(i-1)*48
                landmarks["aircraft_hangar"](box,landmark_mesh,door,cache,f"{apron}_{i}",
                                             hx,y,z+.22,concrete,scale=1.25)
                ramp(f"{apron}_lane_{i}","Reserve squadron taxi lane",hx,ty,y-17,
                     field["datum"]+(hx-field["x"])*field["slope"],z+.22,18,concrete)
            migrate_landmark(key,apron,(key+"_slab",))
        elif site["id"] == "hangars":
            new_key = key+"_operations"
            landmarks["aircraft_hangar"](box, landmark_mesh, door, cache, new_key,
                                          x-45, y, z+.215, concrete)
            landmarks["aircraft_hangar"](box, landmark_mesh, door, cache, new_key+"_east",
                                         x+45, y, z+.215, concrete)
            taxi = plan["facilities"]["airport"]
            taxi_y = taxi["y"]+airfield_dimensions["TAXIWAY_Y"]
            for side in (-1, 1):
                hx = x+side*45
                ramp(f"{new_key}_taxilane_{side}", "Maintenance apron taxi lane", hx, taxi_y, y-32,
                     taxi["datum"]+(hx-taxi["x"])*taxi["slope"], z+.21, 42, concrete)
            box(new_key+"_landside_wall","Restricted apron wall",x,y+39,z+.2,174,1,3.2,material=concrete)
            district_mesh(new_key+"_overwatch","Air operations watchtower","GuardTower",x-81,y+38,
                          z+.2,(5,5,9))
            district_mesh(new_key+"_rescue","Flight line ambulance","MilitaryAmbulance",x+4,y+34,
                          z+.2,(5.4,2.2,2.7))
            cache(new_key+"_armory", "Military weapon crate", x+45-21, y+14, z+.215,
                  profile="MILITARY_ARMORY")
            sign(new_key+"_sign", "JOINT AIR OPERATIONS", x, y+32, z+3)
            migrate_landmark(key, new_key, (key+"_slab",))
        elif site["id"] in {"city-police", "city-hall", "university"}:
            new_key = key+"_landmark"
            resized_site = new_key
            if site["id"] == "university":
                civic["campus"](new_key, x, y, z+.2, box, door, cache, encounter, sign, campus_art, wreck,
                                style_materials)
                remove_generated(*(tag for tag in actor_tags if tag.startswith(new_key+"_")
                                   and tag not in site_keys))
            else:
                role = "police" if site["id"] == "city-police" else "civic"
                civic["place_landmark"](new_key, role, x, y, z+.2, box, door, cache, encounter,
                                        sign, hero_art, style_materials)
            migrate_landmark(key, new_key)
        elif site["id"] == "city-towers":
            tower_key = key+"_landmark"
            def tower_art(tag, role, tx, ty, base, width, depth, height, story, yaw, opening):
                if hero_art(tag, role, tx, ty, base, width, depth, height, story, yaw, opening):
                    remove_generated(tag+"_office_panels")
                else:
                    facade(tag+"_office_panels", "OfficeFacade", tx, ty, base, width, depth,
                           height, yaw, story, opening, bundled=True)
                return True
            civic["hall"](tower_key, x-4.4, y, z+.2, w*.83, d*.86, 22, 0, "tower",
                         name, box, door, cache, encounter, sign, tower_art, style_materials)
            for level in range(22):
                scatter_caches(f"{tower_key}_office_{level}", x-4.4, y, z+.2+level*3.65,
                               ((-9,-6), (-4,-7), (-1,8), (3,7)), (17,9,9,4,47,14), 96)
            encounter(tower_key+"_upper_walker", "Upper office walker", x-11, y-5, z+.2+21*3.65)
        elif site["id"] == "city-hospital":
            resized_site = key
            place_hospital(key, x, y, z+.2)
        elif site["id"] in {"west-factory", "east-factory"}:
            works = key+"_plant"
            for side in (-1, 1):
                hall = f"{works}_{side}"
                hx = x+side*28
                inward = -side
                def shell(place, tag, cx, cy, base, width, depth, height, opening, wall, roof, **options):
                    def split_wall(actor_key, label, px, py, bottom, sw, sd, sh, **kwargs):
                        if actor_key != f"{tag}_side_{inward}":
                            return place(actor_key,label,px,py,bottom,sw,sd,sh,**kwargs)
                        for i,(lo,hi) in enumerate(((-sd/2,-6),(-2,sd/2))):
                            place(f"{actor_key}_{i}",label,px,py+(lo+hi)/2,bottom,sw,hi-lo,sh,**kwargs)
                        place(actor_key+"_sill",label,px,py-4,bottom,sw,4,4,**kwargs)
                        place(actor_key+"_head",label,px,py-4,bottom+7,sw,4,sh-7,**kwargs)
                    return site_tools["building_shell"](split_wall,tag,cx,cy,base,width,depth,height,
                                                       opening,style_materials["StationBrick"],roof,**options)
                interiors["place_site"](box,door,ramp,cache,encounter,sign,storefront_window,
                    treatment_bed,furniture,shell,site_tools["double_door"],hall,
                    dict(site, name="Machine shop" if side<0 else "Packing and dispatch"),
                    hx,y,z+.2,48,42,style_materials,scatter_caches)
                dress_industrial(hall,hx,y,z+.2,43.2,36.12,bridge_side=inward)
                box(hall+"_finish","Sealed production floor",hx,y,z+.2,42.7,35.6,.025,
                    material=concrete,ground=True)
                for i in range(3):
                    box(f"{hall}_mat_{i}","Anti-fatigue mat",hx-8+i*5,y-6,z+.23,
                        2.4,1.15,.035,material=style_materials["dark_trim"])
                    box(f"{hall}_pallet_{i}","Shipping pallet",hx-8+i*3,y+12,z+.23,
                        1.6,1.2,.15,material=style_materials["timber"])
                    box(f"{hall}_crate_{i}","Weathered cargo crate",hx-8+i*3,y+12,z+.38,
                        1.3,1,.95,material=style_materials["timber"])
                ramp(hall+"_access","Gantry access",hx+inward*19.5,y-16,y-6,z+.23,z+4.2,3,concrete)
            box(works+"_bridge","Transfer bridge deck",x,y-4,z+3.93,21,4,.27,ground=True,material=concrete)
            for side in (-1,1):
                box(f"{works}_rail_{side}","Bridge guard",x,y-4+side*1.92,z+4.2,13,.16,1.15,
                    material=style_materials["dark_trim"])
            box(works+"_cover","Bridge canopy",x,y-4,z+7.2,13,4.3,.19,material=style_materials["dark_trim"])
            migrate_landmark(key, works, (key+"_slab",))
        elif site["id"] == "warehouses":
            resized_site = key
            district = key+"_distribution"
            for row in (-1, 1):
                for bay in (-1, 0, 1):
                    hall = f"{district}_{row}_{bay}"
                    hx, hy = x+bay*48, y+row*22
                    place_warehouse(hall, hx, hy, z+.2)
                    dress_industrial(hall, hx, hy, z+.2, 44, 40, warehouse=True)
                    sign(hall+"_sign", f"DISPATCH  {1+bay+1+(3 if row>0 else 0):02}", hx,
                         hy-3.61*warehouse_depth_scale, z+.2+3.69)
            migrate_landmark(key, district, (key+"_slab",))
            remove_blockout(key)
        elif site["id"] in {"west-entertainment", "east-entertainment"}:
            district = key+"_quarter"
            venues = (("The Playhouse", "Stage Door Cafe"), ("Riverside Range", "Listening Room"),
                      ("The Comedy Cellar", "Late Show Diner"), ("Crossroads Casino", "Union Nightclub"))
            if site["id"] == "east-entertainment":
                venues = (("Picture House", "Matinee Cafe"), ("Music Hall", "Backstage Bar"),
                          ("Jazz Club", "Midnight Kitchen"), ("Branch Library", "Corner Bistro"))
            for i, pair in enumerate(venues):
                vx, vy = x+(-24 if i%2==0 else 24), y+(-17 if i<2 else 17)
                block = f"{district}_{i}"
                site_tools["nightlife_layout"](box, door, cache, encounter, sign, storefront_window,
                                               block, vx, vy, z+.2, style_materials, pair)
                for side in (-1, 1):
                    facade(f"{block}_skin_{side}", "Storefront", vx+side*7.8, vy+1.7, z+.2,
                           13.6, 12, 3.5, 0, 2.5, 2.8, bundled=True)
            box(district+"_street", "Entertainment quarter street", x, y, z+.2, w, 9, .06,
                ground=True, material=asphalt)
            migrate_landmark(key, district, (key+"_slab",))
        elif site["kind"] in {"retail_strip","hardware"}:
            shops = (("Hardware","BUILDING"),("Supermarket","FOOD"),("Clothing Outlet","GENERAL"),
                     ("Bookshop","OFFICE"),("Electronics","OFFICE"))
            if site["kind"] == "hardware":
                shops = shops[:1]
            for index,(title,profile) in enumerate(shops):
                unit = f"{key}_unit_{index}"
                size = ((28,21,5.6),(38,27,6.8),(27,21,5.2),(22,18,4.8),(30,21,5.6))[index]
                width,depth,roof = size
                if len(shops)==1:
                    cx,cy=x,y+2
                else:
                    # Front-row clearances are 50 m between adjacent side walls.
                    dx,dy=((-83,-41),(0,-41),(82.5,-41),(-39,33),(39,33))[index]
                    cx,cy=x+dx,y+dy
                front = site_tools["building_shell"](box,unit,cx,cy,z+.2,width,depth,roof,2.5,
                                                     style_materials["StationBrick"],concrete)
                site_tools["double_door"](door,unit+"_entry",title+" entrance",cx,front-.04,z+.2,
                                          2.5,2.35,"petrol")
                remove_generated(unit+"_skin")
                for side in (-1,1):
                    for bay in range(max(1,int((width/2-2)/3.25))):
                        storefront_window(f"{unit}_bay_{side}_{bay}",cx+side*(3.25+bay*3.25),front,z+.2)
                box(unit+"_canopy","Shop entrance canopy",cx,front-1.35,z+3.3,
                    width*.68,2.9,.22,material=style_materials["dark_trim"])
                box(unit+"_fascia","Store fascia",cx,front-.25,z+3.68,width*.78,.4,.88,
                    material=style_materials["StationBrick"])
                box(unit+"_footwalk","Store entrance sidewalk",cx,front-1.9,z+.2,width+2,4,.10,
                    material=concrete,ground=True)
                sign(unit+"_sign",title.upper(),cx,front-.49,z+4.18,size=30)
                for side in (-1,1):
                    tag = f"{unit}_stock_{side}"
                    px = cx+side*width*.26
                    if title=="Electronics":
                        furnishing(tag,"Electronics display","desk",px,cy,z+.2,(2.4,1.1,1.2),90)
                    elif title=="Clothing Outlet":
                        box(tag,"Clothing display table",px,cy,z+.2,1.2,3,.85,
                            material=style_materials["timber"])
                    else:
                        furnishing(tag,title+" shelving","shelf",px,cy,z+.2,(3.2,.9,1.9),90)
                cache(unit+"_delivery",title+" delivery",cx-width*.31,cy+4.4,z+.2,profile=profile)
                district_mesh(unit+"_checkout","Shop counter","ShopCounter",cx+width*.28,cy-4,z+.2,
                              (3.2,.88,1.04),yaw=90)
                district_mesh(unit+"_register","Cash register","ShopRegister",cx+width*.28,cy-4,z+1.24,
                              (.62,.51,.42),yaw=90,collision=False)
                remove_generated(unit+"_till")
                if title=="Supermarket":
                    for i in range(3):
                        district_mesh(f"{unit}_cooler_{i}","Grocery cooler","ShopCooler",
                                      cx-10+i*6,cy+depth*.35,z+.2,(3.5,1.15,2.1))
            if len(shops) == 1:
                box(key+"_parking", "Hardware forecourt", x, y-14, z+.2, w-2, 9, .05,
                    ground=True, material=asphalt)
            else:
                box(key+"_parking", "Customer access lane", x, y, z+.2, w-2, 8, .05,
                    ground=True, material=asphalt)
                box(key+"_entry_link", "Entrance turning apron", x+13, y-d/2+2, z+.2, 32, 5, .05,
                    ground=True, material=asphalt)
                box(key+"_drive", "Retail entrance drive", x+26, y-d/4, z+.2, 6, d/2, .05,
                    ground=True, material=asphalt)
                for side in (-1, 1):
                    cx = x+side*48
                    box(f"{key}_court_{side}", "Short-stay parking", cx, y-19, z+.2,
                        22, 32, .05, ground=True, material=asphalt)
                    for row in range(6):
                        for bank in (-1, 1):
                            box(f"{key}_stall_{side}_{row}_{bank}", "Retail parking line",
                                cx+bank*7.8, y-30+row*3, z+.26, 5, .1, .015,
                                material=style_materials["traffic"], collision=False)
                for row, dy in enumerate((-58, 18)):
                    box(f"{key}_public_walk_{row}", "Shop pedestrian walk", x, y+dy, z+.2,
                        191 if row==0 else 112, 2.4, .1, ground=True, material=concrete)
                # Short spurs meet each independently sized shop's entrance sidewalk.
                for index,(dx,front,walk) in enumerate(((-83,-51.5,-58),(0,-54.5,-58),
                        (82.5,-51.5,-58),(-39,24,18),(39,22.5,18))):
                    box(f"{key}_walk_link_{index}", "Entrance path", x+dx, y+(front+walk)/2,
                        z+.2, 2.5, front-walk, .1, ground=True, material=concrete)
            remove_generated(key+"_walk")
        elif site["kind"] == "suburbs":
            district = key+"_homes"
            distance = math.hypot(x-sx,y-sy)
            columns = max(2,min(5,round(5-distance/700)))
            for row in (-1, 1):
                for lot in range(columns):
                    hx, hy = x+((lot+.5)/columns-.5)*(w-2), y+row*27
                    home(f"{district}_{row}_{lot}",hx,hy,z+.2,0 if row>0 else 180,housing_styles[site["id"]])
            remove_generated(*(tag for tag in actor_tags if tag.startswith(district+"_") and tag not in site_keys))
            box(district+"_lane", "Residential lane", x, y, z+.2, w, 7, .06, ground=True, material=asphalt)
            migrate_landmark(key, district, (key+"_slab",))
        elif site["id"] == "city-nightlife":
            site_tools["nightlife_layout"](box, door, cache, encounter, sign, storefront_window,
                                          key, x, y, z+.2, style_materials)
            remove_generated(*(f"{key}_bay_{index}" for index in range(4)))
            remove_blockout(key)
        elif site["kind"] == "police":
            site_tools["precinct_layout"](box, door, cache, encounter, sign, key, x, y, z+.2,
                                         style_materials, name)
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
        elif site["id"] in {"southwest-apartments", "north-apartments"}:
            estate = key+"_panel_estate"
            for row in (-1,1):
                block = f"{estate}_{row}"
                by = y+row*10
                district_mesh(block+"_skin","Residential panel block","PanelHousingExterior",
                              x,by,z+.2,(38,13,11),collision=False)
                civic["hall"](block,x,by,z+.2,37,12.7,3,0,"academic","Apartments",
                              box,door,cache,encounter,sign,lambda *args: True,style_materials)
            migrate_landmark(key,estate,(key+"_slab",))
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
            shop_x, shop_w = x, w
            if site["id"] in {"car-lot", "dock-strip"}:
                shop_x, shop_w = x-w*.19, w*.58
                yard_x = x+w*.29
                if site["id"] == "car-lot":
                    for i, vehicle in enumerate(("BurnedSedan", "BurnedCompact", "AbandonedVan")):
                        wreck(f"{key}_display_car_{i}", yard_x, y+(i-1)*5, z+.2, 12, vehicle)
                else:
                    wreck(key+"_delivery", yard_x, y-4.5, z+.2, 90, "RustedTruck")
                    for i in range(3):
                        box(f"{key}_cargo_{i}", "Weathered cargo crate", yard_x+(i-1)*2.1, y+4,
                            z+.2, 1.4, 1.15, 1.1)
            interiors["place_site"](box, door, ramp, cache, encounter, sign, storefront_window,
                treatment_bed, furniture, site_tools["building_shell"], site_tools["double_door"], key,
                site, shop_x, y, z+.2, shop_w, d, style_materials, scatter_caches)
            if site["kind"] == "shops":
                facade(key+"_shopfront", "Storefront", shop_x, y, z+.2, shop_w*.92, d*.90, 3.65,
                       0, 2.5, 2.9, bundled=True)
            elif site["kind"] in {"clinic", "civic", "hospital"}:
                facade(key+"_masonry", "StoneFacade", x, y, z+.2, w*.88, d*.84, 4.25,
                       0, 3.15, 3.3, bundled=True)
            remove_generated(key+"_shell", key+"_trim", key+"_portico_roof",
                             *(f"{key}_portico_{i}" for i in range(4)))
        if site["kind"] == "services":
            garage = key+("_SixTwelve_garage" if site["id"]=="southwest-services" else "_garage")
            gx,gy = x+12,y+1
            front = site_tools["building_shell"](box,garage,gx,gy,z+.2,8.6,10,4.8,4.2,
                style_materials["StationBrick"],concrete,clear_height=3.5)
            door(garage+"_shutter","Auto shop bay door",gx,front-.06,z+.2,4.2,3.5,
                 roll_up=True,finish="dark_trim")
            sign(garage+"_sign","AUTO SERVICE",gx,front-.25,z+4.35,size=28)
            box(garage+"_link","Covered service passage",x+7,gy,z+3.2,3,4,.18,material=concrete)
            for side in (-1,1):
                box(f"{garage}_post_{side}","Vehicle lift column",gx+side*1.65,gy,z+.2,.35,.45,2.85,
                    material=style_materials["petrol"])
            box(garage+"_bench","Maintenance workbench",gx,gy+3.6,z+.2,3.8,.85,.95,
                material=style_materials["timber"])
            cache(garage+"_parts","Auto parts cabinet",gx-3,gy+3.4,z+.2,profile="BUILDING")
        if site["kind"] == "industry" and site["id"] not in {"hangars","air-reserve","water-treatment","power-southwest","power-northeast","warehouses", "east-factory", "west-factory"}:
            dress_industrial(key, x, y, z+.2, w*.90, d*.86, warehouse=site["id"] == "warehouses")
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
        if site["id"] in {"city-parking", "city-nightlife"}:
            city_cache_layouts = {
                "city-parking": (((-7.5,9.4), (2.2,9.4), (8,9.4), (12.1,8.9)), (10, 51, 9, 8, 7, 15)),
                "city-nightlife": (((-12.8,2.1), (-10.8,-1.4), (2.9,4.4), (4.7,-1.4)), (68, 5, 9, 3, 6, 9)),
            }
            positions, weights = city_cache_layouts[site["id"]]
            scatter_caches(key+"_district", x, y, z+.2, positions, weights, 95)

        if site["id"] in {"city-towers", "city-hospital", "city-hall", "university", "city-police", "airport", "hangars",
                          "east-entertainment", "west-entertainment"}:
            remove_generated(*(tag for tag in actor_tags if tag.startswith(key+"_") and tag not in site_keys))
        if site["id"] in {"winter-village", "hill-town", "north-lowlands"}:
            landmark_mesh(key+"_bus_stop", "Weathered roadside bus shelter", "WeatheredBusStop",
                          x+7.6, y-d/2-2.8, z+.05, collision=True)
        if site["id"] in {"city-hospital","city-police"}:
            for side in (-1,1):
                communications_art(f"{key}_landmark_triage_{side}","Outbreak screening barrier","CheckpointWall",
                                   x+side*8,y-d/2+1,z+.2,(8,1,1.6),health=240)
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

    resized_site = None
    for route in plan["m4_route"]:
        place_route(route)
    garrison=next(r for r in plan["m4_route"] if r["id"]=="garrison-road")
    for idx,seg in enumerate((garrison["segments"][0],garrison["segments"][-1])):
        px,py,pz=seg["start"]
        guide = f"CrossroadsGarrisonGuide_{idx}"
        box(guide+"_pole", "Garrison direction sign post", px, py+5.9, pz-.3, .16, .16, 3.15,
            material=style_materials["dark_trim"])
        box(guide+"_board", "Garrison direction sign", px, py+5.9, pz+2.05, 8.6, .12, .96,
            material=style_materials["petrol"])
        sign(guide, "SOUTH GARRISON  /  BASE ACCESS", px, py+5.8, pz+2.5, size=36)
    for site in plan["communications"]:
        resized_site = "CrossroadsComms_" + site["id"]
        if site["kind"] in {"radio","transmitter"}:
            remove_generated(resized_site + ("_Laitse_pad" if site["kind"] == "radio" else "_pad"))
        communications["stamp_compound"](site, box, door, sign, site_tools["building_shell"],
                                         storefront_window, communications_art, cache, encounter, style_materials)
        if site["kind"] == "radio":
            key = "CrossroadsComms_" + site["id"]
            migrate_landmark(key, key+"_Laitse")
    resized_site = None
    for site in plan["substations"]:
        resized_site = "CrossroadsUtility_"+site["id"]
        utility["stamp_substation"](site, box, district_mesh, district_mesh, cache, sign, concrete)
    transmission = plan["grid"]
    for key, pole in transmission["poles"].items():
        district_mesh("CrossroadsPylon_"+key, "High-voltage transmission pylon", "TransmissionPylon",
                      pole["x"],pole["y"],pole["z"],(10.432,2.622,pole["height_m"]),yaw=pole["yaw"])
    for a,b in transmission["spans"]:
        for index,(start,end) in enumerate(zip(transmission["poles"][a]["phases"],
                                             transmission["poles"][b]["phases"], strict=True)):
            cable(f"CrossroadsCable_{a}_{b}_{index}",start,end)
    mine_class=unreal.load_class(None,"/Script/Survival.SurvivalQuarantineMine")
    if not mine_class:
        raise RuntimeError("Build SurvivalEditor to load quarantine hazards.")
    perimeter_keys=set()
    resized_site="CrossroadsCordon"
    for i,(x,y,bottom,top,span,dx,nx,ny,kind) in enumerate(plan["quarantine"]["sections"]):
        tag=f"CrossroadsCordon_{i}"
        yaw=0 if dx else 90
        gate=kind=="gate"
        box(tag+"_wall","Sealed checkpoint gate" if gate else "Quarantine wall",x,y,bottom,
            span+.08,.95,top-bottom,material=style_materials["dark_trim"] if gate else concrete,
            rotation=(0,yaw))
        perimeter_keys.add(tag+"_wall")
        if gate:
            district_mesh(tag+"_tower","Abandoned checkpoint tower","GuardTower",x+nx*5,y+12,
                          top-4.2,(5.5,5.5,10))
            district_mesh(tag+"_ambulance","Military evacuation vehicle","MilitaryAmbulance",
                          x+nx*12,y-7,top-4.2,(5.4,2.2,2.7),yaw=8)
            sign(tag+"_order","QUARANTINE  /  GATE CLOSED",x+nx*.58,y,top-.8,
                 yaw=0 if nx>0 else 180,size=30)
            perimeter_keys.update(tag+s for s in ("_tower","_ambulance","_order"))
        else:
            for suffix,offset,base in (("_crest",0,top-.08),("_outer",2.4,bottom+1.2)):
                def wire(actor):
                    component=fitted_mesh(actor,district_meshes["QuarantineWire"],(span,.80,.85),
                                          yaw,align_long_axis=True)
                    component.set_material(0,style_materials["dark_trim"])
                    component.set_collision_profile_name("NoCollision")
                spawn(unreal.StaticMeshActor,tag+suffix,"Quarantine wire",x+nx*offset,y+ny*offset,
                      base+.425,wire,relocate=True,reconfigure=True)
                perimeter_keys.add(tag+suffix)
    for i,(x,y,z) in enumerate(plan["quarantine"]["hazards"]):
        tag=f"CrossroadsCordon_hazard_{i}"
        def marker(actor):
            mesh=actor.get_component_by_class(unreal.StaticMeshComponent)
            asset=detail_meshes["QuarantineMine"]
            mesh.set_static_mesh(asset)
            ext=asset.get_bounding_box().max-asset.get_bounding_box().min
            mesh.set_relative_scale3d(unreal.Vector(72/ext.x,72/ext.y,16/ext.z))
            mesh.set_relative_location(unreal.Vector(0,0,4))
            for slot in range(mesh.get_num_materials()):
                mesh.set_material(slot,asset.get_material(slot))
        spawn(mine_class,tag,"Quarantine hazard",x,y,z+.1,marker,relocate=True,reconfigure=True)
        perimeter_keys.add(tag)
        if i%8==0 or i>=len(plan["quarantine"]["hazards"])-len(plan["quarantine"]["road_hazards"]):
            sign(tag+"_warning","DANGER  /  QUARANTINE MINES",x,y+1,z+1.35,size=23)
            perimeter_keys.add(tag+"_warning")
    remove_generated(*(t for t in actor_tags if t.startswith("CrossroadsCordon_") and t not in perimeter_keys))
    resized_site=None
    downtown["stamp_city"](plan["city_lots"],plan["city_barriers"],box,facade,
                           communications_art,door,cache,furnishing,tower,church,roof_ladder,style_materials,home)
    remove_generated(*(previous_infill - touched_infill))
    for car in plan["wrecks"]:
        wreck("CrossroadsWreck_"+car["id"],car["x"],car["y"],car["z"],car["yaw"],car["model"])
    place_rail(plan["rail"])
    remove_generated(*(previous_roads - touched_roads))
    for checkpoint in plan["checkpoints"]:
        communications["stamp_checkpoint"](checkpoint, communications_art)
        x, y, z = checkpoint["xyz_m"]
        dx, dy = checkpoint["end"][0] - x, checkpoint["end"][1] - y
        length = math.hypot(dx, dy) or 1
        offset = checkpoint["width_m"]/2 + 2.0
        cache(f'CrossroadsCheckpoint_{checkpoint["id"]}_supplies', "Checkpoint supply crate",
              x-dy/length*offset, y+dx/length*offset, z, profile="SECURITY", chance=65)

    spawn(unreal.TargetPoint, "CrossroadsRegionReady", "Crossroads region bake complete",
          sx, sy, 0, lambda actor: None, always_loaded=True)
    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}.")
    unreal.log(f"Stamped {LEVEL}. Saving external actors.")


if __name__ == "__main__":
    place()
