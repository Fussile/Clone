"""Reusable layouts for the first Crossroads scavenging route."""
import random

import unreal


def station_layout(box, door, key, x, y, floor, palette):
    wall, roof, trim, counter = (palette[name] for name in ("StationBrick", "asphalt", "petrol", "timber"))
    box(key+"_rear", "Station rear wall", x, y+4, floor, 12, .3, 3.15, material=wall)
    for sign, side in ((-1, "west"), (1, "east")):
        box(f"{key}_{side}", f"Station {side} wall", x+sign*5.85, y, floor, .3, 8, 3.15, material=wall)
        bay = f"{key}_{side}_display"
        cx = x+sign*3.75
        box(bay+"_sill", "Storefront sill", cx, y-4, floor, 4.5, .3, .94, material=wall)
        box(bay+"_head", "Storefront lintel", cx, y-4, floor+2.38, 4.5, .3, .77, material=wall)
        for i, offset in enumerate((-2.04, 0, 2.04)):
            width = .42 if offset else .08
            box(f"{bay}_post_{i}", "Storefront window post", cx+offset, y-4,
                floor+.94, width, .16, 1.44, material=trim)
        box(f"{key}_{side}_leg", "Canopy column", x+sign*4.5, y-7, floor, .3, .3, 3.15, material=wall)
    box(key+"_roof", "Station roof", x, y, floor+3.15, 12, 8.3, .3, material=roof)
    box(key+"_canopy", "Station canopy", x, y-6, floor+3.15, 12, 4, .3, material=roof)
    box(key+"_counter", "Station counter", x-2.8, y+2.8, floor, 3.8, .75, 1.05, material=counter)
    double_door(door, key+"_entry", "Shop doors", x, y-4.16, floor, 3, 2.35, "petrol")
    box(key+"_entry_header", "Storefront header", x, y-4, floor+2.35, 3, .3, .8, material=trim)
    box(key+"_fascia", "Station fascia", x, y-8, floor+2.9, 12, .24, .58, material=trim)


def warehouse_yard(box, key, x, y, floor, palette, depth_scale):
    steel, wood, paint = (palette[name] for name in ("dark_trim", "timber", "traffic"))
    approach = 3.16 * (depth_scale-1)
    for side in (-1, 1):
        for index in range(4):
            px, py = x + side * (6.5 + index * 2.8), y - 5.7 - approach
            box(f"{key}_bollard_{side}_{index}", "Loading yard bollard",
                px, py, floor, .22, .22, 1, material=paint)
        for index in range(3):
            px, py = x + side * 8.2, y - 8.2 - approach - index * 1.5
            box(f"{key}_pallet_{side}_{index}", "Shipping pallet",
                px, py, floor, 1.25, 1.05, .16, material=wood)
            if index < 2:
                box(f"{key}_cargo_{side}_{index}", "Weathered cargo crate",
                    px, py, floor+.16, .95, .85, .58 + .36 * index, material=wood)
        box(f"{key}_lane_{side}", "Loading lane stripe", x+side*2.2, y-10.5-approach,
            floor+.015, .12, 10.5, .02, material=paint, collision=False)
        box(f"{key}_rack_{side}", "Warehouse shelving", x+side*9.7, y+2.25*depth_scale,
            floor, .65, 1.28, 1.8, material=steel)


def service_forecourt(box, key, x, y, floor, palette):
    for dx in (-3.1, 3.1):
        bay = f"{key}_pump_{dx}"
        box(bay+"_island", "Fuel island", x+dx, y-6.5, floor, 1.4, 2.7, .16, material=palette["concrete"])
        box(bay+"_body", "Fuel dispenser", x+dx, y-6.5, floor+.16, .68, .6, 1.4, material=palette["petrol"])
        for dy in (-1.05, 1.05):
            box(f"{bay}_guard_{dy}", "Pump guard", x+dx, y-6.5+dy, floor+.16,
                .14, .14, .85, material=palette["traffic"])


def double_door(door, key, label, x, y, floor, width, height, finish):
    for side in (-1, 1):
        door(f"{key}_{side}", label, x+side*width/4, y, floor, width/2, height,
             reverse=side>0, finish=finish)


