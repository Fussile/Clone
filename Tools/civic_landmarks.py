"""Author playable interiors and a walkable campus around the City Sample exteriors."""
import math

HEROES = {
    "police": ("Bldg_Hero_CHA_A01", True),
    "civic": ("Bldg_Hero_Mid_NYG_Triangle_B01", True),
    "library": ("Bldg_Hero_SFB", False),
    "academic": ("Bldg_Hero_Low_SFD_Long", True),
    "hospital": ("Bldg_Hero_Mid_NYG_Triangle_B01", True),
    "tower": ("Bldg_Hero_NYG_Modern_Square_A01", True),
    "science": ("Bldg_Hero_NYG_Modern_Square_A01", True),
}
CAMPUS_SIZE = (178, 216)
CAMPUS_PARKING_END = -98


def hall(key, x, y, floor, width, depth, levels, yaw, role, title, box, door, cache, encounter,
         sign, art, palette, full_envelope=False):
    story = 4.0 if role in {"library", "civic"} else 3.65
    height = levels*story
    exterior_height = height + {"hospital": 5, "tower": 3}.get(role, 0)*story
    entry = 3.2 if role != "academic" else 2.6
    front = -depth/2
    skinned = art(key, "science" if role == "academic" and levels >= 3 else role, x, y, floor, width, depth, exterior_height, story, yaw, entry)
    cs, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def xy(u, v):
        return x+u*cs+v*sn, y-u*sn+v*cs

    def part(tag, label, u, v, dz, w, d, h, finish="classical_civic", **kwargs):
        pitch = kwargs.pop("pitch", 0)
        facing = kwargs.pop("facing", 0)
        box(f"{key}_{tag}", label, *xy(u,v), floor+dz, w, d, h, material=palette[finish],
            rotation=(pitch, yaw+facing), **kwargs)

    def leaf(tag, label, u, v, dz, w, h, *, reverse=False):
        door(f"{key}_{tag}", label, *xy(u,v), floor+dz, w, h, yaw=yaw, reverse=reverse, finish="petrol")

    part("foundation", "Building footing", 0, 0, -1.0, width, depth, 1, "concrete", ground=True)
    for side in (-1, 1):
        part(f"side_{side}", "Hall exterior wall", side*(width/2-.28), 0, 0, .26, depth-.28, height,
             hidden=skinned)
        part(f"front_{side}", "Entrance wall", side*(width+entry)/4, front+.18,
             0, (width-entry)/2-.18, .27, height, hidden=skinned)
        leaf(f"entry_{side}", title+" entrance", side*entry/4, front+.01, 0, entry/2, 2.65, reverse=side>0)
    part("header", "Stone entrance lintel", 0, front+.18, 2.65, entry, .34, height-2.65, hidden=skinned)
    if skinned:
        for side in (-1,1):
            part(f"trim_{side}", "Recessed entrance jamb", side*(entry/2+.105), front-.03,
                 0, .21, .4, 2.65)
        # A source bay can span more than one storey. Line the recessed portal
        # behind it when the skin baker clears that bay for the working doors.
        for side in (-1, 1):
            part(f"portal_return_{side}", "Entrance reveal", side*(entry/2+.12),
                 front+1.2, 0, .24, 2.4, story)
            part(f"portal_spandrel_{side}", "Lobby glazing", side*(entry/2+1.45),
                 front+.42, 0, 2.7, .16, story, "glazing")
        part("portal_transom", "Entrance transom", 0, front+.38, 2.93,
             entry+.4, .18, story+.2-2.93, "glazing")
        part("trim_header", "Entrance surround", 0, front-.03, 2.65, entry+.42, .4, .28)
    part("back", "Hall rear wall", 0, depth/2-.28, 0, width-.30, .26, height, hidden=skinned)
    part("roof", "Ceiling slab", 0, 0, height, width-.25, depth-.25, .25, "dark_trim",
         hidden=skinned and not full_envelope, collision=not skinned or full_envelope)
    # Sample trim, terraces and the roof silhouette are layered outside this playable core.
    if not skinned:
        for n in range(levels):
            for i in range(4):
                u = width*(i-1.5)/5
                part(f"window_{n}_{i}", "Recessed façade panel", u, front-.03, n*story+1.05,
                     width/7, .15, 1.65, "glazing", collision=False)
        for side in (-1, 1):
            part(f"pilaster_{side}", "Entry stone pilaster", side*(entry/2+.48), front-.34,
                 0, .58, .56, story)
    sign(key+"_sign", title.upper(), *xy(0, front-.24), floor+3.05, yaw=yaw+90)

    # Bounds include spires, roof furniture and setbacks. Reserve the upper
    # silhouette for the pack art and build stairs only inside the occupied core.
    if skinned and not full_envelope:
        width_scale, depth_scale, playable_levels = {
            "library": (.60, .60, 3), "civic": (.43, .52, 6),
            "hospital": (.43, .52, 12), "tower": (.66, .70, 22),
            "police": (.60, .65, 3), "academic": (.72, .72, levels),
        }[role]
        width *= width_scale
        depth *= depth_scale
        levels = min(levels, playable_levels)

    shaft = 4.8
    shaft_u = width/2-shaft/2-.44
    stair_length = min(12.0, depth*.65)
    landing = (depth-stair_length)/2-.22
    for n in range(levels):
        z = n*story
        if n:
            part(f"deck_{n}", "Upper floor slab", -shaft/2-.07, 0, z-.23,
                 width-shaft-.66, depth-.44, .23, "concrete")
            for side in (-1, 1):
                part(f"landing_{n}_{side}", "Stair landing", shaft_u,
                     side*(stair_length+landing)/2, z-.23, shaft+.06, landing, .23, "concrete")
        if n+1 < levels:
            direction = 1 if n%2 == 0 else -1
            stair_pitch = math.atan2(story, stair_length)
            thickness = .26
            center_v = direction*thickness*math.sin(stair_pitch)/2
            part(f"stairs_{n}", "Stairwell ramp", shaft_u-direction*1.18, center_v,
                 z+story/2-thickness*(1+math.cos(stair_pitch))/2, math.hypot(stair_length, story),
                 2.28, thickness, "concrete", pitch=math.degrees(stair_pitch), facing=-90*direction)
            part(f"guard_{n}", "Stair guard", shaft_u-shaft/2, 0, z, .12, stair_length, 1.03, "dark_trim")

        # The front lobby reaches the stairs around the end of the rear suite partition.
        partition_end = width/2-shaft-.75
        room_entry = -width*.24
        for side,(a,b) in enumerate(((-width/2+.25, room_entry-.8), (room_entry+.8, partition_end))):
            part(f"suite_wall_{n}_{side}", "Room partition", (a+b)/2, depth*.13, z,
                 b-a, .22, story-.23)
        leaf(f"suite_{n}", "Reading room door" if role == "library" else "Ward door" if role == "hospital" else "Office door",
             room_entry, depth*.13, z, 1.6, 2.35)
        if role == "hospital":
            part(f"nurse_{n}", "Reception desk", -width*.22, -depth*.18, z, 2.8, .9, 1.05)
            part(f"meds_{n}", "Treatment room cabinet", width*.18, depth*.39, z, 1.5, .65, 1.5)
        elif role in {"library","police"}:
            for bay in range(3):
                part(f"shelves_{n}_{bay}", "Archive shelving", -width*.34+bay*width*.14, depth*.34,
                     z, 1.7, .6, 1.85, "timber")
        else:
            for row in (-.12,.31):
                part(f"tables_{n}_{row}", "Classroom table" if role=="academic" else "Civic work table",
                     -width*.20, depth*row, z, 2.8, .88, .82, "timber")
        profile = {"hospital": "MEDICAL", "police": "SECURITY"}.get(role, "OFFICE")
        cache(f"{key}_supplies_{n}", "Room supply cabinet", *xy(-width*.27,depth*.34), floor+z,
              profile=profile, chance=85)
    if role in {"police", "civic", "library"}:
        part("reception", "Reception desk", -width*.21, -depth*.21, 0, 3.1, .84, 1.05, "timber")
    if role == "police":
        cell_u, cell_v = width*.13, depth*.33
        for side in (-1,1):
            part(f"holding_side_{side}", "Holding room partition", cell_u+side*1.6, cell_v,
                 0, .20, 3.0, story-.22)
            part(f"holding_front_{side}", "Holding room jamb", cell_u+side*1.18, cell_v-1.4,
                 0, .95, .20, story-.22)
        part("holding_back", "Holding room rear wall", cell_u, cell_v+1.45,
             0, 3.2, .20, story-.22)
        part("holding_lintel", "Holding room lintel", cell_u, cell_v-1.4,
             2.35, 1.4, .25, story-2.57)
        leaf("holding_entry", "Holding room door", cell_u, cell_v-1.49, 0, 1.4, 2.35)
    encounter(key+"_walker", title+" walker", *xy(-width*.28,-depth*.14), floor)


