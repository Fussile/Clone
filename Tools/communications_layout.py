"""Place communications compounds and roadside defenses against the baked terrain."""
import math

COMPOUNDS = {"repeater": (32, 28), "radio": (66, 62), "transmitter": (38, 36), "substation": (31, 25)}


def bake_communications(plan, height, available, water_distance):
    from region_route import nearest_on_road

    roads = {road["id"]: road for road in plan["roads"]}
    reserved = []
    sites = []

    def elevations(cx, cy, width, depth):
        return [height(cx+width*i/4, cy+depth*j/4) for i in range(-2, 3) for j in range(-2, 3)]

    for spec in plan["communications"]:
        w, d = COMPOUNDS[spec["kind"]]
        clearance = math.hypot(w, d)/2 + 5
        x, y = spec["xy_m"]
        position = None
        for radius in range(0, 161, 10):
            candidates = [(round(x + radius*math.cos(angle*math.tau/16)),
                           round(y + radius*math.sin(angle*math.tau/16)))
                          for angle in range(16 if radius else 1)]
            for px, py in candidates:
                if not available(px, py, clearance) or any(
                    math.hypot(px-sx, py-sy) <= clearance+r for sx, sy, r in reserved
                ):
                    continue
                samples = elevations(px, py, w, d)
                if max(samples)-min(samples) <= 2.5:
                    position = (px, py)
                    break
            if position:
                break
        if not position:
            raise ValueError(f"No plot near communications site {spec['id']}")
        x, y = position
        reserved.append((x, y, clearance))
        samples = elevations(x, y, w, d)
        floor = round(max(samples)+.22, 3)
        footing = round(min(samples)-.65, 3)
        natural = spec["kind"] in {"radio", "transmitter"}
        ground = [[round(height(x+w*(i/4-.5), y+d*(j/4-.5)),3) for i in range(5)] for j in range(5)]
        entry = (x, y-d/2)
        approach = (x, entry[1]-14)
        road = roads[spec["road"]]
        connection = nearest_on_road(approach, road)
        # Join the nearest road where the map offers a short, dry approach.
        # The short apron still gives foot access at the other sites.
        points = [entry, approach]
        side = 1 if connection[0] > x else -1
        corner = (x+side*(w/2+8), approach[1])
        route = [approach, corner, connection] if connection[1] > entry[1] else [approach, connection]
        total = sum(math.dist(a, b) for a, b in zip(route, route[1:]))
        if total <= 480 and all(water_distance(a[0]+(b[0]-a[0])*t/16, a[1]+(b[1]-a[1])*t/16) > 8
                                for a, b in zip(route, route[1:]) for t in range(17)):
            points.extend(route[1:])
        segments = []
        for index, (a, b) in enumerate(zip(points, points[1:])):
            distance = math.dist(a, b)
            count = max(1, math.ceil(distance / 8))
            samples = [(a[0]+(b[0]-a[0])*i/count, a[1]+(b[1]-a[1])*i/count) for i in range(count+1)]
            heights = [height(*p)+.10 for p in samples]
            if index == 0 and not natural:
                # Span the whole apron down to grade instead of dropping at the pad edge.
                bottom = heights[-1]
                heights = [max(grade, floor+(bottom-floor)*i/count)
                           for i, grade in enumerate(heights)]
            positions = [[round(px, 3), round(py, 3), round(z, 3)] for (px, py), z in zip(samples, heights)]
            segments.extend({"start": start, "end": end, "bridge": False}
                            for start, end in zip(positions, positions[1:]))
        sites.append({**spec, "plot_xy_m": [x, y], "footprint_m": [w, d],
                      "ground_samples": ground, "floor_z_m": floor, "footing_z_m": footing,
                      "approach": {"id": spec["id"]+"_access", "width_m": 4.2,
                                   "surface": "gravel", "segments": segments}})
    return sites


def bake_checkpoints(specs, routes):
    by_id = {route["id"]: route for route in routes}
    checkpoints = []
    for spec in specs:
        route = by_id[spec["road"]]
        x, y = spec["xy_m"]
        candidates = []
        for segment in route["segments"]:
            a, b = segment["start"], segment["end"]
            length = math.dist(a[:2], b[:2])
            if segment["bridge"] or length < 3 or abs(a[2]-b[2])/length > .12:
                continue
            center = [(p+q)/2 for p, q in zip(a, b)]
            candidates.append((math.hypot(center[0]-x, center[1]-y), center, a, b))
        best = min(candidates, default=None)
        if best is None or best[0] > 90:
            raise ValueError(f"No level road near checkpoint {spec['id']}")
        _, center, a, b = best
        yaw = math.degrees(math.atan2(a[1]-b[1], b[0]-a[0]))
        checkpoints.append({**spec, "xyz_m": center, "start": a, "end": b,
                            "yaw_degrees": yaw, "width_m": route["width_m"]})
    return checkpoints


