"""Reusable layouts for the first Crossroads scavenging route."""
import random

import unreal


def station_layout(box, key, x, y, floor):
    box(key+"_rear", "Station rear wall", x, y+4, floor, 12, .3, 3.15)
    for sign, side in ((-1, "west"), (1, "east")):
        box(f"{key}_{side}", f"Station {side} wall", x+sign*5.85, y, floor, .3, 8, 3.15)
        box(f"{key}_{side}_front", f"Station {side} storefront", x+sign*3.75, y-4,
            floor, 4.5, .3, 3.15)
        box(f"{key}_{side}_leg", "Canopy column", x+sign*4.5, y-7, floor, .3, .3, 3.15)
    box(key+"_roof", "Station roof", x, y, floor+3.15, 12, 8.3, .3)
    box(key+"_canopy", "Station canopy", x, y-6, floor+3.15, 12, 4, .3)
    box(key+"_counter", "Station counter", x-2.8, y+2.8, floor, 3.8, .75, 1.05)


def warehouse_parts():
    # Coordinates are build cells, ordered from the slab up. Place the infill one
    # row behind the roof so it can be removed without taking a roof support.
    columns = [(8, x, 0, 0) for x in (-3, 3)]
    infill = [(6, 0, -2, layer) for layer in (0, 1)]
    roof = [(2, x, y, 2) for x in (-3, 3, -2, 2, -1, 1, 0) for y in (0, -1)]
    return columns + infill + roof


def warehouse_structure(actor, key):
    cell = 105
    origin = actor.get_actor_location()
    actor.set_actor_location(unreal.Vector(round(origin.x/cell)*cell, round(origin.y/cell)*cell,
                                           origin.z), False, False)
    parts = []
    for piece, x, y, z in warehouse_parts():
        part = unreal.SurvivalPrefabPart()
        part.set_editor_property("piece_index", piece)
        part.set_editor_property("offset", unreal.Vector(x*cell, y*cell, z*cell))
        parts.append(part)
    actor.set_editor_property("prefab_id", unreal.Name(key))
    actor.set_editor_property("parts", parts)


def apartment_layout(box, cache, encounter, key, x, y, floor, seed):
    # A protected shared stair outside the east wall leaves the same entry on both levels.
    # Doors to the left and right face the central corridor.
    height = 3.15
    for level in range(2):
        base = floor + level*height
        story = f"{key}_floor_{level}"
        rng = random.Random(f"{seed}/{key}/{level}")

        def panel(name, dx, dy, w, d):
            box(f"{story}_{name}", f"Apartment {name}", x+dx, y+dy, base, w, d, height-.2)

        panel("west wall", -5.9, 0, .2, 9)
        panel("rear wall", 0, 4.4, 12, .2)
        for dx, side in ((-3.5, "west"), (3.5, "east")):
            panel(f"{side} front wall", dx, -4.4, 5, .2)
        # Continuous east wall with a gap at the southern end for the stair landing.
        panel("east wall", 5.9, .65, .2, 7.7)
        # Two layouts share the landing and partitions but reverse their room entrances.
        for dx, side in ((-1, "west"), (1, "east")):
            door_y = 1.25 if level else -1.25
            for index, (a, b) in enumerate(((-2.8, door_y-.65), (door_y+.65, 4.3))):
                panel(f"{side} room wall {index}", dx, (a+b)/2, .2, b-a)
        box(story+"_clutter", "Apartment fallen cabinet", x-4, y+rng.uniform(-2, 3),
            base, 1.2, .5, .45)
        cache(story+"_cache", "Apartment supply crate", x+rng.uniform(2.4, 4.7),
              y+rng.uniform(-1.6, 2.8), base, food_amount=1+level, cloth_amount=2-level)
        encounter(story+"_walker", "Apartment walker", x-3, y+rng.uniform(-1, 2), base)

    # Close the upper front corridor while leaving the ground entrance open.
    box(key+"_upper_front", "Apartment upper front wall", x, y-4.4, floor+height, 2, .2, height-.2)
    box(key+"_upper_floor", "Apartment upper floor", x, y, floor+height-.2, 12, 9, .2)
    box(key+"_roof", "Apartment roof", x, y, floor+height*2-.2, 12, 9, .25)
    box(key+"_stair_landing", "Apartment stair landing", x+6.85, y-3.75,
        floor+height-.2, 1.7, 1.5, .2)
    for step in range(18):
        box(f"{key}_step_{step}", "Apartment stair tread", x+6.85, y+2.15-step*.3,
            floor, 1.7, .3, (step+1)*height/18)
