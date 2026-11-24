"""Place two roadside scavenging stops on new and previously prepared Crossroads maps."""
import unreal


def place():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    present = {str(tag) for actor in actors.get_all_level_actors()
               for tag in actor.get_editor_property("tags")}
    meshes = {name: unreal.load_asset(f"/Engine/BasicShapes/{name}.{name}")
              for name in ("Cube", "Cylinder")}
    crate_type = unreal.load_class(None, "/Script/Survival.SurvivalSupplyCrate")
    if not crate_type or not all(meshes.values()):
        raise RuntimeError("Missing Crossroads POI geometry or supply crate class.")

    def spawn(kind, label, x, y, z, scale, configure, tags=()):
        key = "CrossroadsPOI_" + label.replace(" ", "_")
        if key in present:
            return
        actor = actors.spawn_actor_from_class(kind, unreal.Vector(x * 100, y * 100, z * 100))
        if not actor:
            raise RuntimeError(f"Could not place {label}.")
        try:
            configure(actor)
            actor.set_actor_scale3d(unreal.Vector(*scale))
            actor.set_actor_label(label)
            actor.set_editor_property("tags", [unreal.Name(name) for name in (key, *tags)])
        except Exception:
            actors.destroy_actor(actor)
            raise
        present.add(key)

    def solid(label, x, y, bottom, w, d, h, shape="Cube", surface="Concrete034", ground=False):
        def configure(actor):
            mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh.set_static_mesh(meshes[shape])
            mesh.set_collision_profile_name("BlockAll")
        tags = [f"CrossroadsSurface_{surface}"]
        if ground:
            tags.append("StructuralGround")
        spawn(unreal.StaticMeshActor, label, x, y, bottom + h / 2, (w, d, h), configure, tags)

    def cache(label, x, y, **items):
        def configure(actor):
            for name, amount in {"timber_amount": 0, "stone_amount": 0, **items}.items():
                actor.set_editor_property(name, amount)
        spawn(crate_type, label, x, y, .48, (.8, .65, .7), configure)

    # A raised tank marks the eastbound turn from the station. Walk beneath it to the pump bay.
    solid("Pump station access", 152, 13, 0, 5, 14, .08, surface="Asphalt021", ground=True)
    solid("Pump station slab", 152, 29, 0, 19, 18, .13, ground=True)
    for x in (148, 156):
        for y in (27, 35):
            solid(f"Tank pier {x} {y}", x, y, .13, .8, .8, 6)
    solid("Tank platform", 152, 31, 6.13, 10, 10, .4)
    solid("Water storage tank", 152, 31, 6.53, 10, 10, 5, "Cylinder")
    solid("Tank outlet riser", 155, 33, .13, .48, .48, 6, "Cylinder")
    solid("Pump housing", 148, 23, .13, 2.5, 1.1, 1.05)
    cache("Pump station water chest", 150, 23, water_amount=5, stone_amount=4)

    # An open ambulance bay and two treatment cots give the western stop its own floor plan.
    solid("Aid post access", -120, -14, 0, 5, 16, .08, surface="Asphalt021", ground=True)
    solid("Aid post slab", -120, -29, 0, 17, 14, .13, ground=True)
    solid("Aid post rear wall", -120, -35.85, .13, 17, .3, 3)
    solid("Aid post west wall", -128.35, -29, .13, .3, 13.4, 3)
    solid("Aid post screen", -120, -31, .13, .2, 5.5, 2)
    for x in (-128, -112):
        solid(f"Ambulance bay pillar {x}", x, -22.5, .13, .4, .4, 3)
    solid("Aid post canopy", -120, -29, 3.13, 18, 15, .3)
    for y in (-27, -32):
        solid(f"Treatment cot {y}", -125, y, .13, 2.2, .85, .58)
    cache("Aid post dressing chest", -123, -33, cloth_amount=5, dressing_schematic_amount=1)


place()
