"""Bake the first connected scavenging route into the region manifest."""
import math


def bake_route(plan, ground_height, terrain_height):
    """Follow the southern arterial, with spurs to the warehouse and hospital pads."""
    plots = {site["id"]: site for site in plan["sites"] if "plot_xy_m" in site}

    def approach(site_id, direction):
        site = plots[site_id]
        x, y = site["plot_xy_m"]
        z = site["foundation_z_m"] + .2
        return (x, y + direction * site["footprint_m"][1]/2, z)

    # Grade each leg from the local terrain, then carry that grade across water.
    # The shared junction and adjoining legs use the same sampled terrain height.
    junction = (2354, 1292, None)
    routes = {
        "station": [approach("south-services", -1), (2580, 838, None), (2460, 915, None), junction],
        "warehouse": [junction, (2681, 1315, None), (2819, 1168, None), (2854, 1170, None),
                      approach("warehouses", -1)],
        "hospital": [junction, (2341, 1715, None), (2371, 2028, None), (2371, 2200, None),
                     (2293, 2456, None), (2250, 2535, None), approach("city-hospital", -1)],
    }
    road_width = 7
    baked = []
    for name, route in routes.items():
        samples = []
        for start, end in zip(route, route[1:]):
            dx, dy = end[0]-start[0], end[1]-start[1]
            distance = math.hypot(dx, dy)
            divisions = math.ceil(distance/10)
            for i in range(divisions):
                t = i/divisions
                x, y = start[0]+t*dx, start[1]+t*dy
                samples.append((x, y, start[2] if i == 0 else None))
        samples.append(route[-1])
        grade = [z if z is not None else max(ground_height(x, y), terrain_height(x, y)) + .12
                 for x, y, z in samples]
        segments = []
        for i, (start, end) in enumerate(zip(samples, samples[1:])):
            a, b = (*start[:2], grade[i]), (*end[:2], grade[i+1])
            x, y, z = ((v+w)/2 for v,w in zip(a,b))
            bridge = z-terrain_height(x,y) > 1.8
            segments.append({
                "start": [round(value, 3) for value in a],
                "end": [round(value, 3) for value in b],
                "bridge": bridge,
            })
        baked.append({"id": name, "width_m": road_width, "segments": segments})
    return baked
