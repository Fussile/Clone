"""Airfield infrastructure and placements using the uploaded source meshes."""
import math


def pitch(box, mesh, key, design, palette, concrete):
    x, y, floor = design["x"], design["y"], design["floor"]
    # Mesh detail follows a sealed collision base so the photogrammetry edges don't snag movement.
    box(key+"_pitch_pad", "Community pitch ground", x, y, floor-1.4, 66, 50, 1.4,
        material=palette["grass"], ground=True)
    mesh(key+"_pitch", "Community soccer field", "CommunityPitch", x, y, floor-.025,
         scale=(.79, .70, 1), collision=True, align_long_axis=True)
    mesh(key+"_pitch_sign", "Soccer field entrance sign", "PitchEntranceSign", x+7, y+24.5, floor)
    box(key+"_path", "Pitch entrance path", x, y+24, floor-.24, 5, 3, .24,
        material=concrete, ground=True)
    for side in (-1, 1):
        box(f"{key}_bench_{side}", "Sideline bench", x+side*17, y+21, floor,
            3.6, .55, .45, material=palette["timber"])


def aircraft_hangar(box, mesh, door, cache, key, x, y, floor, concrete, scale=2.4):
    box(key+"_footing", "Hangar apron and floor", x, y, floor-.7, 36*scale, 29*scale, .7,
        material=concrete, ground=True)
    mesh(key+"_shell", "Aircraft hangar", "AircraftHangar", x, y, floor+.015,
         collision=True, opening_south=True, scale=(scale, scale, scale))
    door(key+"_aircraft_door", "Hangar shutter", x, y-12.55*scale, floor, 16.0*scale, 7.2*scale,
         roll_up=True, finish="dark_trim")
    cache(key+"_parts", "Aircraft service supplies", x-11.4*scale, y+4.0*scale, floor, cloth_amount=3,
          timber_amount=5)


