"""Bake the street grid, frontage lots, and abandoned traffic for Crossroads."""
import math
import random


# Local streets meet the loop, and stop at the highway or the viaduct approaches.
STREETS = (
    ((2086, 2271), (2086, 2150), (2141, 2051)),
    ((2220, 2313), (2230, 2190), (2230, 2051)),
    ((2312, 2317), (2312, 2051)),
    ((2492, 2336), (2492, 2051)),
    ((2622, 2364), (2619, 2051)),
    ((2800, 2337), (2778, 2179), (2778, 2051)),
    ((2075, 2270), (2160, 2215), (2410, 2198), (2697, 2179), (2934, 2216)),
    ((1942, 2140), (2167, 2137), (2410, 2142)),
    ((2548, 2100), (2690, 2100), (2934, 2100)),
    ((2270, 1794), (2270, 2051)),
    ((2524, 1794), (2524, 2051)),
    ((2743, 1830), (2770, 2051)),
    ((2040, 1945), (2416, 1945), (2909, 1936)),
    ((1993, 1996), (2414, 1996), (2922, 1996)),
    ((2160, 2215), (2099, 2194), (2024, 2232)),
    ((2787, 2239), (2841, 2261), (2874, 2273)),
)


def streets():
    return [{"id": f"downtown-street-{index:02d}", "width_m": 8.5,
             "points_m": points} for index, points in enumerate(STREETS)]


def segment_samples(route, spacing):
    """Sample along the paved deck at distances independent of the bake tessellation."""
    remaining = spacing/2
    for segment in route["segments"]:
        a, b = segment["start"], segment["end"]
        length = math.dist(a[:2], b[:2])
        if length < .01:
            continue
        dx, dy = (b[0]-a[0])/length, (b[1]-a[1])/length
        while remaining < length:
            t = remaining/length
            pos = tuple(start+(end-start)*t for start, end in zip(a, b))
            if not segment["bridge"] and abs(b[2]-a[2])/length < .15:
                yield pos, dx, dy
            remaining += spacing
        remaining -= length


def lots_overlap(x, y, w, d, yaw, other):
    # Separating axes of the two oriented footprints, with a service passage.
    def axes(angle):
        theta = math.radians(-angle)
        return ((math.cos(theta), math.sin(theta)), (-math.sin(theta), math.cos(theta)))

    a, b = axes(yaw), axes(other["yaw"])
    dx, dy = other["x"]-x, other["y"]-y
    for ux, uy in (*a, *b):
        def span(basis, width, depth):
            return (width*abs(ux*basis[0][0]+uy*basis[0][1]) +
                    depth*abs(ux*basis[1][0]+uy*basis[1][1]))/2
        if abs(dx*ux+dy*uy) >= span(a, w, d)+span(b, other["w"], other["d"])+3:
            return False
    return True


def lot_is_available(available, x, y, dx, dy, width, depth):
    # Cover the rectangle with small discs. One diagonal-sized disc would reject
    # viable street frontage, especially between two parallel streets.
    nx, ny = math.ceil(width/9), math.ceil(depth/9)
    cell_w, cell_d = width/nx, depth/ny
    radius = math.hypot(cell_w, cell_d)/2
    for i in range(nx):
        u = (i+.5)*cell_w-width/2
        for j in range(ny):
            v = (j+.5)*cell_d-depth/2
            if not available(x+dx*u-dy*v, y+dy*u+dx*v, radius):
                return False
    return True


def bake_city(routes, height, available, seed):
    rng = random.Random(seed)
    buildings = []
    blockers = []
    city_routes = [route for route in routes if route["id"].startswith("downtown-street-")]
    for suffix, spacing, sizes in (
        ("", 39, ((30, 27), (33, 31), (35, 36))),
        ("_mid", 16, ((23, 20), (26, 22), (24, 19))),
    ):
        for route in city_routes:
            placements = list(segment_samples(route, spacing))
            for index, ((x, y, z), dx, dy) in enumerate(placements):
                # Reserve larger floor plates and leave a service alley between neighbors.
                for side in (-1, 1):
                    width, depth = rng.choice(sizes)
                    offset = route["width_m"]/2 + depth/2 + 3.2
                    px, py = x-dy*offset*side, y+dx*offset*side
                    if not lot_is_available(available, px, py, dx, dy, width, depth):
                        continue
                    yaw = math.degrees(math.atan2(-dy, dx)) + (180 if side == -1 else 0)
                    if any(lots_overlap(px, py, width, depth, yaw, lot) for lot in buildings):
                        continue
                    samples = [height(px+dx*u-dy*v, py+dy*u+dx*v)
                               for u in (-width/2, width/2) for v in (-depth/2, depth/2)]
                    if max(samples)-min(samples) > 2.8:
                        continue
                    ident = f'{route["id"]}_{index}_{side}{suffix}'
                    face = rng.choice(("StoneFacade", "OfficeFacade", "OfficeFacade", "CornerShop"))
                    core = math.hypot((px-2451)/340, (py-2069)/220) < 1
                    levels = 3 if face == "CornerShop" else rng.choice(
                        (13, 17, 21, 25) if core and face == "OfficeFacade" else (5, 9, 13))
                    buildings.append(dict(id=ident, x=px, y=py, z=max(samples)+.08, road_z=z, setback=3.2,
                                          base=min(samples)-.35, w=width, d=depth,
                                          yaw=yaw,
                                          facade=face, levels=levels))
    for route in city_routes:
        # Barricade local roads at intervals with a staggered pedestrian gap.
        for index, ((x, y, z), dx, dy) in enumerate(segment_samples(route, 81)):
            blockers.append(dict(id=f'{route["id"]}_{index}', x=x, y=y, z=z, dx=dx, dy=dy,
                                 yaw=math.degrees(math.atan2(-dy, dx)), width=route["width_m"]))
    return buildings, blockers


