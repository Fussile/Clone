"""Grade the freight and tram alignments against the regional road and terrain bake."""
import math
from collections import defaultdict

# The purple alignments follow the west highway corridor, then the south river.
FREIGHT = {
    "freight-main": ((235,1910),(400,1930),(650,1925),(1040,1930),(1495,1944),
                     (1770,1937),(2060,1915),(2270,1850),(2420,1730),(2540,1650),
                     (2730,1515),(2850,1320),(2940,1130),(3090,800),(3180,515),(3290,275)),
    "freight-farms": ((650,1925),(650,2070),(590,2165),(550,2312)),
    # The short purple marks in the north are an abandoned service reach.
    "freight-north-remnant": ((1095,3280),(1115,3380),(1115,3480),(1095,3615)),
    "freight-west-siding": ((1495,1944),(1428,1913),(1305,1884),(1236,1884)),
    "freight-south-spur": ((2420,1730),(2390,1510),(2392,1220),(2370,970),
                           (2310,720),(2120,500),(1900,370)),
    "freight-warehouse-siding": ((2850,1320),(2823,1260),(2848,1187),(2854,1107),(2940,1130)),
    "freight-depot": ((3180,515),(3300,439),(3455,350),(3640,360),(3820,435),(4045,485)),
}
TRAM = {
    "tram-main": ((2570,3277),(2595,3135),(2640,2990),(2670,2840),(2710,2650),
                  (2676,2452),(2615,2375),(2520,2290),(2498,2245),(2490,2175),(2471,2051),
                  (2520,1935),(2640,1895),(2830,1842),(2997,1830),(3250,1821),
                  (3500,1800),(3715,1843),(3838,1848)),
}
STOPS = (
    ("school", "North School", (2570,3277)),
    ("middle-suburbs", "Middle-Class Suburbs", (2676,2452)),
    ("downtown", "Crossroads Central", (2490,2175)),
    ("south-suburbs", "Low-Income Suburbs", (2997,1830)),
    ("university", "University", (3838,1848)),
)


def project(p, a, b):
    dx,dy = b[0]-a[0], b[1]-a[1]
    den = dx*dx+dy*dy
    t = max(0, min(1, ((p[0]-a[0])*dx+(p[1]-a[1])*dy)/den)) if den else 0
    return math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy), t


def sample_line(controls):
    # Interpolating Hermite curves preserve the named turnout and station locations.
    # Each turnout endpoint is straightened by the neighboring control tangent.
    edges = list(zip(controls,controls[1:]))
    out = []
    for i, (a,b) in enumerate(edges):
        prev = controls[max(0,i-1)]
        following = controls[min(i+2,len(controls)-1)]
        span = math.dist(a,b)
        tangent_a = [(b[k]-prev[k])*span/math.dist(prev,b) for k in range(2)]
        tangent_b = [(following[k]-a[k])*span/math.dist(a,following) for k in range(2)]
        steps = math.ceil(span/4)
        for n in range(steps):
            t=n/steps
            out.append(tuple((2*t**3-3*t*t+1)*a[k]+(t**3-2*t*t+t)*tangent_a[k]
                             +(-2*t**3+3*t*t)*b[k]+(t**3-t*t)*tangent_b[k] for k in range(2)))
    return [*out,controls[-1]]


