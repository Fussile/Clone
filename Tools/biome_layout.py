"""Regional ground cover and repeatable foliage placements in Crossroads map metres."""
from array import array
from collections import defaultdict
import math
import random
import struct
import sys
import zlib


def smooth(low, high, value):
    t = max(0., min(1., (value-low)/(high-low)))
    return t*t*(3.-2.*t)


class Surface:
    """Sample the same quantized surface that Unreal imports, including graded roads and pads."""
    def __init__(self, path, plan):
        self.width, self.depth = plan["size_m"]
        self.n = plan["terrain"]["resolution"]
        self.samples = array("H")
        self.samples.frombytes(path.read_bytes())
        if sys.byteorder != "little":
            self.samples.byteswap()
        if len(self.samples) != self.n**2:
            raise ValueError("Incomplete Crossroads terrain samples")

    def __call__(self, x, y):
        u = max(0.,min(self.n-1., x/self.width*(self.n-1)))
        v = max(0.,min(self.n-1., (1-y/self.depth)*(self.n-1)))
        i, j = min(int(u),self.n-2), min(int(v),self.n-2)
        k = j*self.n+i
        a,b = self.samples[k:k+2]
        c,d = self.samples[k+self.n:k+self.n+2]
        top = a+(b-a)*(u-i)
        return (top+(c+(d-c)*(u-i)-top)*(v-j)-32768)/128.


class Cover:
    def __init__(self, plan):
        self.cx,self.cy = plan["downtown_xy_m"]
        winter = next(s for s in plan["sites"] if s["id"]=="winter-village")
        self.wx,self.wy = winter["plot_xy_m"]

    def weights(self, x, y, z):
        # Each channel is a continuous weight. The material and plants read the same transitions.
        ripple = 85*math.sin(x/274+y/610)+44*math.cos(y/193-x/420)
        urban = 1-smooth(.68,1.25,math.hypot((x-self.cx)/790,(y-self.cy)/610))
        dry = 1-smooth(570,1580,y+ripple)
        winter = 1-smooth(130,570,math.hypot(x-self.wx,y-self.wy))
        snow = max(winter*.90, smooth(4220,4830,y+ripple)*(0.58+.42*smooth(110,158,z)),
                   smooth(155,222,z)*smooth(3400,4450,y))
        grove = math.sin(x/163+1.2*math.sin(y/290)) + .76*math.cos(y/227-x/310)+.32*math.sin(y/61+x/92)
        forest = smooth(-.12,.99,grove)*smooth(960,2110,y+ripple)*(1-urban)*(1-.70*snow)
        return dry,forest,snow,urban

    def write_mask(self, path, plan, height, size=512):
        rows=bytearray()
        w,d=plan["size_m"]
        for j in range(size):
            y=d*(1-(j+.5)/size)
            rows.append(0)
            for i in range(size):
                x=w*(i+.5)/size
                dry,forest,snow,_ = self.weights(x,y,height(x,y))
                grade=math.hypot(height(x+5,y)-height(x-5,y),height(x,y+5)-height(x,y-5))/10
                # Keep PNG alpha nonzero. Importers may rewrite RGB in fully transparent texels.
                rows.extend(round(v*255) for v in (dry,forest,snow))
                rows.append(32+round(223*smooth(.28,.82,grade)))
        def chunk(kind,data):
            return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data))
        path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",size,size,8,6,0,0,0))
                         +chunk(b"IDAT",zlib.compress(rows))+chunk(b"IEND",b""))


