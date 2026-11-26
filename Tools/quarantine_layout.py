"""Author downtown boundary intervals in map metres; breaks are gameplay passages."""
import math

WEST, EAST, SOUTH, NORTH = 1770, 2950, 1650, 2700
SIDES = ((WEST,SOUTH,0,1,NORTH-SOUTH,-1,0),(EAST,SOUTH,0,1,NORTH-SOUTH,1,0),
         (WEST,SOUTH,1,0,EAST-WEST,0,-1),(WEST,NORTH,1,0,EAST-WEST,0,1))


def near_boundary(x,y,r):
    return any(-r <= (x-ox)*dx+(y-oy)*dy <= length+r and
               abs((x-ox)*nx+(y-oy)*ny)<=r for ox,oy,dx,dy,length,nx,ny in SIDES)


def perimeter(plan,height):
    sections, hazards = [], []
    roads = [(s["start"],s["end"],r.get("width_m",5)) for r in plan["m4_route"]+plan["rail"]["tracks"]
             for s in r["segments"] if near_boundary(*s["start"][:2],65)]
    def surface(x,y):
        z=height(x,y)
        for a,b,w in roads:
            vx,vy=b[0]-a[0],b[1]-a[1]
            t=max(0,min(1,((x-a[0])*vx+(y-a[1])*vy)/max(.001,vx*vx+vy*vy)))
            if math.hypot(x-a[0]-vx*t,y-a[1]-vy*t)<w/2+2:
                z=max(z,a[2]+(b[2]-a[2])*t)
        return z
    for side,(ox,oy,dx,dy,length,nx,ny) in enumerate(SIDES):
        gate=401 if side==1 else None
        if side==0:
            gate=next(a[1]+(WEST-a[0])*(b[1]-a[1])/(b[0]-a[0])-SOUTH
                      for road in plan["roads"] if road["id"]=="east-west-highway"
                      for a,b in zip(road["points_m"],road["points_m"][1:]) if a[0]<=WEST<=b[0])
        holes=[(length*.28,4.4,"breach")] if side>=2 else [(length*.7,1.6,"passage")]
        if gate is not None:holes.append((gate,16,"gate"))
        cursor=0
        for center,width,kind in sorted(holes+[(length,0,"end")]):
            stop=center-width/2
            count=math.ceil((stop-cursor)/12)
            for j in range(count):
                span=(stop-cursor)/count;t=cursor+(j+.5)*span
                x,y=ox+dx*t,oy+dy*t
                ends=[surface(x+dx*u,y+dy*u) for u in (-span/2,0,span/2)]
                base=min(height(x+dx*u,y+dy*u) for u in (-span/2,0,span/2))-1.4
                sections.append((x,y,base,max(ends)+4.4,span,dx,nx,ny,"wall"))
                if j%4==2 and (gate is None or abs(t-gate)>45):
                    hx,hy=x+nx*6,y+ny*6
                    hazards.append((hx,hy,surface(hx,hy)))
            if kind=="gate":
                x,y=ox+dx*center,oy+dy*center
                sections.append((x,y,height(x,y)-1,surface(x,y)+4.2,width,dx,nx,ny,"gate"))
            cursor=center+width/2
    gates=[s[:2] for s in sections if s[-1]=="gate"]
    road_markers=[]
    for route in plan["m4_route"]:
        if route["id"].endswith("_access"):
            continue
        for seg in route["segments"]:
            a,b=seg["start"],seg["end"]
            x,y,z=((u+v)/2 for u,v in zip(a,b))
            outside=not (WEST<=x<=EAST and SOUTH<=y<=NORTH)
            if (outside and not seg.get("bridge") and near_boundary(x,y,22)
                    and all(math.hypot(x-gx,y-gy)>60 for gx,gy in gates)
                    and all(math.hypot(x-px,y-py)>85 for px,py,_ in road_markers)):
                road_markers.append((x,y,z))
                break
        if len(road_markers)==3:
            break
    return dict(sections=sections,hazards=hazards+road_markers,road_hazards=road_markers)
