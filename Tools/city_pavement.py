"""Build continuous, mitered pavement with shared edges and broken viaduct ends."""
import hashlib
import math
import struct


def is_city_route(route):
    ident = route["id"]
    return ident.startswith("downtown-") or ident == "east-west-highway" or ident in {
        "city-parking_access", "city-nightlife_access", "city-police_access",
    }


def pavement_chunks(route):
    runs = []
    for segment in route["segments"]:
        if not runs or runs[-1][-1]["end"] != segment["start"]:
            runs.append([])
        runs[-1].append(segment)

    for run in runs:
        points = [run[0]["start"], *(s["end"] for s in run)]
        width = route["width_m"]/2
        offsets = []
        distance = [0.0]
        for a,b in zip(points, points[1:]):
            length = math.dist(a[:2], b[:2])
            offsets.append(((a[1]-b[1])/length, (b[0]-a[0])/length))
            distance.append(distance[-1]+length)
        edges = []
        closed = points[0] == points[-1]
        for i,pt in enumerate(points):
            a = offsets[i-1] if i or closed else offsets[0]
            b = offsets[i] if i < len(offsets) else offsets[0 if closed else -1]
            divisor = max(.6, 1+a[0]*b[0]+a[1]*b[1])
            delta = tuple(width*(v+w)/divisor for v,w in zip(a,b))
            deck = ((i > 0 and run[i-1]["bridge"]) or (i < len(run) and run[i]["bridge"]))
            depth = .50 if deck else max(run[max(0,i-1)].get("fill",.09),
                                          run[min(i,len(run)-1)].get("fill",.09))
            edges.append([(pt[0]+side*delta[0], pt[1]+side*delta[1], pt[2])
                          for side in (-1, 1)] + [depth])
        # Break faces are extended into the missing span at irregular intervals.
        terminal = {}
        for i, broken in ((0, run[0].get("broken_start")), (len(run), run[-1].get("broken_end"))):
            if broken:
                q = points[1 if i == 0 else i-1]
                p = points[i]
                span = math.dist(p[:2],q[:2])
                terminal[i] = tuple((p[k]-q[k])/span for k in range(2))
        for begin in range(0,len(run),16):
            end = min(begin+16,len(run))
            yield points[begin], edges[begin:end+1], distance[begin:end+1], {
                i-begin: direction for i,direction in terminal.items() if begin <= i <= end
            }


def mesh_data(origin, edges, distance, breaks):
    points, uv, triangles = [], [], []
    across = (0, .16, .37, .56, .78, 1) if breaks else (0, 1)
    columns = len(across)
    for row, (a,b,depth) in enumerate(edges):
        for bottom in (False,True):
            for col,t in enumerate(across):
                x,y,z = (v+(w-v)*t for v,w in zip(a,b))
                if row in breaks:
                    dx,dy = breaks[row]
                    reach = (0,.75,.24,1.3,.28,.66)[col]
                    x += reach*dx
                    y += reach*dy
                    z -= (0,.21,.08,.42,.13,.28)[col]
                points.append(((x-origin[0])*100, (origin[1]-y)*100,
                               (z-origin[2]-(depth if bottom else 0))*100))
                uv.append((t*math.dist(a,b)/4, -distance[row]/4))

    def quad(a,b,c,d):
        triangles.extend((a,b,c,a,c,d))
    stride = 2*columns
    for row in range(len(edges)-1):
        a,b = row*stride,(row+1)*stride
        for col in range(columns-1):
            quad(a+col,b+col,b+col+1,a+col+1)
            quad(a+columns+col,a+columns+col+1,b+columns+col+1,b+columns+col)
        quad(a,a+columns,b+columns,b)
        r = columns-1
        quad(a+r,b+r,b+r+columns,a+r+columns)
    for row in breaks:
        start = row*stride
        for col in range(columns-1):
            a = start+col
            indices = (a,a+1,a+1+columns,a+columns)
            quad(*(indices if row == 0 else indices[::-1]))

    payload = b"".join(struct.pack("<fff", *point) for point in points)
    payload += b"".join(struct.pack("<ff", *coord) for coord in uv)
    payload += struct.pack(f"<{len(triangles)}H", *triangles)
    digest = hashlib.sha256(payload).hexdigest()[:16]
    return f"SM_CityPavement_{digest}", points, uv, triangles
