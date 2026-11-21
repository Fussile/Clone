"""Mark ground-bearing surfaces and seed a destructible authored canopy."""
import unreal


def prepare_structures():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = actors.get_all_level_actors()
    ground = unreal.Name("StructuralGround")
    for actor in existing:
        if not isinstance(actor, unreal.StaticMeshActor) or actor.actor_has_tag(ground):
            continue
        label = actor.get_actor_label().lower()
        if (label.startswith(("ground ", "farm row ", "south crossroads "))
                or label.endswith((" road", " slab", " driveway", " apron", " forecourt", " access", " lane"))
                or label.startswith(("country road ", "farm access lane"))):
            tags = list(actor.get_editor_property("tags"))
            actor.set_editor_property("tags", [*tags, ground])

    tag = unreal.Name("CrossroadsUtilityCanopy")
    canopy = next((actor for actor in existing if actor.actor_has_tag(tag)), None)
    if canopy is None:
        prefab_class = unreal.load_class(None, "/Script/Survival.SurvivalStructurePrefab")
        if not prefab_class:
            raise RuntimeError("Build SurvivalEditor before placing the utility canopy.")
        canopy = actors.spawn_actor_from_class(prefab_class, unreal.Vector(6300, 3600, 0))
        if not canopy:
            raise RuntimeError("Could not place the utility canopy.")
    else:
        current = [(part.get_editor_property("piece_index"), part.get_editor_property("offset"))
                   for part in canopy.get_editor_property("parts")]
        if not any(
                [(index, offset.x, offset.y, offset.z) for index, offset in current] ==
                [(8, 0, 0, 0), (8, 3 * width, 0, 0), (2, 0, 0, height), (2, width, 0, height),
                 (2, 3 * width, 0, height), (2, 2 * width, 0, height)]
                for width, height in ((100, 200), (100, 220), (220, 220))):
            return

    parts = []
    # Keep the end pillars and four panels across the 16 m chunk boundary.
    for piece, x, z in ((8, 0, 0), (8, 315, 0), (2, 0, 210), (2, 105, 210), (2, 315, 210), (2, 210, 210)):
        part = unreal.SurvivalPrefabPart()
        part.set_editor_property("piece_index", piece)
        part.set_editor_property("offset", unreal.Vector(x, 0, z))
        parts.append(part)
    canopy.set_editor_property("parts", parts)
    if not canopy.actor_has_tag(tag):
        canopy.set_editor_property("tags", [tag])
    canopy.set_actor_label("Destructible utility canopy")


prepare_structures()
