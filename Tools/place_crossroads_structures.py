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
        old_layouts = [
            [(8, 0), (8, 300), (2, 0), (2, 100), (2, 300), (2, 200)],
            [(8, 0), (8, 660), (2, 0), (2, 220), (2, 660), (2, 440)],
        ]
        current = [(part.get_editor_property("piece_index"), part.get_editor_property("offset").x)
                   for part in canopy.get_editor_property("parts")]
        if current not in old_layouts or any(
                part.get_editor_property("offset").y != 0 or
                part.get_editor_property("offset").z != (0 if part.get_editor_property("piece_index") == 8 else 220)
                for part in canopy.get_editor_property("parts")):
            return

    parts = []
    # Keep the end pillars and four panels across the 16 m chunk boundary.
    for piece, x, z in ((8, 0, 0), (8, 300, 0), (2, 0, 200), (2, 100, 200), (2, 300, 200), (2, 200, 200)):
        part = unreal.SurvivalPrefabPart()
        part.set_editor_property("piece_index", piece)
        part.set_editor_property("offset", unreal.Vector(x, 0, z))
        parts.append(part)
    canopy.set_editor_property("parts", parts)
    if not canopy.actor_has_tag(tag):
        canopy.set_editor_property("tags", [tag])
    canopy.set_actor_label("Destructible utility canopy")


prepare_structures()
