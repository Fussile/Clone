"""Run in the Unreal Editor to generate an editable solo test level."""
import unreal

LEVEL = "/Game/Maps/L_SouthernCrossroads"
CUBE = "/Engine/BasicShapes/Cube.Cube"


def build():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if assets.does_asset_exist(LEVEL):
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor.get_editor_world()
        if (not world or world.get_path_name().split(".")[0] != LEVEL) and not levels.load_level(LEVEL):
            raise RuntimeError(f"Could not open {LEVEL}.")
        # An interrupted first build can leave the empty package created by new_level().
        if any(isinstance(actor, (unreal.PlayerStart, unreal.StaticMeshActor))
               for actor in actors.get_all_level_actors()):
            return
    elif not levels.new_level(LEVEL):
        raise RuntimeError(f"Could not create {LEVEL}.")
    cube = unreal.load_asset(CUBE)
    if cube is None:
        raise RuntimeError("Could not load the engine blockout cube.")
    def spawn(cls, label, x, y, z, rotation=None):
        location = unreal.Vector(x * 100, y * 100, z * 100)
        actor = actors.spawn_actor_from_class(cls, location, rotation or unreal.Rotator())
        if actor is None:
            raise RuntimeError(f"Could not place {label}.")
        actor.set_actor_label(label)
        return actor

    def box(label, x, y, bottom, width, depth, height, surface="Concrete034"):
        actor = spawn(unreal.StaticMeshActor, label, x, y, bottom + height / 2)
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        comp.set_static_mesh(cube)
        comp.set_collision_profile_name("BlockAll")
        actor.set_editor_property("tags", [unreal.Name(f"CrossroadsSurface_{surface}")])
        actor.set_actor_scale3d(unreal.Vector(width, depth, height))

    def shell(name, x, y, width, depth, height, door):
        # Front faces south and is split around a full-height opening.
        box(name + " slab", x, y, 0, width, depth, 0.12)
        box(name + " roof", x, y, height, width, depth, 0.24)
        box(name + " back", x, y + depth / 2 - 0.12, 0.12, width, 0.24, height - 0.12)
        for side, label in ((-1, " west"), (1, " east")):
            box(name + label, x + side * (width / 2 - 0.12), y,
                0.12, 0.24, depth, height - 0.12)
            box(name + " front" + label, x + side * (width + door) / 4,
                y - depth / 2 + 0.12, 0.12, (width - door) / 2, 0.24, height - 0.12)

    # Local (0, 0) is (2650, 350) meters in the standard world plan.
    for ix in range(-2, 2):
        for iy in range(-1, 3):
            box(f"Ground {ix},{iy}", (ix + 0.5) * 250, (iy + 0.5) * 250,
                -0.5, 250, 250, 0.5, "Ground036")

    box("South crossroads east-west", 0, 0, 0, 1000, 12, 0.06, "Asphalt021")
    box("Country road north", 0, 378, 0, 12, 744, 0.06, "Asphalt021")
    box("Country road south", 0, -128, 0, 12, 244, 0.06, "Asphalt021")
    box("Suburban access road", -119, 235, 0, 226, 8, 0.06, "Asphalt021")
    box("Service station forecourt", 36, 23, 0, 48, 34, 0.08, "Asphalt021")
    shell("Gas station shop", 37, 49, 18, 12, 4, 2.4)
    box("Canopy", 35, 22, 4, 18, 8, 0.4)
    for x in (28, 42):
        box("Canopy post", x, 22, 0.08, 0.5, 0.5, 3.92)
        box("Pump island", x, 24, 0.08, 1.2, 0.6, 1.2)

    for index, x in enumerate((-46, -81, -116, -151, -186), 1):
        shell(f"Suburban house {index}", x, 252, 13, 11, 3.1, 1.6)
        box(f"House {index} driveway", x, 242, 0, 3.5, 6, 0.06)
    box("Farm access lane", 56, 139, 0, 100, 6, 0.06, "Asphalt021")
    shell("Farm barn", 118, 155, 22, 16, 5, 4)
    for index in range(10):
        box(f"Farm row {index + 1}", 170 + index * 3, 180, 0, 1.2, 55, 0.1, "Ground036")

    # The plan places the warehouse 500 m west and 100 m south.
    # Inset its footprint from the west edge so players can walk around it.
    shell("Warehouse", -466, -100, 44, 30, 8, 6)
    box("Warehouse access", -466, -40, 0, 8, 68, 0.06, "Asphalt021")
    box("Warehouse side lane", -438, -102, 0, 6, 56, 0.06, "Asphalt021")
    box("Warehouse loading apron", -461, -122, 0, 52, 14, 0.06, "Asphalt021")
    spawn(unreal.PlayerStart, "Start by gas station", 34, 12, 1.15, unreal.Rotator(yaw=90))
    sun = spawn(unreal.DirectionalLight, "Sun", 0, 0, 20, unreal.Rotator(pitch=-45, yaw=-35))
    sun_light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_light.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_light.set_editor_property("atmosphere_sun_light", True)
    sky = spawn(unreal.SkyLight, "Sky light", 0, 0, 20)
    sky_light = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_light.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_light.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, "Sky atmosphere", 0, 0, 0)

    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}. Save it from the editor before closing.")
    unreal.log(f"Saved {LEVEL}. Press Play to check the solo route.")


build()
