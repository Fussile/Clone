"""Place the prototype supply crate in an existing Southern Crossroads map."""
import unreal


def place():
    level = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not level or level.get_path_name().split(".")[0] != "/Game/Maps/L_SouthernCrossroads":
        raise RuntimeError("Open L_SouthernCrossroads before placing the supply crate.")

    crate_class = unreal.load_class(None, "/Script/Survival.SurvivalSupplyCrate")
    if not crate_class:
        raise RuntimeError("Build SurvivalEditor and restart the editor first.")

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    tag = unreal.Name("CrossroadsStationSupplies")
    if any(actor.actor_has_tag(tag) for actor in actors.get_all_level_actors()):
        unreal.log("The gas station supply crate is already in this level.")
        return

    # The gas station's slab is 12 cm high. Set the bottom of the crate on it.
    crate = actors.spawn_actor_from_class(crate_class, unreal.Vector(3700, 5150, 52))
    if not crate:
        raise RuntimeError("Could not place the gas station supply crate.")
    crate.set_actor_label("Gas station supply crate")
    crate.set_actor_scale3d(unreal.Vector(1.2, 0.8, 0.8))
    crate.set_editor_property("tags", [tag])
    unreal.log("Placed the supply crate. Save the map when ready.")


place()
