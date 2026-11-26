"""Bake arterial pavement, overpasses, and the playable POI approaches."""
import math

from civic_landmarks import CAMPUS_PARKING_END
from city_pavement import is_city_route


def nearest_on_road(point, road):
    best = None
    for a, b in zip(road["points_m"], road["points_m"][1:]):
        delta = (b[0]-a[0], b[1]-a[1])
        length_squared = sum(d*d for d in delta)
        if length_squared == 0:
            continue
        t = sum((p-q)*d for p, q, d in zip(point, a, delta))/length_squared
        local = tuple(q+min(1, max(0, t))*d for q, d in zip(a, delta))
        candidate = math.dist(point, local), local
        if best is None or candidate < best:
            best = candidate
    return best[1]


def bake_route(plan, ground_height, terrain_height):
    def base_grade(x, y):
        return max(ground_height(x, y), terrain_height(x, y))+.12

    roads = {road["id"]: road for road in plan["roads"]}
    links = {
        "south-services": "southern-arterial", "warehouses": "southern-industrial-road",
        "city-hospital": "hospital-arterial", "city-police": "downtown-loop",
        "city-parking": "east-west-highway", "city-nightlife": "east-west-highway",
        "power-northeast":"headworks-road","water-treatment":"southern-industrial-road",
        "university": "airport-road", "school":"school-road", "north-clinic":"school-road",
        "north-police":"school-road", "mall":"school-road", "car-lot":"school-road",
        "north-services":"winter-approach", "winter-village":"winter-approach",
        "western-estates":"estate-avenue",
    }
    for site in plan["sites"]:
        if "driveway_end" in site:
            x,y = site["plot_xy_m"]
            roads[site["id"]+"_access"] = dict(id=site["id"]+"_access", width_m=6,
                points_m=[(x,y-site["footprint_m"][1]/2),site["driveway_end"]],
                start_z=site["foundation_z_m"]+.2)
        if site["id"] not in links:
            continue
        x, y = site["plot_xy_m"]
        entrance = (x, y+CAMPUS_PARKING_END if site["id"] == "university" else y-site["footprint_m"][1]/2)
        approach = (x, entrance[1]-8)
        if site["id"] == "warehouses":
            entrance = (x, y+site["footprint_m"][1]/2)
            approach = (x, entrance[1]+8)
        road = roads[links[site["id"]]]
        connection = nearest_on_road(approach, road)
        roads[site["id"]+"_access"] = {
            "id": site["id"]+"_access", "width_m": 8 if site["id"] == "university" else 7,
            "points_m": [entrance, approach, connection],
            "start_z": site["foundation_z_m"]+.2,
        }

    for site_id, road_id, offset, width in (
        ("stadium", "school-road", 25.5, 5),
        ("airport", "terminal-access", 17.5, 8),
    ):
        design = plan["facilities"][site_id]
        x, y = design["x"], design["y"]
        entry = (x, y+offset)
        approach = (x, entry[1]+8)
        connection = nearest_on_road(approach, roads[road_id])
        site = next(s for s in plan["sites"] if s["id"] == site_id)
        roads[site_id+"_access"] = {
            "id": site_id+"_access", "width_m": width,
            "points_m": [entry, approach, connection],
            "start_z": design.get("floor", site["foundation_z_m"]+.2),
        }

    # Keep bridge supports out of traffic lanes, including roads crossing beneath a viaduct.
    corridors = [(road["id"], a, b, road["width_m"]/2)
                 for road in roads.values()
                 for a,b in zip(road["points_m"],road["points_m"][1:]) if a != b]

    def corridor_clear(ident, x, y, margin):
        for other,a,b,half_width in corridors:
            if other == ident:
                continue
            dx,dy = b[0]-a[0], b[1]-a[1]
            t = max(0, min(1, ((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)))
            if math.hypot(x-a[0]-t*dx, y-a[1]-t*dy) < half_width+margin:
                return False
        return True

    baked = []
    for road in roads.values():
        urban = is_city_route(road)
        points = road["points_m"]
        distances = [math.dist(a, b) for a, b in zip(points, points[1:])]
        total = sum(distances)
        viaduct = road.get("viaduct")
        gap = viaduct.get("gap_fraction") if viaduct else None
        samples = []
        walked = 0
        for (start, end), distance in zip(zip(points, points[1:]), distances):
            if distance == 0:
                continue
            count = math.ceil(distance/(5 if urban else 10))
            samples.extend((start[0]+i/count*(end[0]-start[0]), start[1]+i/count*(end[1]-start[1]),
                            (walked+i/count*distance)/total) for i in range(count))
            walked += distance
        samples.append((*points[-1], 1.0))

        def grade(x, y, fraction):
            base = base_grade(x,y)
            if viaduct:
                # Keep the loop junctions on grade and raise the deck across the highway.
                # Short level junctions, then a smooth climb to the crossing.
                t = max(0, min(1, (min(fraction, 1-fraction)-.035)/.19))
                return base + viaduct["lift_m"] * t*t*(3-2*t)
            if fraction == 0 and "start_z" in road:
                return road["start_z"]
            return base

        segments = []
        broken_next = False
        for index, (start, end) in enumerate(zip(samples, samples[1:])):
            a, b = (*start[:2], grade(*start)), (*end[:2], grade(*end))
            x, y, z = ((v+w)/2 for v,w in zip(a,b))
            fraction = (start[2]+end[2])/2
            if gap and gap[0] <= fraction <= gap[1]:
                if not broken_next and segments:
                    segments[-1]["broken_end"] = True
                broken_next = True
                continue
            terrain = terrain_height(x, y)
            clearance = z-terrain
            segment = {"start": [round(v,3) for v in a], "end": [round(v,3) for v in b],
                       "bridge": clearance > 1.8}
            if urban:
                segment["parapet"] = clearance >= 3 and (bool(viaduct) or
                    corridor_clear(road["id"], x, y, road["width_m"]/2+2.5))
            if broken_next:
                segment["broken_start"] = True
                broken_next = False
            spacing = 7 if urban else 4
            if clearance > 3 and index % spacing == 0 and (not urban or corridor_clear(road["id"], x, y, 4)):
                segment["pier_base_z_m"] = round(terrain-.3, 3)
            segments.append(segment)
        baked.append({"id": road["id"], "width_m": road["width_m"], "segments": segments})
    return baked