def airfield(box, mesh, door, ramp, sign, window, cache, encounter, key, design, floor, palette,
             asphalt, concrete, dimensions):
    x, y, datum, slope = (design[k] for k in ("x", "y", "datum", "slope"))
    half = dimensions["RUNWAY_HALF"]
    runway_y = dimensions["RUNWAY_Y"]
    taxi_y = dimensions["TAXIWAY_Y"]
    apron_y = dimensions["APRON_Y"]
    road_pitch = math.degrees(math.atan(slope))

    def grade(dx):
        return datum+dx*slope

    def paved_strip(name, dx, dy, length, width, material):
        # Keep texture scale consistent with the surrounding runway segments.
        count = math.ceil(length/24)
        span = length/count
        for i in range(count):
            offset = dx-length/2+(i+.5)*span
            box(f"{key}_{name}_{i}", name.replace("_"," ").title(),
                x+offset, y+dy, grade(offset)-1.45, span+.08, width, 1.45, ground=True,
                material=material, rotation=(road_pitch, 0))

    paved_strip("runway", 0, runway_y, 2*half, 45, asphalt)
    mesh(key+"_runway_paint", "Runway threshold and center markings", "RunwayMarkings",
         x, y+runway_y, datum+.017, pitch=road_pitch, align_long_axis=True, scale=(2*half/426, 45/28, 1))
    paved_strip("parallel_taxiway", 0, taxi_y, 1800, 23, asphalt)
    for offset in dimensions["TURN_OFFS"]:
        box(f"{key}_turnoff_{offset}", "Runway turnoff", x+offset, y+(runway_y+taxi_y)/2,
            grade(offset)-1.46, 23, taxi_y-runway_y, 1.45, material=asphalt, ground=True,
            rotation=(road_pitch, 0))
        box(f"{key}_guide_{offset}", "Taxiway turnoff guide line", x+offset, y+(runway_y+taxi_y)/2,
            grade(offset)+.013, .13, taxi_y-runway_y, .008, material=palette["traffic"],
            collision=False)
    for i in range(150):
        offset = (i-74.5)*12
        box(f"{key}_taxi_line_{i}", "Taxiway centerline", x+offset, y+taxi_y, grade(offset)+.018,
            12.06, .14, .01, material=palette["traffic"], collision=False, rotation=(road_pitch, 0))
    paved_strip("terminal_apron", -11, apron_y, 238, 32, concrete)
    ramp(key+"_concourse_ramp", "Terminal apron approach", x-3, y-30, y-8,
         grade(-3), floor, 12, concrete)

    aircraft_hangar(box, mesh, door, cache, key+"_service_hangar", x+99, y-20,
                    grade(99)+.05, concrete, scale=1.8)
    ramp(key+"_tow_path", "Hangar taxi connector", x+99, y+taxi_y, y-43,
         grade(99), grade(99)+.05, 30, concrete)

    # Preserve the uploaded damaged terminal as a wreck alongside an enterable concourse.
    mesh(key+"_terminal_ruin", "Collapsed terminal wing", "RuinedTerminal", x+54, y+1, floor,
         collision=True)
    shop_floor = grade(-66)+.04
    box(key+"_shop_base", "Ruined shop ground", x-66, y+1, shop_floor-.38, 35, 41, .38,
        material=concrete, ground=True)
    mesh(key+"_shop_ruin", "Ruined airport shop", "RuinedAirportShop", x-66, y+1, shop_floor,
         collision=True)

    box(key+"_landside_path", "Landside pedestrian path", x-1.5, y+15.7, floor-.20,
        9, 4, .20, material=concrete, ground=True)
    tx, ty = x-3, y+3
    wall = palette["stucco_metal"]
    for side in (-1, 1):
        box(f"{key}_terminal_side_{side}", "Terminal end wall", tx+side*24.9, ty,
            floor, .22, 22, 5.1, material=wall)
        edge = ty+side*10.9
        for j in (-1, 1):
            box(f"{key}_terminal_entry_wall_{side}_{j}", "Terminal entrance wall",
                tx+j*13.4, edge, floor, 23.2, .22, 5.1, material=wall)
        box(f"{key}_terminal_header_{side}", "Terminal entrance lintel", tx, edge, floor+2.45,
            3.6, .22, 2.65, material=wall)
        for leaf in (-1, 1):
            door(f"{key}_terminal_door_{side}_{leaf}", "Terminal entrance door", tx+leaf*.9,
                 edge+side*.14, floor, 1.8, 2.45, reverse=leaf>0, finish="petrol")
    box(key+"_terminal_roof", "Terminal roof slab", tx, ty, floor+5.1, 51, 23, .35, material=concrete)
    for j in (-4,-3,-2,-1,1,2,3,4):
        window(f"{key}_terminal_window_{j}", tx+j*4.5, ty-11, floor+.5)
    for side in (-1, 1):
        box(f"{key}_terminal_counter_{side}", "Check-in counter", tx+side*12.5, ty+1.5,
            floor, 9.4, .9, 1.05, material=palette["petrol"])
        for i in range(3):
            box(f"{key}_terminal_bench_{side}_{i}", "Concourse seating", tx+side*(6.0+5*i),
                ty-5.5, floor, 3.4, .7, .46, material=palette["dark_trim"])
    sign(key+"_terminal_sign", "DOMESTIC TERMINAL", tx, ty-11.65, floor+4.5, size=50)
    cache(key+"_terminal_case", "Terminal emergency supplies", tx-12.5, ty+4.5, floor,
          water_amount=3, cloth_amount=3)
    encounter(key+"_terminal_walker", "Concourse walker", tx+12, ty-2.8, floor)

    cx, cy, base = x+32, y-41, grade(32)
    upper = base+5.4
    box(key+"_tower_base", "Control tower core", cx, cy, base-.45, 4.1, 4.1, 5.85, material=concrete)
    box(key+"_tower_deck", "Control room floor", cx, cy, upper-.23, 7, 7, .23, material=concrete)
    box(key+"_tower_roof", "Tower roof", cx, cy, upper+2.85, 7.2, 7.2, .28, material=concrete)
    for side in (-1, 1):
        box(f"{key}_tower_wall_{side}", "Tower observation side", cx+side*3.4, cy,
            upper, .18, 7, 2.85, material=palette["glazing"])
        box(f"{key}_tower_jamb_{side}", "Observation door jamb", cx+side*2.1, cy-3.4,
            upper, 2.7, .18, 2.85, material=palette["glazing"])
    box(key+"_tower_back", "Tower observation wall", cx, cy+3.4,
        upper, 7, .18, 2.85, material=palette["glazing"])
    box(key+"_tower_header", "Tower door header", cx, cy-3.4,
        upper+2.2, 1.5, .18, .65, material=palette["glazing"])
    door(key+"_tower_entry", "Control room door", cx, cy-3.45, upper, 1.5, 2.2, finish="petrol")
    ramp(key+"_tower_ramp", "Control tower access ramp", cx, cy-18, cy-3.5,
         base, upper, 2.6, concrete)
    sign(key+"_tower_sign", "CONTROL", cx, cy-3.7, upper+2.58)
    cache(key+"_tower_cache", "Control room supplies", cx+2.15, cy+2, upper,
          food_amount=2, water_amount=2)