def building_shell(box, key, x, y, floor, width, depth, height, doorway, wall, roof, *, clear_height=2.35):
    t = .24
    front = y-depth/2
    for sign in (-1, 1):
        box(f"{key}_side_{sign}", "Exterior wall", x+sign*(width-t)/2, y, floor,
            t, depth, height, material=wall)
        box(f"{key}_front_{sign}", "Entrance jamb", x+sign*(width+doorway)/4,
            front+t/2, floor, (width-doorway)/2, t, height, material=wall)
    box(key+"_back", "Rear wall", x, y+(depth-t)/2, floor, width-2*t, t, height, material=wall)
    box(key+"_lintel", "Entrance lintel", x, front+t/2, floor+clear_height, doorway, t,
        height-clear_height, material=wall)
    box(key+"_roof", "Roof", x, y, floor+height, width+.45, depth+.45, .25, material=roof)
    return front


def nightlife_layout(box, door, cache, encounter, sign, facade, key, x, y, floor, palette):
    trim, brick, wood = (palette[name] for name in ("dark_trim", "brick_hardwood", "timber"))
    for side, name, shade in ((-1, "Freight Bar", brick), (1, "Crossroads Diner", palette["petrol"])):
        building = f"{key}_venue_{side}"
        cx, cy = x+side*7.8, y+1.7
        front = building_shell(box, building, cx, cy, floor, 13.6, 12, 3.5, 2.2, shade, trim)
        double_door(door, building+"_entry", "Venue entrance", cx, front-.04, floor, 2.2, 2.35,
                    "timber" if side<0 else "petrol")
        box(building+"_fascia", "Storefront fascia", cx, front-.13, floor+2.65,
            12.4, .22, .58, material=trim)
        sign(building+"_sign", name, cx, front-.26, floor+3.04)
        box(building+"_canopy", "Entrance canopy", cx, front-.95, floor+2.35,
            9.8, 1.9, .15, material=trim)
        facade(building+"_window", cx-3.55, front, floor)
        facade(building+"_window_2", cx+3.55, front, floor)
        box(building+"_counter", "Service counter", cx-1.3, cy+2.4, floor,
            5.9, .75, 1.04, material=wood)
        for i in range(3):
            tx, ty = cx+4.05, cy-2.5+i*2.3
            box(f"{building}_table_{i}", "Dining table", tx, ty, floor+.74,
                1.2, .75, .09, material=wood)
            box(f"{building}_table_base_{i}", "Table pedestal", tx, ty, floor,
                .2, .2, .76, material=trim)
            for direction in (-1, 1):
                box(f"{building}_seat_{i}_{direction}", "Booth seat", tx, ty+direction*.64,
                    floor, 1.25, .5, .45, material=shade)
        cache(building+"_stock", f"{name} stock", cx-4.8, cy+3.7, floor,
              food_amount=2 if side>0 else 1, water_amount=3 if side>0 else 2,
              scavenger_notes_amount=int(side<0))
        encounter(building+"_walker", f"{name} walker", cx+1.6, cy+1.5, floor)
    box(key+"_walk", "Nightlife sidewalk", x, y-5.9, floor, 33, 2.7, .12, material=palette["concrete"])


def precinct_layout(box, door, cache, encounter, sign, key, x, y, floor, palette, name):
    stone, trim, wood = (palette[name] for name in ("classical_civic", "dark_trim", "timber"))
    front = building_shell(box, key+"_precinct", x, y, floor, 24, 17, 4.2, 3.2, stone, trim)
    double_door(door, key+"_entry", "Precinct entrance", x, front-.04, floor, 3.2, 2.35, "petrol")
    double_door(door, key+"_records_entry", "Records room doors", x, y+2.5, floor, 2.8, 2.3, "petrol")
    box(key+"_badge_band", "Precinct fascia", x, front-.15, floor+2.82, 17, .24, .7, material=trim)
    sign(key+"_sign", name.upper(), x, front-.3, floor+3.26)
    box(key+"_awning", "Precinct portico", x, front-1.3, floor+2.7, 10, 2.6, .25, material=stone)
    for side in (-1, 1):
        box(f"{key}_column_{side}", "Portico column", x+side*4.3, front-2.2,
            floor, .52, .52, 2.7, material=stone)
        box(f"{key}_office_{side}", "Office divider", x+side*4.2, y-2.3,
            floor, .2, 6, 3, material=stone)
        box(f"{key}_rear_partition_{side}", "Records room partition", x+side*6.7, y+2.5,
            floor, 10.6, .2, 3, material=stone)
    box(key+"_desk", "Reception desk", x-1.6, y-2.2, floor, 4, .82, 1.05, material=wood)
    for i in range(5):
        box(f"{key}_archive_{i}", "Archive shelving", x-8+i*3.6, y+7.65,
            floor, 2.35, .65, 1.82, material=trim)
    cache(key+"_medical", "Precinct first-aid cabinet", x-8.5, y-4.5, floor, cloth_amount=5)
    cache(key+"_records", "Precinct emergency supplies", x+5.6, y+5.7, floor,
          water_amount=2, dressing_schematic_amount=1)
    encounter(key+"_watch", "Precinct walker", x+1.3, y-2.5, floor)
    encounter(key+"_records_walker", "Records room runner", x+7.0, y+5.8, floor, runner=True)