class Clearances:
    """Index reserved land in 64 m cells, including routes that extend beyond their host parcel."""
    def __init__(self, plan):
        self.cells=defaultdict(list)
        def reserve(x0,y0,x1,y1,test):
            for i in range(math.floor(x0/64),math.floor(x1/64)+1):
                for j in range(math.floor(y0/64),math.floor(y1/64)+1):
                    self.cells[i,j].append(test)

        def pad(x,y,w,d,margin=8,yaw=0):
            a=math.radians(-yaw)
            c,s=math.cos(a),math.sin(a)
            hw,hd=w/2+margin,d/2+margin
            ex,ey=abs(c)*hw+abs(s)*hd,abs(s)*hw+abs(c)*hd
            reserve(x-ex,y-ey,x+ex,y+ey,lambda px,py: abs((px-x)*c+(py-y)*s)<=hw
                    and abs((py-y)*c-(px-x)*s)<=hd)

        def corridor(a,b,r):
            x,y=a[:2];xx,yy=b[:2];dx,dy=xx-x,yy-y
            length=max(.001,dx*dx+dy*dy)
            def inside(px,py):
                t=max(0,min(1,((px-x)*dx+(py-y)*dy)/length))
                return (px-x-t*dx)**2+(py-y-t*dy)**2<=r*r
            reserve(min(x,xx)-r,min(y,yy)-r,max(x,xx)+r,max(y,yy)+r,inside)

        for site in [*plan["sites"],*plan["communications"],*plan["substations"]]:
            if "footprint_m" in site:
                pad(*site["plot_xy_m"],*site["footprint_m"],margin=12)
            approach=site.get("approach")
            if isinstance(approach,dict):
                for seg in approach.get("segments",[]):
                    corridor(seg["start"],seg["end"],approach["width_m"]/2+8)
        for lot in plan["city_lots"]:
            pad(lot["x"],lot["y"],lot["w"],lot["d"],yaw=lot["yaw"])
        for route in [*plan["m4_route"],*plan["rail"]["tracks"]]:
            for seg in route["segments"]:
                corridor(seg["start"],seg["end"],route["width_m"]/2+10)
        for river in plan["rivers"]:
            for a,b in zip(river["points_m"],river["points_m"][1:]):
                corridor(a,b,river["width_m"]/2+9)
        for lake in plan["lakes"]:
            x,y=lake["xy_m"];rx,ry=(r+10 for r in lake["radii_m"])
            reserve(x-rx,y-ry,x+rx,y+ry,lambda px,py,x=x,y=y,rx=rx,ry=ry:
                    ((px-x)/rx)**2+((py-y)/ry)**2<=1)
        for key,radius in (("fissure_m",50),("depot_fissure_m",39)):
            for a,b in zip(plan[key],plan[key][1:]):
                corridor(a,b,radius)
        for x,y,_,_,length,dx,_,_,_ in plan["quarantine"]["sections"]:
            pad(x,y,length if dx else 2,2 if dx else length,margin=15)
        for hazard in plan["quarantine"]["hazards"]:
            pad(*hazard[:2],2,2,margin=6)
        grid=plan["grid"]
        for a,b in grid["spans"]:
            ends=[(grid["poles"][k]["x"],grid["poles"][k]["y"]) for k in (a,b)]
            corridor(*ends,10)
        airport=plan["facilities"]["airport"]
        from airfield_layout import RUNWAY_HALF, RUNWAY_Y
        pad(airport["x"],airport["y"]+RUNWAY_Y/2-14,2*RUNWAY_HALF+60,abs(RUNWAY_Y)+100)
        for stop in plan["rail"]["stops"]:
            pad(*stop["position"][:2],36,24,margin=10,yaw=stop["yaw"])

    def allows(self,x,y):
        return not any(test(x,y) for test in self.cells.get((int(x//64),int(y//64)),()))


def scatter(plan,height,cover,clear):
    w,d=plan["size_m"]
    # Each candidate has a private seed so a blocked plot never shifts another district's planting.
    batches=defaultdict(list)
    for band,step in enumerate((10,11)):
        for i in range(math.ceil(w/step)):
            for j in range(math.ceil(d/step)):
                rng=random.Random((plan["seed"]<<32)+(band<<28)+(i<<14)+j)
                x,y=(i+.14+.72*rng.random())*step,(j+.14+.72*rng.random())*step
                if not (4<x<w-4 and 4<y<d-4) or not clear.allows(x,y):
                    continue
                z=height(x,y)
                dry,forest,snow,urban=cover.weights(x,y,z)
                grade=math.hypot(height(x+3,y)-height(x-3,y),height(x,y+3)-height(x,y-3))/6
                draw=rng.random()
                if band==0:
                    if grade<.55 and draw<.82*forest+.01*(1-urban)*(1-dry):
                        name=rng.choice(("BirchA","BirchB"))
                        size=rng.uniform(.62,1.26)
                    elif draw>1-(.025+.22*smooth(.2,.9,grade)+.016*dry)*(1-urban):
                        name="Boulder" if rng.random()<.37 else "Stones"
                        size=rng.uniform(.52,1.44)
                    else:
                        continue
                else:
                    if grade>.75 or snow>.85 or draw>(.20+.56*forest+.10*dry)*(1-.9*urban):
                        continue
                    name="Shrub" if dry>.42 or rng.random()<.18 else "Fern" if forest>.45 else "Weeds"
                    if forest>.38 and name in ("Fern","Shrub") and rng.random()<.18:
                        name="FirSapling"
                    size=rng.uniform(.7,1.5) if name!="FirSapling" else rng.uniform(1.0,2.1)
                batches[int(x//256),int(y//256),name].append((x,y,z,rng.uniform(-180,180),size))
    return batches


def forage(plan, height, cover, clear):
    """Keep one wood source and one mineral source per suitable patch of terrain."""
    w, d = plan["size_m"]
    spacing = 150
    for i in range(math.ceil(w/spacing)):
        for j in range(math.ceil(d/spacing)):
            for mineral in (False, True):
                seed = (plan["seed"] << 32) + (i << 16) + (j << 1) + int(mineral)
                rng = random.Random(seed)
                for attempt in range(24):
                    x = i*spacing + rng.uniform(5, min(spacing, w-i*spacing)-4)
                    y = j*spacing + rng.uniform(5, min(spacing, d-j*spacing)-4)
                    if not clear.allows(x,y):
                        continue
                    dz = math.hypot(height(x+2,y)-height(x-2,y), height(x,y+2)-height(x,y-2))
                    if dz > 1.15:
                        continue
                    z = height(x,y)
                    dry = cover.weights(x,y,z)[0]
                    name = "Geode" if mineral else ("Saguaro" if dry>.65 and rng.random()<.78 else "Deadwood")
                    yield i, j, name, x, y, z, rng.uniform(-180,180), rng.uniform(.83,1.2)
                    break
