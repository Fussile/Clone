"""Bake substation footprints and a continuous overhead transmission network."""
import math


def bake_grid(plan, sites, height, available):
    substations = {site["id"]: site for site in plan["substations"]}
    nodes = {}
    for key, site in substations.items():
        x, y = site["plot_xy_m"]
        nodes[key] = (x+9, y+7, site["floor_z_m"])
    for key in ("power-southwest", "power-northeast"):
        site = sites[key]
        x, y = site["plot_xy_m"]
        # Terminate on the plant apron, clear of the main generator hall.
        nodes[key] = (x+24, y+12, site["foundation_z_m"]+.2)

    bends_by_link = {
        ("power-northeast", "substation-4"): ((3520, 3550), (3480, 2790)),
        ("substation-5", "substation-3"): ((4170, 1500), (4140, 1150)),
    }
    poles = {}
    spans = []
    for a, b in plan["transmission"]:
        start, end = nodes[a], nodes[b]
        path = [a]
        # Skirt the west bank of the Great Lake instead of skipping dry pylon sites.
        bends = bends_by_link.get((a, b), ())
        waypoints = (start[:2], *bends, end[:2])
        for leg, (origin, target) in enumerate(zip(waypoints, waypoints[1:])):
            dx, dy = target[0]-origin[0], target[1]-origin[1]
            length = math.hypot(dx,dy)
            count = max(1, math.ceil(length/112))
            for i in range(1, count+(leg < len(waypoints)-2)):
                t = i/count
                x, y = origin[0]+t*dx, origin[1]+t*dy
                for offset in (0, 15, -15, 32, -32, 52, -52, 74, -74):
                    px, py = x-dy/length*offset, y+dx/length*offset
                    if available(px,py,3.2):
                        key = f"{a}_{b}_{leg}_{i}"
                        nodes[key] = (px,py,height(px,py)-.28)
                        path.append(key)
                        break
        path.append(b)
        for first, second in zip(path,path[1:]):
            spans.append((first, second))
            for p, q in ((first,second),(second,first)):
                poles.setdefault(p,[]).append(q)

    # Give every pylon fixed phase sockets, including where branches share it.
    towers = {}
    phase_heights = (9.49, 11.58, 11.58, 9.49)
    for key, adjacent in poles.items():
        x,y,z = nodes[key]
        vx,vy = (nodes[adjacent[0]][i]-nodes[key][i] for i in range(2))
        if vx<0 or (vx==0 and vy<0):
            vx,vy = -vx,-vy
        length = math.hypot(vx,vy)
        nx,ny = -vy/length,vx/length
        phases = [(x+nx*lateral, y+ny*lateral, z+rise*1.32)
                  for lateral, rise in zip((-5.16, -3.55, 3.55, 5.16), phase_heights)]
        towers[key] = dict(x=x,y=y,z=z,yaw=math.degrees(math.atan2(-ny,nx)),phases=phases)

    # Raise both supports where the hanging conductor would intersect rising ground.
    factors = dict.fromkeys(towers, 1.32)
    for a, b in spans:
        for start, end, rise in zip(towers[a]["phases"], towers[b]["phases"], phase_heights):
            sag = cable_sag(math.dist(start,end))
            for sample in range(1,32):
                t = sample/32
                x,y,z = (v+(w-v)*t for v,w in zip(start,end))
                need = (height(x,y)+7.0+4*t*(1-t)*sag-z)/rise
                factors[a] = max(factors[a], 1.32+need)
                factors[b] = max(factors[b], 1.32+need)
    for key, tower in towers.items():
        factor = factors[key]
        tower["height_m"] = 14.401*factor
        tower["phases"] = [(x,y,tower["z"]+rise*factor)
                           for (x,y,_), rise in zip(tower["phases"],phase_heights)]
    return {"poles": towers, "spans": spans}


def cable_sag(length):
    return min(length*.044, 6.0)


def stamp_substation(site, box, mesh, prop, cache, sign, concrete):
    key = "CrossroadsUtility_"+site["id"]
    x,y = site["plot_xy_m"]
    z,base = site["floor_z_m"],site["footing_z_m"]
    w,d = site["footprint_m"]
    box(key+"_pad","Substation concrete pad",x,y,base,w,d,z-base,ground=True,material=concrete)
    mesh(key+"_house","Soviet electrical substation building","SubstationHouse",
         x-8,y+3.8,z,(6.4,6.9,4.4))
    prop(key+"_switchgear","Electrical switchgear yard","SubstationSwitchgear",
         x+3,y-2.8,z,(16,12,4.2),health=480)
    prop(key+"_transformer","Substation transformer","YardTransformer",
         x-.5,y+6.8,z,(3.6,2.5,3.1),health=420)
    cache(key+"_parts","Utility parts crate",x-6.6,y-3,z,profile="BUILDING")
    sign(key+"_sign",site["name"].upper(),x-6,y-d/2+.35,z+1.45,size=28)
