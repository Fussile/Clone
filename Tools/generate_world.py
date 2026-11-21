"""Bake the authored regional layout, an Unreal heightmap, and a planning map."""
import argparse
import html
import json
import math
from pathlib import Path
import struct
import zlib

from region_route import bake_route

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
    t = max(0, min(1, ((x - start[0]) * dx + (y - start[1]) * dy) / (dx*dx + dy*dy)))
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
    fissure_segments = list(segments(plan["fissure_m"]))
    for line in [*plan["rivers"], *plan["roads"]]:
        assert all(0 <= x <= width and 0 <= y <= depth for x, y in line["points_m"]), line["id"]

    def ground_height(x, y):
        # Broad hills keep the city/delta low. The seed only varies relief.
        hills = 8 + 8 * math.sin((x + seed % 731) / 420) * math.cos(y / 480)
        edge = max(0, 1-min(x, width-x)/440, 1-(depth-y)/480)
        return hills + (125 + 35*math.cos(x/180 - y/260)) * edge**2

    lake_shapes = [(lake["xy_m"], lake["radii_m"]) for lake in plan["lakes"]]
    def water_distance(x, y):
        lake_edge = min((math.hypot((x-cx)/rx, (y-cy)/ry)-1) * min(rx, ry)
                        for (cx, cy), (rx, ry) in lake_shapes)
        river_edge = min(segment_distance(x, y, a, b) - w/2 for a, b, w in river_segments)
        return min(lake_edge, river_edge)

    plots = []
    road_segments = [(a, b, road["width_m"]) for road in plan["roads"] for a, b in segments(road["points_m"])]
    for site in plan["sites"]:
        if site["kind"] not in FOOTPRINTS:
            continue
        w, d = FOOTPRINTS[site["kind"]]
        clearance = math.hypot(w, d)/2 + 5
        x, y = site["xy_m"]

        def available(px, py):
            return (clearance < min(px, width-px, py, depth-py)
                    and water_distance(px, py) > clearance + 12
                    and all(segment_distance(px, py, a, b) > clearance + rw/2 for a, b, rw in road_segments)
                    and all(segment_distance(px, py, a, b) > clearance + 34 for a, b in fissure_segments)
                    and all(math.hypot(px-sx, py-sy) > clearance + sr for sx, sy, sr, _ in plots))

        for radius in range(0, 301, 10):
            candidates = [(round(x + radius*math.cos(i*math.tau/16)),
                           round(y + radius*math.sin(i*math.tau/16))) for i in range(16 if radius else 1)]
            position = next(((px, py) for px, py in candidates if available(px, py)), None)
            if position is not None:
                break
        if position is None:
            raise ValueError(f"No dry buildable plot near {site['id']}")
        px, py = position
        z = round(ground_height(px, py), 2)
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
    plan.update(version=5, seed=seed, bridge_candidates=bridges, stream_cell_m=256, stream_radius_m=768,
                climate_celsius_offset={"north_edge": -15, "city": 0, "south_edge": 14},
                terrain={"resolution": RESOLUTION, "xyz_scale_cm": [width*100/(RESOLUTION-1),
                                                                  depth*100/(RESOLUTION-1), 100]})

    def height(x, y):
        base = ground_height(x, y)
        channel = water_distance(x, y)
        # Reference water level is 0 m. Keep beds below it, blending to land at the banks.
        bank = max(0., min(1., (channel + 4)/16))
        base = -4*(1-bank) + base*bank
        gap = min(segment_distance(x, y, a, b) for a,b in fissure_segments)
        base -= 44 * max(0., 1-gap/34)**2
        for px, py, clearance, z in plots:
            distance = math.hypot(x-px, y-py)
            blend = max(0., min(1., (clearance+28-distance)/22))
            base += (z-base)*blend
        return base

    plan["m4_route"] = bake_route(plan, ground_height, height)

    # Assign static content to planned cells. IDs persist when exact coordinates change.
    cells = {}
    for site in plan["sites"]:
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
               "shops": "#e0b88e", "industry": "#e2a773", "highrise": "#f1f4de"}
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
    lines.append(f'<polyline points="{points(plan["fissure_m"])}" fill="none" stroke="#e09a81" stroke-width="5"/>')
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
    for bridge in plan["bridge_candidates"]:
        x, y = svg_xy(bridge["xy_m"])
        lines.append(f'<rect x="{x-3}" y="{y-3}" width="6" height="6" fill="#17242a" stroke="#fff3d4"/>')
    for site in plan["sites"]:
        x, y = svg_xy(site.get("plot_xy_m", site["xy_m"]))
        color = palette.get(site["kind"], "#d8dfc1")
        lines.append(f'<circle cx="{x}" cy="{y}" r="3.2" fill="{color}" stroke="#1b2524"/>')
        if site["id"] in labels:
            label = html.escape(site["name"])
            lines.append(f'<text x="{x+6}" y="{y-5}" fill="#f7faed" stroke="#24312f" stroke-width="2" paint-order="stroke" font-family="sans-serif" font-size="12">{label}</text>')
    lines += ['<g fill="#e1e8dc" font-family="sans-serif" font-size="14">',
              '<text x="925" y="39">N ↑</text><text x="0" y="907">20 km²  ·  5 km east–west  ·  4 km north–south</text>',
              '<text x="0" y="936">Gold highway  ·  Grey proposed roads  ·  Squares mark candidate bridges  ·  Salmon marks the Great Fissure</text>',
              '<text x="0" y="959">Lime marks the city loop and site approaches. Orange marks the elevated decks.</text></g></svg>']
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