def bake_rail(roads, ground, height, water_distance):
    road_cells = defaultdict(list)
    for road in roads:
        half = road["width_m"]/2
        for segment in road["segments"]:
            a,b = segment["start"],segment["end"]
            pad = half+4.5
            for x in range(math.floor((min(a[0],b[0])-pad)/64),math.floor((max(a[0],b[0])+pad)/64)+1):
                for y in range(math.floor((min(a[1],b[1])-pad)/64),math.floor((max(a[1],b[1])+pad)/64)+1):
                    road_cells[x,y].append((a,b,half,road["id"]))

    def constraints(point, rail_width):
        targets = {}
        for a,b,half,ident in road_cells[math.floor(point[0]/64),math.floor(point[1]/64)]:
            distance,t = project(point,a,b)
            if distance < half+rail_width/2+2.1:
                level = a[2]+(b[2]-a[2])*t
                targets[ident] = max(level,targets.get(ident,-math.inf))
        # Keep a clean opening under the rail deck or under an already taller road bridge.
        return targets

    lines = {}
    routes = {**FREIGHT,**TRAM}
    for name, controls in routes.items():
        freight = name in FREIGHT
        width = 4.4 if freight else 3.9
        max_grade = .035 if freight else .077
        pts = sample_line(controls)
        grades, ground_z, crosses = [], [], []
        for point in pts:
            terrain = height(*point)
            wet = water_distance(*point)<5
            base = max(terrain,ground(*point)+1 if wet else terrain)+.11
            nearby = constraints(point,width)
            # Freight and tram share the same reliable clearance budget.
            over = {key:value for key,value in nearby.items() if value-base<5.7}
            grades.append(max([base,*(road+6.45 for road in over.values())]))
            ground_z.append(terrain)
            crosses.append(nearby)
        lines[name]=(pts,grades,ground_z,crosses,max_grade,width)

    junctions = {point for points in routes.values() for point in (points[0],points[-1])
                 if sum(point in nodes for nodes in routes.values())>1}
    members = {p:[(data[1],i) for data in lines.values() for i,q in enumerate(data[0]) if math.dist(p,q)<.001]
               for p in junctions}
    tram_pts,tram_grades,_,_,_,_ = lines["tram-main"]
    level_stops = [[i for i,p in enumerate(tram_pts) if math.dist(p,point)<=11]
                   for _,_,point in STOPS]
    # Constrain ramps in both directions and share the exact deck height at turnouts.
    for _ in range(len(lines)+len(level_stops)+1):
        for indices in level_stops:
            level=max(tram_grades[i] for i in indices)
            for i in indices:
                tram_grades[i]=level
        for pts,grades,_,crossings,_,_ in lines.values():
            for i,targets in enumerate(crossings):
                for road in targets.values():
                    if road-5.7 < grades[i] < road+6.45:
                        grades[i] = road+6.45
        for grades in members.values():
            level = max(g[i] for g,i in grades)
            for g,i in grades:
                g[i]=level
        for pts,grades,_,_,limit,_ in lines.values():
            for indices in (range(1,len(pts)),range(len(pts)-2,-1,-1)):
                direction = 1 if indices.step>0 else -1
                for i in indices:
                    j=i-direction
                    grades[i]=max(grades[i],grades[j]-limit*math.dist(pts[i],pts[j]))

    tracks = []
    for name,(pts,grades,terrain,crosses,_,width) in lines.items():
        nodes = [[round(v,3) for v in (*p,z)] for p,z in zip(pts,grades)]
        sections = []
        supports=[]
        pillars = 0
        walked = 0
        for i,(a,b) in enumerate(zip(nodes,nodes[1:])):
            bridge = min(grades[i]-terrain[i],grades[i+1]-terrain[i+1])>.85 or bool(crosses[i] or crosses[i+1])
            segment = dict(start=a,end=b,bridge=bridge)
            if not bridge:
                segment["fill"] = round(max(.09, grades[i]-terrain[i]+.11, grades[i+1]-terrain[i+1]+.11), 3)
            sections.append(segment)
            if i==0:
                continue
            walked+=math.dist(a[:2],b[:2])
            if (walked-pillars>=25 and grades[i]-terrain[i]>1.05 and not crosses[i]
                and not any(crosses[max(0,i-2):i+3]) and water_distance(*pts[i])>3):
                supports.append(dict(at=a,base=round(terrain[i]-.18,3)))
                pillars=walked
        tracks.append(dict(id=name,kind="freight" if name in FREIGHT else "tram",width_m=width,
                           segments=sections,supports=supports))

    def mark(route, point, name, label):
        pts,grades,_,_,_,_ = lines[route]
        i=min(range(len(pts)),key=lambda index: math.dist(pts[index],point))
        a,b=pts[max(0,i-1)],pts[min(i+1,len(pts)-1)]
        yaw=math.degrees(math.atan2(a[1]-b[1], b[0]-a[0]))
        j,k = max(0,i-1),min(i+1,len(pts)-1)
        pitch=math.degrees(math.atan2(grades[k]-grades[j],math.dist(a,b)))
        return dict(id=name,label=label,position=[*pts[i],grades[i]],yaw=yaw,pitch=pitch,
                    kind="freight" if route in FREIGHT else "tram")
    stops = [mark("tram-main", pos, ident, label) for ident,label,pos in STOPS]
    for stop in stops:
        x,y,z=stop["position"]
        angle=math.radians(stop["yaw"])
        options=[]
        for side in (-1,1):
            dx,dy = -math.sin(angle)*side, -math.cos(angle)*side
            foot = (x+dx*8,y+dy*8)
            road = constraints(foot,0)
            cost = (100 if road else 0)+abs(z-height(*foot))
            options.append((cost,side,height(*foot)+.15))
        if stop["id"] == "university":
            options = [option for option in options if option[1] == 1]
        _,stop["side"],stop["foot_z"]=min(options)

    rolling_stock = [
        mark("freight-west-siding",(1320,1889),"west-factory-engine","Soviet freight locomotive"),
        mark("freight-warehouse-siding",(2840,1190),"warehouse-engine","Soviet freight locomotive"),
        mark("freight-depot",(4000,475),"depot-engine","Soviet freight locomotive"),
        mark("tram-main",(2490,2175),"central-tram","Abandoned city tram"),
        mark("tram-main",(3745,1811),"university-tram","Abandoned city tram"),
    ]
    return dict(tracks=tracks,stops=stops,rolling_stock=rolling_stock)