def bake_traffic(routes, sites, blockers, seed):
    rng = random.Random(seed+9101)
    towns = [s["plot_xy_m"] for s in sites if "plot_xy_m" in s and
             s["kind"] in {"village", "shops", "highrise", "garage", "services", "suburbs"}]
    parked = []
    for route in routes:
        ident = route["id"]
        if route["width_m"] < 7 or ident.endswith("_access"):
            continue
        highway = "highway" in ident
        city = ident.startswith("downtown-")
        for index, ((x, y, z), dx, dy) in enumerate(segment_samples(route, 26 if city else 39 if highway else 56)):
            near_town = any(math.hypot(x-tx, y-ty) < 205 for tx, ty in towns)
            if rng.random() > (.92 if city else .90 if highway else .80 if near_town else .16):
                continue
            if any(math.hypot(x-b["x"], y-b["y"]) < 8 for b in blockers):
                continue
            offset = (route["width_m"]/2-1.48) * rng.choice((-1, 1))
            px, py = x-dy*offset, y+dx*offset
            if any(math.hypot(px-car["x"], py-car["y"]) < 6.8 for car in parked):
                continue
            yaw = math.degrees(math.atan2(-dy, dx)) + rng.choice((0, 180)) + rng.uniform(-12, 12)
            model = rng.choices(("BurnedSedan", "BurnedCompact", "RustedTruck", "AbandonedVan"),
                                (30, 28, 12 if highway else 4, 36))[0]
            parked.append(dict(id=f"{ident}_{index}", x=px, y=py, z=z-.015, yaw=yaw, model=model))
    return parked


def stamp_city(lots, blockers, box, facade, prop, door, cache, palette):
    for lot in lots:
        key = "CrossroadsInfill_"+lot["id"]
        x, y, z, w, d, yaw = (lot[k] for k in ("x", "y", "z", "w", "d", "yaw"))
        angle = math.radians(yaw)
        c, s = math.cos(angle), math.sin(angle)

        def point(u, v):
            return x+c*u+s*v, y-s*u+c*v

        def slab(suffix, label, u, v, floor, width, depth, height, finish, pitch=0, heading=yaw):
            box(key+suffix, label, *point(u,v), floor, width, depth, height,
                material=palette[finish], rotation=(pitch, heading))

        slab("_base", "City block foundation", 0, 0, lot["base"], w, d, z-lot["base"], "concrete")
        rise = z-lot["road_z"]
        if rise > .17:
            run = lot["setback"]
            pitch = math.atan2(rise,run)
            slab("_entry_ramp", "Lobby approach", 0, -d/2-run/2,
                 z-rise/2-.15, math.hypot(run,rise), 2.3, .15, "concrete",
                 math.degrees(pitch), yaw-90)
            walkway_width = (w+.4-2.3)/2
            for side in (-1, 1):
                slab(f"_walk_{side}", "Entry sidewalk", side*(walkway_width+2.3)/2, -d/2-1.15,
                     z-.20, walkway_width, 2.3, .20, "concrete")
        else:
            slab("_walk", "Street sidewalk", 0, -d/2-1.15, z-.20, w+.4, 2.3, .20, "concrete")
        style = lot["facade"]
        if style == "CornerShop":
            style = "StoneFacade"

        height = lot["levels"]*3.65
        wall = "classical_civic" if style == "StoneFacade" else "modern"
        # Make the street-level recessed foyer reachable through a real door.
        lobby_depth = d * .47
        for side in (-1, 1):
            slab(f"_foyer_{side}", "Foyer side wall", side*(w/2-.13), -d/2+lobby_depth/2,
                 z, .26, lobby_depth, 3.24, wall)
            u = side*(w+1.75)/4
            slab(f"_front_{side}", "Retail display backing", u, -d/2+.22,
                 z, (w-1.75)/2, .42, 3.24, wall)
        door(key+"_foyer_door", "Lobby door", *point(0,-d/2), z, 1.75, 2.35, yaw=yaw)
        slab("_lintel", "Lobby door lintel", 0, -d/2+.22, z+2.35, 1.75, .42, .89, wall)
        slab("_foyer_back", "Foyer rear wall", 0, -d/2+lobby_depth, z, w, .32, 3.24, wall)
        slab("_structure", "Upper office block", 0, 0, z+3.24, w, d, height-3.24, wall)
        slab("_rear_base", "Rear service block", 0, lobby_depth/2, z, w, d-lobby_depth, 3.24, wall)
        slab("_parapet", "Office roof cap", 0, 0, z+height, w+.36, d+.36, .34, "dark_trim")
        facade(key+"_panels", style, x, y, z, w, d, height, yaw, 3.24, 1.75)
        cache(key+"_lobby_cache", "Lobby scavenging cache", *point(-w*.28, -d/2+3.5), z,
              profile="OFFICE", chance=82)

    for barrier in blockers:
        key = "CrossroadsStreetBarrier_"+barrier["id"]
        x,y,z,dx,dy,yaw,w = (barrier[field] for field in ("x","y","z","dx","dy","yaw","width"))
        for side in (-1, 1):
            offset = w/2-1.42
            px, py = x-dy*offset*side, y+dx*offset*side
            prop(f"{key}_concrete_{side}", "Downtown concrete barricade", "CheckpointWall",
                 px+dx*side*2.7, py+dy*side*2.7, z, (2.8,.68,1.25), yaw=yaw+90, health=170)
            prop(f"{key}_wire_{side}", "Downtown wire barricade", "TimberWire",
                 px-dx*side*2.7, py-dy*side*2.7, z, (2.7,1.05,1.05), yaw=yaw+83, health=95)