def garage_layout(box, door, ramp, cache, encounter, sign, key, x, y, floor, palette):
    slab, trim, paint = (palette[name] for name in ("concrete", "dark_trim", "traffic"))
    upper = floor+3.15
    # Clear an opening for the ramp along the western edge of the deck.
    box(key+"_deck_main", "Parking deck", x+1.9, y, upper-.28, 24.2, 22, .28, material=slab)
    box(key+"_deck_landing", "Ramp landing", x-12.1, y+7.25, upper-.28, 3.8, 7.5, .28, material=slab)
    ramp(key+"_ramp", "Parking ramp", x-12.1, y-13.8, y+3.5, floor, upper, 3.8, slab)
    box(key+"_front_rail", "Upper deck parapet", x+1.9, y-10.9, upper, 24.2, .22, 1.06, material=slab)
    box(key+"_rear_rail", "Upper deck parapet", x, y+10.9, upper, 28, .22, 1.06, material=slab)
    box(key+"_east_rail", "Upper deck parapet", x+13.9, y, upper, .22, 22, 1.06, material=slab)
    box(key+"_west_rail", "Landing parapet", x-13.9, y+7.2, upper, .22, 7.6, 1.06, material=slab)
    box(key+"_ramp_guard", "Ramp opening guard", x-10.1, y-3.7, upper, .22, 14.3, 1.06, material=slab)
    for i in range(5):
        px = x-7.8+i*4.6
        for j, dy in enumerate((-8.9, 8.9)):
            box(f"{key}_pier_{i}_{j}", "Parking column", px, y+dy, floor,
                .44, .44, upper-floor-.28, material=slab)
        for level in (0, 1):
            box(f"{key}_stall_{i}_{level}", "Parking line", px, y+7.1, floor+3.15*level+.012,
                .105, 4.75, .025, material=paint, collision=False)
    box(key+"_signboard", "Parking signboard", x+3.8, y-10.98, upper+.18, 4.1, .16, .66, material=trim)
    sign(key+"_sign", "PARKING", x+3.8, y-11.1, upper+.64)
    front = building_shell(box, key+"_equipment_room", x+10.4, y+7.6, floor,
                           3.7, 3.1, 2.6, 1.2, slab, trim)
    door(key+"_equipment_entry", "Maintenance door", x+10.4, front-.04, floor, 1.2, 2.35, finish="petrol")
    cache(key+"_maintenance", "Parking maintenance case", x+10.1, y+7.2, floor,
          timber_amount=7, cloth_amount=2)
    cache(key+"_vehicle_supplies", "Upper deck emergency kit", x+11.0, y+7.6, upper,
          food_amount=2, water_amount=2)
    encounter(key+"_walker", "Parking deck walker", x+4.9, y+1.7, upper)


def apartment_layout(box, door, cache, encounter, key, x, y, floor, seed):
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
        door(story+"_landing_entry", "Landing door", x+5.9, y-3.85, base, 1.3, 2.2, yaw=90)
        if level == 0:
            double_door(door, story+"_front_entry", "Apartment entrance", x, y-4.48, base, 2, 2.3, "petrol")
        # Two layouts share the landing and partitions but reverse their room entrances.
        for dx, side in ((-1, "west"), (1, "east")):
            door_y = 1.25 if level else -1.25
            door(f"{story}_{side}_entry", f"{side.title()} unit door", x+dx, y+door_y, base,
                 1.3, 2.2, yaw=90 if dx<0 else -90)
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
