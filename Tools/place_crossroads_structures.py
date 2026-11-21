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
    if any(actor.actor_has_tag(tag) for actor in existing):
        return
    prefab_class = unreal.load_class(None, "/Script/Survival.SurvivalStructurePrefab")
    if not prefab_class:
        raise RuntimeError("Build SurvivalEditor before placing the utility canopy.")
    canopy = actors.spawn_actor_from_class(prefab_class, unreal.Vector(6300, 3600, 0))
    if not canopy:
        raise RuntimeError("Could not place the utility canopy.")

    parts = []
    # The floor tiles bridge the 16 m chunk line at local X = 64 m.
    for piece, x, z in ((8, 0, 0), (8, 300, 0), (2, 0, 220), (2, 100, 220), (2, 300, 220), (2, 200, 220)):
        part = unreal.SurvivalPrefabPart()
        part.set_editor_property("piece_index", piece)
        part.set_editor_property("offset", unreal.Vector(x, 0, z))
        parts.append(part)
    canopy.set_editor_property("parts", parts)
    canopy.set_editor_property("tags", [tag])
    canopy.set_actor_label("Destructible utility canopy")


prepare_structures()
