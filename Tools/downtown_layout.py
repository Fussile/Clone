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


def bake_city(routes, height, available, seed):
    rng = random.Random(seed)
    buildings = []
    blockers = []
    for route in routes:
        if not route["id"].startswith("downtown-street-"):
            continue
        placements = list(segment_samples(route, 30))
        for index, ((x, y, z), dx, dy) in enumerate(placements):
            # Both rows face the same street, leaving roughly 4 m between lots.
            for side in (-1, 1):
                width, depth = rng.choice(((20, 15), (23, 18), (23, 17)))
                offset = route["width_m"]/2 + depth/2 + 6
                px, py = x-dy*offset*side, y+dx*offset*side
                margin = math.hypot(width, depth)/2
                if not available(px, py, margin):
                    continue
                if any(math.hypot(px-lot["x"], py-lot["y"]) < (width+lot["w"])/2+3.4
                       for lot in buildings):
                    continue
                samples = [height(px+dx*u-dy*v, py+dy*u+dx*v)
                           for u in (-width/2, width/2) for v in (-depth/2, depth/2)]
                if max(samples)-min(samples) > 2.8:
                    continue
                ident = f'{route["id"]}_{index}_{side}'
                face = rng.choice(("StoneFacade", "OfficeFacade", "OfficeFacade", "CornerShop"))
                levels = 3 if face == "CornerShop" else rng.choice((4, 4, 6, 7, 9))
                buildings.append(dict(id=ident, x=px, y=py, z=max(samples)+.08,
                                      base=min(samples)-.35, w=width, d=depth,
                                      yaw=math.degrees(math.atan2(-dy, dx))+(180 if side==-1 else 0),
                                      facade=face, levels=levels))
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


def stamp_city(lots, blockers, box, mesh, prop, door, cache, palette):
    for lot in lots:
        key = "CrossroadsInfill_"+lot["id"]
        x, y, z, w, d, yaw = (lot[k] for k in ("x", "y", "z", "w", "d", "yaw"))
        angle = math.radians(yaw)
        c, s = math.cos(angle), math.sin(angle)

        def point(u, v):
            return x+c*u+s*v, y-s*u+c*v

        def slab(suffix, label, u, v, floor, width, depth, height, finish):
            box(key+suffix, label, *point(u,v), floor, width, depth, height,
                material=palette[finish], rotation=(0, yaw))

        slab("_base", "City block foundation", 0, 0, lot["base"], w, d, z-lot["base"], "concrete")
        slab("_walk", "Street sidewalk", 0, -d/2-1.15, z-.20, w+.4, 2.3, .20, "concrete")
        style = lot["facade"]
        if style == "CornerShop":
            mesh(key+"_corner", "Brick corner shops", style, x, y, z, (w,d,13.8), yaw=yaw)
            continue

        height = lot["levels"]*3.65
        wall = "classical_civic" if style == "StoneFacade" else "modern"
        # Make the street-level recessed foyer reachable through a real door.
        for side in (-1, 1):
            slab(f"_foyer_{side}", "Foyer side wall", side*(w/2-.13), -d/2+2.7,
                 z, .26, 5.4, 3.24, wall)
            u = side*(w+1.75)/4
            slab(f"_front_{side}", "Retail display backing", u, -d/2+.22,
                 z, (w-1.75)/2, .42, 3.24, wall)
            mesh(f"{key}_storefront_{side}", "Street-level storefront", "Storefront",
                 *point(u, -d/2-.29), z, ((w-2.05)/2, .68, 3.2), yaw=yaw, collision=False)
        door(key+"_foyer_door", "Lobby door", *point(0,-d/2), z, 1.75, 2.35, yaw=yaw)
        slab("_lintel", "Lobby door lintel", 0, -d/2+.22, z+2.35, 1.75, .42, .89, wall)
        slab("_foyer_back", "Foyer rear wall", 0, -d/2+5.55, z, w, .32, 3.24, wall)
        slab("_structure", "Upper office block", 0, 0, z+3.24, w, d, height-3.24, wall)
        slab("_rear_base", "Rear service block", 0, 2.8, z, w, d-5.6, 3.24, wall)
        slab("_parapet", "Office roof cap", 0, 0, z+height, w+.36, d+.36, .34, "dark_trim")
        for face, offset, wide in ((0, -d/2, w), (180, -d/2, w), (90, -w/2, d), (270, -w/2, d)):
            theta = math.radians(face)
            for bay in (-1, 1):
                u, v = wide*.25*bay, offset-.20
                bx, by = point(math.cos(theta)*u+math.sin(theta)*v,
                               -math.sin(theta)*u+math.cos(theta)*v)
                mesh(f"{key}_facade_{face}_{bay}", "Upper floor facade", style, bx, by, z+3.24,
                     (wide*.5, .80 if style == "StoneFacade" else .35, height-3.25),
                     yaw=yaw+face, collision=False)
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
