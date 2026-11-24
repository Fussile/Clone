"""Bake the authored regional layout, an Unreal heightmap, and a planning map."""
import argparse
import html
import json
import math
from pathlib import Path
import struct
import zlib

from airfield_layout import facilities, RUNWAY_HALF, RUNWAY_Y, TAXIWAY_Y
from civic_landmarks import CAMPUS_SIZE
from communications_layout import bake_checkpoints, bake_communications
from region_route import bake_route
from rail_layout import bake_rail
from downtown_layout import bake_city, bake_traffic, streets
from utility_layout import bake_grid

LAYOUT = Path(__file__).with_name("world_layout.json")
RESOLUTION = 2017
# East/west and north/south footprints for the first blockout, in meters.
FOOTPRINTS = {
    "hospital": (66, 48), "highrise": (48, 42), "garage": (48, 32),
    "civic": (54, 36), "police": (34, 26), "industry": (56, 38),
    "airport": (160, 35), "shops": (36, 22), "clinic": (24, 18),
    "services": (23, 19), "suburbs": (22, 15), "ranch": (24, 18),
    "village": (23, 17),
}


def segments(points):
    return zip(points, points[1:])


def segment_distance(x, y, start, end):
    dx, dy = end[0] - start[0], end[1] - start[1]
    length_squared = dx*dx + dy*dy
    if length_squared == 0:
        return math.hypot(x-start[0], y-start[1])
    t = max(0, min(1, ((x - start[0]) * dx + (y - start[1]) * dy) / length_squared))
    return math.hypot(x - start[0] - t*dx, y - start[1] - t*dy)


def intersection(a, b, c, d):
    dx, dy = b[0] - a[0], b[1] - a[1]
    ex, ey = d[0] - c[0], d[1] - c[1]
    cross = dx*ey - dy*ex
    if not cross:
        return None
    t = ((c[0] - a[0])*ey - (c[1] - a[1])*ex) / cross
    u = ((c[0] - a[0])*dy - (c[1] - a[1])*dx) / cross
    if 0 <= t <= 1 and 0 <= u <= 1:
        return [round(a[0]+dx*t), round(a[1]+dy*t)]
    return None