def place_landmark(key, role, x, y, floor, box, door, cache, encounter, sign, art, palette):
    width, depth, levels, title = {
        "police": (28, 21, 5, "Crossroads Police Headquarters"),
        "civic": (42, 27, 13, "Crossroads Civic Center"),
    }[role]
    box(key+"_plaza", "Public entrance plaza", x, y-depth/2-1.7, floor-.42,
        width*.53, 3.4, .42, material=palette["concrete"], ground=True)
    hall(key, x, y, floor, width, depth, levels, 0, role, title,
         box, door, cache, encounter, sign, art, palette)


def campus(key, x, y, floor, box, door, cache, encounter, sign, art, wreck, palette):
    concrete = palette["concrete"]
    box(key+"_walk", "University central promenade", x, y-9, floor-1, 7, 155, 1,
        material=concrete, ground=True)
    box(key+"_plaza", "Library entrance plaza", x, y+60, floor-1, 40, 16, 1,
        material=concrete, ground=True)

    box(key+"_parking", "University parking lot", x, y-86, floor-1, 50, 24, 1,
        material=palette["asphalt"], ground=True)
    for side in (-1, 1):
        for i in range(8):
            px = x+side*(4.8+i*2.5)
            for row in (-1, 1):
                box(f"{key}_stall_{side}_{row}_{i}", "Parking stall line", px, y-86+row*7.7,
                    floor+.01, .095, 5.1, .017, material=palette["traffic"], collision=False)
            if i in (1, 5):
                wreck(f"{key}_car_{side}_{i}", px+side*1.24, y-78.5, floor, 90,
                      "BurnedSedan" if i==1 else "BurnedCompact")
    sign(key+"_campus_sign", "CROSSROADS UNIVERSITY", x-10, y-96.9, floor+2.45, size=46)

    wings = (
        (-51,-43, 43, 2, "Admissions"),
        ( 51,-43, 44, 3, "Humanities"),
        (-51, 16, 52, 3, "Science Hall"),
        ( 51, 16, 52, 4, "Engineering Hall"),
    )
    for i,(u,v,width,levels,title) in enumerate(wings):
        side = 1 if u>0 else -1
        depth = 34
        distance = abs(u)-depth/2-3.5
        box(f"{key}_branch_walk_{i}", "Academic hall approach", x+side*(3.5+distance/2), y+v,
            floor-1, distance+.18, 3.6, 1, material=concrete, ground=True)
        hall(f"{key}_wing_{i}", x+u, y+v, floor, width, depth, levels, side*90, "academic", title,
             box, door, cache, encounter, sign, art, palette, full_envelope=True)
    hall(key+"_library", x, y+84, floor, 62, 42, 5, 0, "library", "University Library",
         box, door, cache, encounter, sign, art, palette, full_envelope=True)
