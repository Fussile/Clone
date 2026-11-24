"""Place platforms, supports and sparse rolling stock around the baked rail corridors."""
import math


def stamp_platforms(stops, box, sign, material):
    for stop in stops:
        x,y,z = stop["position"]
        yaw = stop["yaw"]
        side = stop["side"]
        ang = math.radians(yaw)
        c,s = math.cos(ang),math.sin(ang)
        top = z+.65
        lower = stop["foot_z"]
        key = "CrossroadsRail_stop_"+stop["id"]

        def point(u,v):
            return x+u*c-v*s*side,y-u*s-v*c*side

        def piece(tag,label,u,v,z,w,d,h,finish,pitch=0,heading=0,collision=True):
            box(key+"_"+tag,label,*point(u,v),z,w,d,h,material=material[finish],
                rotation=(pitch,yaw+heading),collision=collision)

        # Terminal platforms extend back along the route, away from its buffers.
        offset = 8 if stop["id"]=="school" else -8 if stop["id"]=="university" else 0
        piece("deck","Tram platform",offset,3.5,top-.42,23,3,.42,"concrete")
        piece("edge","Platform warning line",offset,2.04,top+.008,23,.12,.012,"traffic",collision=False)
        for u in (offset-7,offset+7):
            for v in (2.56,4.55):
                tag=f"shelter_post_{u}_{v}"
                piece(tag,"Station shelter post",u,v,top,.11,.11,2.35,"dark_trim")
        piece("shelter_roof","Tram stop canopy",offset,3.55,top+2.35,15,2.45,.16,"dark_trim")
        px,py = point(offset,4.86)
        sign(key+"_sign",stop["label"].upper(),px,py,top+2.1,size=32,yaw=yaw+90)

        rise = top-lower
        if rise <= 1.35:
            run=7.1
            angle=math.atan2(rise,run)
            piece("path","Tram access ramp",0,5+run/2,(top+lower)/2-.20,
                  math.hypot(run,rise),2.5,.36,"concrete",pitch=-math.degrees(angle),heading=90*side)
        else:
            flights = math.ceil(rise/2.85)
            # Parallel flights rise from the outside, with a landing at each reversal.
            span = 10.6
            for i in range(flights):
                direction = 1 if i%2==0 else -1
                v = 6+(flights-1-i)*2.45
                lower_z = lower+i*rise/flights
                upper_z = lower+(i+1)*rise/flights
                pitch = math.degrees(math.atan2(upper_z-lower_z,span))
                piece(f"flight_{i}","Station stair ramp",direction*.065,v,(lower_z+upper_z)/2-.24,
                      math.hypot(span,upper_z-lower_z),2.38,.42,"concrete",
                      pitch=pitch,heading=0 if direction==1 else 180)
                if i < flights-1:
                    piece(f"landing_{i}","Station stair landing",direction*(span/2+.83),v-1.225,
                          upper_z-.28,1.72,4.86,.28,"concrete")
                else:
                    piece("upper_landing","Platform access landing",direction*(span/2+.80),5.05,
                          top-.28,1.7,3.48,.28,"concrete")


def stamp_infrastructure(rail, box, sign, palette):
    for track in rail["tracks"]:
        key = "CrossroadsRail_"+track["id"]
        for index,support in enumerate(track["supports"]):
            x,y,z = support["at"]
            base=support["base"]
            box(f"{key}_pier_{index}","Rail viaduct pier",x,y,base,1.8,1.65,z-base-.65,
                material=palette["concrete"])
            box(f"{key}_cap_{index}","Rail viaduct pier cap",x,y,z-.73,2.0,3.8,.3,
                material=palette["concrete"])
        for side,segment in enumerate((track["segments"][0],track["segments"][-1])):
            end=segment["end" if side else "start"]
            other=segment["start" if side else "end"]
            # Shared turnout endpoints stay open.
            if any(math.dist(end[:2],near[:2])<.01 for line in rail["tracks"] if line is not track
                   for near in (line["segments"][0]["start"],*(s["end"] for s in line["segments"]))):
                continue
            yaw=math.degrees(math.atan2(other[1]-end[1],end[0]-other[0]))
            box(f"{key}_buffer_{side}","Rail buffer stop",*end,1.0,1.9,.85,
                material=palette["dark_trim"],rotation=(0,yaw))

    stamp_platforms(rail["stops"],box,sign,palette)