def stamp_compound(site, box, door, sign, shell, window, art, cache, encounter, palette):
    key = "CrossroadsComms_" + site["id"]
    if site["kind"] == "radio":
        key += "_Laitse"
    x, y = site["plot_xy_m"]
    w, d = site["footprint_m"]
    floor, base = site["floor_z_m"], site["footing_z_m"]
    concrete, trim, petrol, timber = (palette[k] for k in ("concrete", "dark_trim", "petrol", "timber"))
    natural = site["kind"] in {"radio", "transmitter"}
    def grade(px, py):
        if not natural:
            return floor
        u = min(4,max(0,(.5+(px-x)/w)*4))
        v = min(4,max(0,(.5+(py-y)/d)*4))
        i,j = min(int(u),3), min(int(v),3)
        grid = site["ground_samples"]
        return sum(grid[j+b][i+a]*(u-i if a else 1-u+i)*(v-j if b else 1-v+j)
                   for a in (0,1) for b in (0,1))
    if not natural:
        box(key+"_pad", "Communications yard foundation", x, y, base, w, d, floor-base,
        ground=True, material=concrete)

    def building(suffix, bx, by, width, depth, height, opening):
        if natural:
            box(key+suffix+"_footing", "Service building floor", bx, by, base, width, depth, floor-base, ground=True, material=concrete)
        front = shell(box, key+suffix, bx, by, floor, width, depth, height,
                      opening, concrete, trim)
        door(key+suffix+"_entrance", "Communications entrance", bx, front-.055, floor,
             opening, 2.35, finish="petrol")
        return front

    for side in (-1, 1):
        for index, offset in enumerate((-6, -1, 4)):
            art(f"{key}_security_{side}_{index}", "Perimeter wire barricade", "TimberWire",
                x+side*(w/2-1.22), y+offset, grade(x+side*(w/2-1.22), y+offset), (4.65, 1.3, 1.28), yaw=90,
                health=100)
        art(f"{key}_entry_chicane_{side}", "Concrete entry barricade", "CheckpointWall",
            x+side*5.4, y-d/2+1.3, grade(x+side*5.4, y-d/2+1.3), (5, .68, 1.35), health=180)
    sign(key+"_name", site["name"].upper(), x, y-d/2+.4, floor+2.65, size=40)

    if site["kind"] == "radio":
        # The survey is an exterior scan. Keep the accessible control room
        # in its own footprint so scan triangles never block the entrance.
        art(key+"_hall", "Laitse radio station", "LaitseStation",
            x, y+5, grade(x,y+5)-.8, (52, 47, 15))
        control_x = x-18
        floor = grade(control_x,y-23.3)+.12
        control_y = y-23.3
        front = building("_control", control_x, control_y, 10, 6.3, 3.15, 1.65)
        sign(key+"_control_sign", "BROADCAST CONTROL", control_x, front-.18, floor+2.85, size=24)
        box(key+"_console", "Shelved work station", control_x-2.7, control_y+.9, floor, 2, 1.5, 1.3)
        box(key+"_records", "Archive shelving", control_x+3.95, control_y+2.1, floor, .8, .6, 1.5)
        cache(key+"_studio_case", "Studio equipment case", control_x-3.7, control_y-1.8, floor, profile="OFFICE")
        cache(key+"_records_case", "Station records case", control_x+2.4, control_y+2.1, floor, profile="OFFICE")
        cache(key+"_first_aid", "Station first aid", control_x+3.7, control_y-1.8, floor, profile="MEDICAL")
        encounter(key+"_studio_walker", "Radio studio walker", control_x+.7, control_y+1.3, floor)
    else:
        large = site["kind"] == "transmitter"
        name = "SkalkaTower" if large else "RepeaterMast"
        dimensions = (20, 21, 42) if large else (11, 11, 36)
        art(key+"_tower", "Broadcast transmitter" if large else "Cell relay mast",
            name, x+5, y+3, grade(x+5,y+3)-(1.55 if large else 0), dimensions)
        shed_x, shed_y = x-11, y-2
        floor = grade(shed_x,shed_y)+(.12 if natural else 0)
        building("_service", shed_x, shed_y, 5.6, 5.8, 3.05, 1.45)
        cache(key+"_equipment", "Relay maintenance crate", shed_x-1.35, shed_y+1.2, floor,
              profile="BUILDING")
        cache(key+"_logbook", "Relay operator's supplies", shed_x+1.45, shed_y+.5, floor,
              profile="OFFICE")
        encounter(key+"_yard_walker", "Communications yard walker", x-3, y-d*.26, floor)


def stamp_checkpoint(checkpoint, art):
    key = "CrossroadsCheckpoint_" + checkpoint["id"]
    x, y, z = checkpoint["xyz_m"]
    a, b = checkpoint["start"], checkpoint["end"]
    extent = checkpoint["width_m"]/2
    # Stagger the barrier tips along the road and preserve a clear center lane.
    angle = math.radians(checkpoint["yaw_degrees"])
    along = (math.cos(angle), -math.sin(angle))
    normal = (math.sin(angle), math.cos(angle))
    slope = (b[2]-a[2])/math.dist(a[:2], b[:2])

    def piece(suffix, label, name, side, offset, lateral, dimensions, yaw, health):
        art(key+suffix, label, name,
            x+along[0]*offset+normal[0]*side*lateral,
            y+along[1]*offset+normal[1]*side*lateral,
            z+offset*slope, dimensions, yaw=checkpoint["yaw_degrees"]+yaw, health=health)

    width = min(5.1, extent-2.35)
    for side in (-1, 1):
        piece(f"_wall_{side}", "Abandoned concrete checkpoint barrier", "CheckpointWall",
              side, side*1.3, extent-.15-width/2, (width, .65, 1.4), 90, 180)
        piece(f"_wire_{side}", "Wire road barricade", "TimberWire",
              side, -side*2, extent-.65, (3.7, 1.1, 1.2), 0, 100)
        if extent >= 5:
            piece(f"_spikes_{side}", "Roadside spike barricade", "RoadSpikes",
                  side, side*3.3, extent-1.2, (2.2, .97, .82), 12*side, 65)