def generate(seed):
    plan = json.loads(LAYOUT.read_text(encoding="utf-8"))
    width, depth = plan["size_m"]
    river_segments = [(a, b, river["width_m"]) for river in plan["rivers"] for a, b in segments(river["points_m"])]
    fissures = [(34, 44, list(segments(plan["fissure_m"]))),
                (24, 28, list(segments(plan["depot_fissure_m"])))]
    hill_specs = [(hill["xy_m"], hill["radius_m"], hill["rise_m"]) for hill in plan["hills"]]
    basin_x, basin_y = plan["lowland"]["xy_m"]
    basin_rx, basin_ry = plan["lowland"]["radii_m"]
    for line in [*plan["rivers"], *plan["roads"]]:
        assert all(0 <= x <= width and 0 <= y <= depth for x, y in line["points_m"]), line["id"]

    def smoothstep(t):
        t = max(0.0, min(1.0, t))
        return t*t*(3-2*t)

    def ground_height(x, y):
        north = smoothstep((y-1900)/1850)
        ripple = math.sin((x + seed % 731)/420) * math.cos(y/480)
        base = 7 + ripple*(1.8 + 4*north) + 109*north
        # The basin has a broad, wet floor and opens toward the western watershed.
        basin_distance = ((x-basin_x)/basin_rx)**2 + ((y-basin_y)/basin_ry)**2
        basin = 1-smoothstep((basin_distance-.16)/.84)
        base = base*(1-basin) + (2.5+.7*ripple)*basin
        for (hx, hy), radius, rise in hill_specs:
            dx, dy = abs(x-hx), abs(y-hy)
            if dx < radius and dy < radius:
                hill = max(0.0, 1-(dx*dx+dy*dy)/(radius*radius))
                base += rise*hill*hill
        # Limit the steepest rock faces to the final strip near the mountain boundary.
        escarpment = max(0, 1-min(x,width-x)/280, 1-(depth-y)/205)
        return base + (111 + 12*math.cos(x/180-y/260))*escarpment**2

    # Draw the north's lakes at their own basin levels and let the rivers follow the grade.
    lake_shapes = [(lake["xy_m"], lake["radii_m"],
                    max(0, ground_height(*lake["xy_m"])-4))
                   for lake in plan["lakes"]]

    def nearest_lake(x, y):
        return min(((math.hypot((x-cx)/rx, (y-cy)/ry)-1)*min(rx,ry), level)
                    for (cx,cy),(rx,ry),level in lake_shapes)

    def river_distance(x, y):
        return min(segment_distance(x,y,a,b)-w/2 for a,b,w in river_segments)

    def water_distance(x, y):
        return min(nearest_lake(x,y)[0], river_distance(x,y))

    plots = []
    road_segments = [(a, b, road["width_m"]) for road in plan["roads"] for a, b in segments(road["points_m"])]

    def available(px, py, clearance):
        return (clearance < min(px, width-px, py, depth-py)
                and water_distance(px, py) > clearance + 12
                and all(segment_distance(px, py, a, b) > clearance + rw/2 for a, b, rw in road_segments)
                and all(segment_distance(px, py, a, b) > clearance + span
                        for span, _, fault in fissures for a, b in fault)
                and all(math.hypot(px-sx, py-sy) > clearance + sr for sx, sy, sr, _ in plots))

    parcel_sizes = {"university": CAMPUS_SIZE, "warehouses": (154, 92), "hangars": (180, 90),
                    "west-entertainment": (106, 74), "east-entertainment": (106, 74)}
    pads = []
    for site in plan["sites"]:
        if site["kind"] not in FOOTPRINTS:
            continue
        w, d = parcel_sizes.get(site["id"], (98, 64) if site["kind"] == "suburbs" else FOOTPRINTS[site["kind"]])
        clearance = math.hypot(w, d)/2 + 5
        x, y = site["xy_m"]

        for radius in range(0, 301, 10):
            candidates = [(round(x + radius*math.cos(i*math.tau/16)),
                           round(y + radius*math.sin(i*math.tau/16))) for i in range(16 if radius else 1)]
            position = next(((px, py) for px, py in candidates if available(px, py, clearance)), None)
            if position is not None:
                break
        if position is None:
            raise ValueError(f"No dry buildable plot near {site['id']}")
        px, py = position
        z = round(ground_height(px, py), 2)
        pads.append((px, py, w/2, d/2, z))
        plots.append((px, py, clearance, z))
        site.update(plot_xy_m=[px, py], footprint_m=[w, d], foundation_z_m=z,
                    construction="shared_kit" if site["kind"] in {"suburbs", "ranch", "village", "services"}
                    else "custom_shell_shared_structure")

    bridges = []
    for road in plan["roads"]:
        for river in plan["rivers"]:
            hits = {tuple(point) for a, b in segments(road["points_m"]) for c, d in segments(river["points_m"])
                    if (point := intersection(a, b, c, d))}
            for index, point in enumerate(sorted(hits), 1):
                bridges.append({"id": f'{road["id"]}-{river["id"]}-{index}', "xy_m": point,
                                "road": road["id"], "river": river["id"]})
    plan.update(version=8, seed=seed, bridge_candidates=bridges, stream_cell_m=256, stream_radius_m=768,
                climate_celsius_offset={"north_edge": -15, "city": 0, "south_edge": 14},
                terrain={"resolution": RESOLUTION, "xyz_scale_cm": [width*100/(RESOLUTION-1),
                                                                  depth*100/(RESOLUTION-1), 100]})

    lot_cells = {}
    airside = None

    def height(x, y):
        base = ground_height(x, y)
        river = river_distance(x,y)
        base -= 6.0*(1-smoothstep((river+3)/14))
        shore, lake_level = nearest_lake(x,y)
        bank = smoothstep((shore+3)/20)
        base = (lake_level-3)*(1-bank) + base*bank
        for span, drop, fault in fissures:
            gap = min(segment_distance(x,y,a,b) for a,b in fault)
            base -= drop*max(0.0, 1-gap/span)**2
        for px, py, half_w, half_d, z in pads:
            dx, dy = abs(x-px)-half_w, abs(y-py)-half_d
            if max(dx,dy) < 34:
                distance = math.hypot(max(0, dx), max(0, dy))
                base += (z-base)*(1-smoothstep((distance-8)/26))
        # Grade the foundation itself, then taper the fill into its street setback.
        for cx, cy, cs, sn, hw, hd, top in lot_cells.get((int(x//64), int(y//64)), ()):
            dx, dy = x-cx, y-cy
            edge = math.hypot(max(0, abs(dx*cs+dy*sn)-hw), max(0, abs(dy*cs-dx*sn)-hd))
            if edge < 3:
                base += (top-base)*(1-smoothstep(edge/3))
        if airside:
            dx, dy = x-airside["x"], y-airside["y"]
            if abs(dx) < RUNWAY_HALF+42 and RUNWAY_Y-65 < dy < -10:
                runway_edge = max(abs(dx)-RUNWAY_HALF, abs(dy-RUNWAY_Y)-22.5)
                taxi_edge = max(abs(dx)-900, abs(dy-TAXIWAY_Y)-11.5)
                infield_edge = max(abs(dx)-900, RUNWAY_Y-dy, dy-TAXIWAY_Y)
                blend = 1-smoothstep((min(runway_edge, taxi_edge, infield_edge)-8)/30)
                grade = airside["datum"]+dx*airside["slope"]-.14
                base += (grade-base)*blend
        return base

    sites = {site["id"]: site for site in plan["sites"]}
    plan["facilities"] = facilities(sites["airport"], sites["stadium"], height)
    airside = plan["facilities"]["airport"]
    # Keep the established POI pads and landscape. Widen the loop after plot placement.
    for road in plan["roads"]:
        if road["id"] == "downtown-loop":
            road["width_m"] = 13
        if road["id"] == "downtown-overpass-east":
            road["viaduct"]["lift_m"] = 7.4
    local_streets = streets()
    plan["roads"].extend(local_streets)
    road_segments.extend((a, b, road["width_m"]) for road in plan["roads"]
                         if road["id"] == "downtown-loop" or road in local_streets
                         for a, b in segments(road["points_m"]))
    plan["m4_route"] = bake_route(plan, ground_height, height)
    # The new pads follow today's terrain so an existing editor map needs no landscape replacement.
    plan["communications"].extend(plan["substations"])
    compounds = bake_communications(plan, height, available, water_distance)
    plan["substations"] = [site for site in compounds if site["kind"] == "substation"]
    for site in plan["substations"]:
        px, py = site["plot_xy_m"]
        w, d = site["footprint_m"]
        pads.append((px, py, w/2, d/2, site["floor_z_m"]-.14))
    plan["communications"] = [site for site in compounds if site["kind"] != "substation"]
    plan["checkpoints"] = bake_checkpoints(plan["checkpoints"], plan["m4_route"])
    plan["m4_route"].extend(site["approach"] for site in compounds)
    road_segments.extend((s["start"][:2], s["end"][:2], route["width_m"])
                         for route in plan["m4_route"] if route["id"].endswith("_access")
                         for s in route["segments"])
    plan["rail"] = bake_rail(plan["m4_route"], ground_height, height, water_distance)
    rail_segments = [(s["start"][:2], s["end"][:2], track["width_m"]/2+1)
                     for track in plan["rail"]["tracks"] for s in track["segments"]]
    rail_cells = {}
    for a,b,radius in rail_segments:
        for cx in range(int(min(a[0],b[0])//64),int(max(a[0],b[0])//64)+1):
            for cy in range(int(min(a[1],b[1])//64),int(max(a[1],b[1])//64)+1):
                rail_cells.setdefault((cx,cy),[]).append((a,b,radius))

    def district_available(x, y, clearance):
        return available(x, y, clearance) and all(
            segment_distance(x,y,a,b)>clearance+radius
            for cx in range(int((x-clearance-5)//64),int((x+clearance+5)//64)+1)
            for cy in range(int((y-clearance-5)//64),int((y+clearance+5)//64)+1)
            for a,b,radius in rail_cells.get((cx,cy),())
        ) and all(
            math.hypot(x-site["plot_xy_m"][0], y-site["plot_xy_m"][1]) >
            clearance + math.hypot(*site["footprint_m"])/2+1 for site in compounds
        )
    plan["city_lots"], plan["city_barriers"] = bake_city(plan["m4_route"], height, district_available, seed)
    for lot in plan["city_lots"]:
        x, y = lot["x"], lot["y"]
        angle = math.radians(-lot["yaw"])
        w, d = lot["w"]/2, lot["d"]/2
        radius = math.hypot(w, d)+3
        pad = (x, y, math.cos(angle), math.sin(angle), w, d, lot["z"]-.14)
        for i in range(int((x-radius)//64), int((x+radius)//64)+1):
            for j in range(int((y-radius)//64), int((y+radius)//64)+1):
                lot_cells.setdefault((i,j), []).append(pad)

    def pole_available(x, y, clearance):
        if abs(x-airside["x"]) < RUNWAY_HALF+35 and RUNWAY_Y-60 < y-airside["y"] < 15:
            return False
        return district_available(x, y, clearance) and all(
            math.hypot(x-lot["x"], y-lot["y"]) > math.hypot(lot["w"], lot["d"])/2+clearance
            for lot in plan["city_lots"])

    plan["wrecks"] = bake_traffic(plan["m4_route"], plan["sites"], plan["city_barriers"], seed)
    plan["grid"] = bake_grid(plan, sites, height, pole_available)

    # Assign static content to planned cells. IDs persist when exact coordinates change.
    cells = {}
    for site in [*plan["sites"], *plan["communications"], *plan["substations"]]:
        x, y = site.get("plot_xy_m", site["xy_m"])
        cells.setdefault((x//256, y//256), []).append(site["id"])
    plan["site_cells"] = [{"xy": key, "sites": ids} for key, ids in sorted(cells.items())]
    return plan, height


def write_heightmap(path, plan, height, raw_path=None):
    width, depth = plan["size_m"]
    columns, rows = 500, 400
    stride_x, stride_y = width/columns, depth/rows
    coarse = [[height(x*stride_x, depth-y*stride_y) for x in range(columns+1)] for y in range(rows+1)]
    x_fractions = [x*columns/(RESOLUTION-1) for x in range(RESOLUTION)]
    x_samples = [(min(int(value), columns-1), value) for value in x_fractions]
    compressor = zlib.compressobj(level=6)
    output = []
    raw_rows = []
    for y in range(RESOLUTION):
        fraction_y = y*rows/(RESOLUTION-1)
        iy = min(int(fraction_y), rows-1)
        north, south = coarse[iy:iy+2]
        row = [a+(b-a)*(fraction_y-iy) for a,b in zip(north,south)]
        values = [max(0, min(65535, round(32768 + 128*(row[ix] + (row[ix+1]-row[ix])*(f-ix)))))
                  for ix, f in x_samples]
        if raw_path:
            raw_rows.append(struct.pack(f"<{RESOLUTION}H", *values))
        output.append(compressor.compress(b"\0" + struct.pack(f">{RESOLUTION}H", *values)))
    output.append(compressor.flush())
    if raw_path:
        raw_path.write_bytes(b"".join(raw_rows))

    def chunk(kind, data):
        return struct.pack(">I", len(data))+kind+data+struct.pack(">I", zlib.crc32(kind+data))

    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     chunk(b"IHDR", struct.pack(">IIBBBBB", RESOLUTION, RESOLUTION, 16, 0, 0, 0, 0)) +
                     chunk(b"IDAT", b"".join(output)) + chunk(b"IEND", b""))


def write_map(path, plan):
    width, depth = plan["size_m"]
    map_w, map_h = 1000, 800

    def svg_xy(xy):
        return xy[0]/5, 74+(depth-xy[1])/5

    def points(line):
        return " ".join(f"{x:.1f},{y:.1f}" for x, y in map(svg_xy, line))

    palette = {"hospital": "#ed8f93", "police": "#b39bf2", "civic": "#e6b7eb",
               "shops": "#e0b88e", "industry": "#e2a773", "highrise": "#f1f4de",
               "repeater": "#d990f3", "radio": "#d990f3", "transmitter": "#d990f3", "substation": "#64e2c1"}
    labels = {"crater-town", "winter-village", "north-industry", "hill-town", "city-hospital",
              "city-police", "city-towers", "west-factory", "university", "airport", "south-base",
              "south-services", "warehouses", "west-entertainment", "east-clinic", "farms", "western-estates", "east-factory"}
    lines = ['<svg xmlns="http://www.w3.org/2000/svg" width="1064" height="982" viewBox="-32 0 1064 982">',
             '<rect x="-32" width="1064" height="982" fill="#131b20"/>',
             '<text x="0" y="39" fill="#edf1e5" font-family="sans-serif" font-size="26" font-weight="bold">Crossroads region</text>',
             '<defs><linearGradient id="region" x2="0%" y2="100%"><stop stop-color="#3b5461"/>'
             '<stop offset=".51" stop-color="#425046"/><stop offset="1" stop-color="#655344"/></linearGradient></defs>',
             f'<rect y="74" width="{map_w}" height="{map_h}" fill="url(#region)"/>']
    for x in range(0, width+1, 500):
        sx = x/5
        lines.append(f'<path d="M{sx} 74v800" stroke="#d8eddf" opacity=".14"/>')
    for y in range(0, depth+1, 500):
        _, sy = svg_xy([0, y])
        lines.append(f'<path d="M0 {sy}h1000" stroke="#d8eddf" opacity=".14"/>')
    for hazard in plan["radiation_corridors"]:
        left, bottom, right, top = hazard["bounds_m"]
        x, y = svg_xy([left, top])
        lines.append(f'<rect x="{x}" y="{y}" width="{(right-left)/5}" height="{(top-bottom)/5}" fill="#a27b56"/>')
    for river in plan["rivers"]:
        lines.append(f'<polyline points="{points(river["points_m"])}" fill="none" stroke="#83c9e4" stroke-linecap="round" stroke-linejoin="round" stroke-width="{river["width_m"]/5}"/>')
    for lake in plan["lakes"]:
        x, y = svg_xy(lake["xy_m"])
        rx, ry = lake["radii_m"]
        lines.append(f'<ellipse cx="{x}" cy="{y}" rx="{rx/5}" ry="{ry/5}" fill="#76b2cf"/>')
        lines.append(f'<text x="{x}" y="{y+4}" text-anchor="middle" fill="#112b38" font-family="sans-serif" font-size="12">{lake["id"]}</text>')
    for fault in ("fissure_m", "depot_fissure_m"):
        stroke = 5 if fault == "fissure_m" else 3
        lines.append(f'<polyline points="{points(plan[fault])}" fill="none" stroke="#e09a81" stroke-width="{stroke}"/>')
    for hill in plan["hills"]:
        x,y = svg_xy(hill["xy_m"])
        lines.append(f'<path d="M{x-5:.1f},{y:.1f} q5,-9 10,0" fill="none" stroke="#a6c87e" stroke-width="1.6"/>')
    for road in plan["roads"]:
        color = "#f5d280" if road["id"] == "east-west-highway" else "#c4c3ad"
        lines.append(f'<polyline points="{points(road["points_m"])}" fill="none" stroke="{color}" stroke-width="{max(2, road["width_m"]/6)}"/>')
    for route in plan["m4_route"]:
        if not route["id"].startswith("downtown-") and not route["id"].endswith("_access"):
            continue
        runs = []
        for segment in route["segments"]:
            if not runs or runs[-1][-1] != segment["start"]:
                runs.append([segment["start"]])
            runs[-1].append(segment["end"])
        color = "#ed956d" if "overpass" in route["id"] else "#ebf78b"
        for line in runs:
            lines.append(f'<polyline points="{points(line)}" fill="none" stroke="{color}" stroke-width="2.8" stroke-linejoin="round"/>')
    for track in plan["rail"]["tracks"]:
        line = [track["segments"][0]["start"], *(s["end"] for s in track["segments"])]
        color = "#b987fd" if track["kind"] == "freight" else "#ff91c6"
        lines.append(f'<polyline points="{points(line)}" fill="none" stroke="{color}" stroke-width="2.15" stroke-linecap="round" stroke-linejoin="round"/>')
    for stop in plan["rail"]["stops"]:
        x,y = svg_xy(stop["position"][:2])
        lines.append(f'<circle cx="{x}" cy="{y}" r="3" fill="#341c34" stroke="#ff91c6"><title>{html.escape(stop["label"])}</title></circle>')
    for lot in plan["city_lots"]:
        x, y = svg_xy((lot["x"], lot["y"]))
        lines.append(f'<rect x="{x-1.6:.1f}" y="{y-1.6:.1f}" width="3.2" height="3.2" fill="#f0e2c3"/>')
    for a, b in plan["grid"]["spans"]:
        poles = plan["grid"]["poles"]
        x, y = svg_xy((poles[a]["x"],poles[a]["y"]))
        u, v = svg_xy((poles[b]["x"],poles[b]["y"]))
        lines.append(f'<path d="M{x:.1f} {y:.1f}L{u:.1f} {v:.1f}" stroke="#64e2c1" stroke-width="1.25"/>')
    for bridge in plan["bridge_candidates"]:
        x, y = svg_xy(bridge["xy_m"])
        lines.append(f'<rect x="{x-3}" y="{y-3}" width="6" height="6" fill="#17242a" stroke="#fff3d4"/>')
    for checkpoint in plan["checkpoints"]:
        x, y = svg_xy(checkpoint["xyz_m"])
        lines.append(f'<path d="M{x-4:.1f},{y:.1f}h8m-4,-4v8" stroke="#fab376" stroke-width="1.6"/>')
    for site in [*plan["sites"], *plan["communications"], *plan["substations"]]:
        x, y = svg_xy(site.get("plot_xy_m", site["xy_m"]))
        color = palette.get(site["kind"], "#d8dfc1")
        lines.append(f'<circle cx="{x}" cy="{y}" r="3.2" fill="{color}" stroke="#1b2524"/>')
        if site["id"] in labels or site["kind"] in {"repeater", "radio", "transmitter", "substation"}:
            label = html.escape(site["name"])
            align = "end" if x>900 or site["kind"] == "radio" else "start"
            offset = -6 if align == "end" else 6
            lines.append(f'<text x="{x+offset}" y="{y-5}" text-anchor="{align}" fill="#f7faed" stroke="#24312f" stroke-width="2" paint-order="stroke" font-family="sans-serif" font-size="12">{label}</text>')
    lines += ['<g fill="#e1e8dc" font-family="sans-serif" font-size="14">',
              '<text x="925" y="39">N ↑</text><text x="0" y="907">20 km²  ·  5 km east–west  ·  4 km north–south  ·  Teal transmission lines</text>',
              '<text x="0" y="936">Gold highway  ·  Grey regional roads  ·  Squares mark candidate bridges  ·  Salmon marks fissures</text>',
              '<text x="0" y="959">Lime approaches  ·  Orange elevated decks  ·  Purple freight rail  ·  Pink tram and stops</text></g></svg>']
    path.write_text("\n".join(lines)+"\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="JSON manifest destination")
    parser.add_argument("--seed", type=int, default=1729)
    parser.add_argument("--bake", action="store_true", help="Also write terrain PNG and overview SVG next to the manifest")
    args = parser.parse_args()
    data, terrain = generate(args.seed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2)+"\n", encoding="utf-8")
    if args.bake:
        write_heightmap(args.output.with_name("crossroads-height.png"), data, terrain)
        write_map(args.output.with_name("crossroads-region.svg"), data)
