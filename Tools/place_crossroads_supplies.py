"""Place the loot crates and update the generated station lighting."""
import unreal


def place():
    level = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not level or level.get_path_name().split(".")[0] != "/Game/Maps/L_SouthernCrossroads":
        raise RuntimeError("Open L_SouthernCrossroads before preparing the defense playtest.")

    crate_class = unreal.load_class(None, "/Script/Survival.SurvivalSupplyCrate")
    if not crate_class:
        raise RuntimeError("Build SurvivalEditor and restart the editor first.")

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = actors.get_all_level_actors()
    crates = (
        ("CrossroadsStationSupplies", "Gas station supply crate", (3700, 5150, 52), (1.2, 0.8, 0.8), 24, 12, False),
        ("CrossroadsStationSidearm", "Gas station sidearm case", (4100, 5050, 39), (0.95, 0.65, 0.54), 0, 0, True),
        ("CrossroadsStationTimber", "Timber cache by west pump", (2850, 2680, 41), (0.95, 0.7, 0.66), 12, 0, False),
        ("CrossroadsStationStone", "Stone cache by east wall", (4750, 4660, 34), (0.7, 0.7, 0.68), 0, 8, False),
    )
    for tag, label, position, scale, timber, stone, sidearm in crates:
        name = unreal.Name(tag)
        if any(actor.actor_has_tag(name) for actor in existing):
            continue
        crate = actors.spawn_actor_from_class(crate_class, unreal.Vector(*position))
        if not crate:
            raise RuntimeError(f"Could not place {label}.")
        crate.set_actor_label(label)
        crate.set_actor_scale3d(unreal.Vector(*scale))
        crate.set_editor_property("tags", [name])
        crate.set_editor_property("timber_amount", timber)
        crate.set_editor_property("stone_amount", stone)
        crate.set_editor_property("contains_sidearm", sidearm)

    survival_crates = (
        ("CrossroadsStationProvisions", "Gas station provisions", (3400, 5230, 52), 2, 3, 4),
        ("CrossroadsHouseProvisions", "House provisions", (-4600, 25450, 52), 2, 2, 3),
    )
    for tag, label, position, food, water, cloth in survival_crates:
        name = unreal.Name(tag)
        if any(actor.actor_has_tag(name) for actor in existing):
            continue
        crate = actors.spawn_actor_from_class(crate_class, unreal.Vector(*position))
        if not crate:
            raise RuntimeError(f"Could not place {label}.")
        crate.set_actor_label(label)
        crate.set_actor_scale3d(unreal.Vector(0.65, 0.6, 0.8))
        for prop, value in (("tags", [name]), ("timber_amount", 0), ("stone_amount", 0),
                            ("food_amount", food), ("water_amount", water), ("cloth_amount", cloth)):
            crate.set_editor_property(prop, value)

    rifle_tag = unreal.Name("CrossroadsRifleLocker")
    if not any(actor.actor_has_tag(rifle_tag) for actor in existing):
        locker = actors.spawn_actor_from_class(crate_class, unreal.Vector(3950, 5310, 36))
        if not locker:
            raise RuntimeError("Could not place the rifle locker.")
        locker.set_actor_label("Rifle and field guide")
        locker.set_actor_scale3d(unreal.Vector(1.1, 0.6, 0.55))
        for prop, value in (("tags", [rifle_tag]), ("timber_amount", 0), ("stone_amount", 0),
                            ("contains_rifle", True), ("builder_guide_amount", 1)):
            locker.set_editor_property(prop, value)

    zombie_tag = unreal.Name("CrossroadsRoamingZombie")
    if not any(actor.actor_has_tag(zombie_tag) for actor in existing):
        zombie_type = unreal.load_class(None, "/Script/Survival.SurvivalZombie")
        zombie = actors.spawn_actor_from_class(zombie_type, unreal.Vector(6900, 4500, 112)) if zombie_type else None
        if not zombie:
            raise RuntimeError("Could not place the Crossroads zombie.")
        zombie.set_actor_label("Crossroads zombie")
        zombie.set_editor_property("tags", [zombie_tag])
        zombie.set_editor_property("aggro_range", 1800.0)

    moon_tag = unreal.Name("CrossroadsMoon")
    if not any(actor.actor_has_tag(moon_tag) for actor in existing):
        moon = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000))
        if not moon:
            raise RuntimeError("Could not place the Crossroads moonlight.")
        moon.set_actor_label("Moonlight")
        moon.set_editor_property("tags", [moon_tag])
        light = moon.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property("intensity", 0.25)
        light.set_editor_property("atmosphere_sun_light", True)
        light.set_editor_property("atmosphere_sun_light_index", 1)

    for actor in existing:
        for label, component_type in (
            ("Sun", unreal.DirectionalLightComponent), ("Sky light", unreal.SkyLightComponent)
        ):
            if actor.get_actor_label() == label:
                light = actor.get_component_by_class(component_type)
                if light:
                    light.set_mobility(unreal.ComponentMobility.MOVABLE)
                    tag = unreal.Name("CrossroadsSun")
                    if isinstance(light, unreal.DirectionalLightComponent) and not actor.actor_has_tag(tag):
                        actor.set_editor_property("tags", [*actor.get_editor_property("tags"), tag])


place()
