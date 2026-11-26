"""Enterable northern commercial and school buildings, laid out in map metres."""


def regional_mall(key, x, y, z, shell, double_door, door, box, display, sign, furnishing, cache, materials):
    brick, tile = materials["StationBrick"], materials["ShopTile"]
    front = shell(box, key, x, y+8, z, 116, 64, 7.6, 4, brick, materials["concrete"])
    double_door(door, key+"_entry", "Mall entrance", x, front, z, 4, 2.35, "petrol")
    sign(key+"_name", "CROSSROADS GALLERIA", x, front-.28, z+5, size=65)
    box(key+"_tile", "Mall concourse tile", x, y+8, z, 115.4, 63.4, .03, material=tile, ground=True)
    box(key+"_approach", "Galleria entrance walk", x, (front+y-48)/2, z, 5, front-y+48, .08,
        material=materials["concrete"], ground=True)
    tenants = (("Footwear","GENERAL"), ("Books","OFFICE"), ("Pharmacy","MEDICAL"),
               ("Menswear","GENERAL"), ("Electronics","OFFICE"), ("Groceries","FOOD"),
               ("Housewares","BUILDING"), ("Record shop","OFFICE"), ("Cafe","FOOD"))
    slots = [(row,col) for row in (-1,1) for col in range(-2,3) if row==1 or col]
    for i, ((name,profile),(side,col)) in enumerate(zip(tenants,slots,strict=True)):
        tag = f"{key}_tenant_{i}"
        cx = x+col*22
        face = y+8+side*5
        cy = face+side*12.5
        box(tag+"_rear", "Tenant stockroom wall", cx, face+side*25, z, 20, .26, 4, material=brick)
        for edge in (-1,1):
            box(f"{tag}_side_{edge}", "Shop partition", cx+edge*9.9, cy, z, .22, 25, 4, material=brick)
            display(f"{tag}_display_{edge}", i, cx+edge*5.6, face, z, 8.5, 3.7, 0 if side==1 else 180)
        door(tag+"_door", name+" entrance", cx, face, z+.03, 2.5, 2.35, finish="petrol")
        box(tag+"_header", "Shop sign backing", cx, face, z+3.65, 20, .32, .95, material=materials["dark_trim"])
        sign(tag+"_sign", name.upper(), cx, face-side*.24, z+4, size=34, yaw=90 if side==1 else -90)
        for edge in (-1,1):
            furnishing(f"{tag}_shelf_{edge}", name+" display shelving", "shelf",
                       cx+edge*6, cy, z+.03, (4,.85,1.75), 90)
        cache(tag+"_stock", name+" stock", cx+6, cy+side*8, z+.03, profile=profile)


def dealership(key, x, y, z, shell, double_door, door, box, glazed_shell, sign, furnishing, cache, wreck, materials):
    front = glazed_shell(key+"_showroom",x-12,y+12,z,47,24,5.7,3,1)
    double_door(door,key+"_entry","Showroom doors",x-12,front,z,3,2.35,"petrol")
    sign(key+"_name","NORTHLINE MOTORS",x-12,front-.45,z+4.55,size=49)
    shop_x = x+27
    service = shell(box,key+"_service",shop_x,y+12,z,23,24,5.1,6,materials["StationBrick"],
                    materials["concrete"],clear_height=3.85)
    door(key+"_shutter","Service bay shutter",shop_x,service,z,6,3.85,roll_up=True)
    furnishing(key+"_salesdesk","Sales desk","desk",x-27,y+17,z,(2.1,.9,1.1),0)
    wreck(key+"_show_car",x-7,y+11,z,22,"BurnedSedan")
    cache(key+"_parts","Parts department",shop_x+7,y+18,z,profile="BUILDING")
    box(key+"_forecourt","Dealer forecourt",x,y-15,z,84,29,.06,material=materials["asphalt"],ground=True)
    for row in range(2):
        for i, model in enumerate(("BurnedCompact","BurnedSedan","AbandonedVan","BurnedCompact")):
            wreck(f"{key}_sale_{row}_{i}",x-33+i*15,y-23+row*13,z+.06,25,model)


def school(key,x,y,z,shell,double_door,door,box,window,sign,furniture,cache,materials):
    brick, concrete = materials["StationBrick"], materials["concrete"]
    cy = y-13
    def school_wall(tag, label, *args, **kwargs):
        if label != "Rear wall":
            box(tag,label,*args,**kwargs)
    front = shell(school_wall,key+"_main",x,cy,z,94,34,4.8,3.4,brick,concrete)
    double_door(door,key+"_main_entry","School entrance",x,front,z,3.4,2.35,"petrol")
    sign(key+"_name","NORTH DISTRICT SECONDARY SCHOOL",x,front-.32,z+3.65,size=43)
    rear = cy+17
    for side in (-1,1):
        box(f"{key}_rear_{side}", "Courtyard wall", x+side*24.35, rear, z, 45.3, .24, 4.8, material=brick)
    box(key+"_rear_head", "Courtyard lintel", x,rear,z+2.35,3.4,.24,2.45,material=brick)
    double_door(door,key+"_courtyard_exit","Courtyard exit",x,rear,z,3.4,2.35,"petrol")
    for row in (-1,1):
        face = cy+row*2.6
        for i, dx in enumerate((-34,-13,13,34)):
            tag = f"{key}_room_{row}_{i}"
            cx = x+dx
            for edge in (-1,1):
                box(f"{tag}_jamb_{edge}", "Classroom corridor wall", cx+edge*5.35,face,z,9.35,.24,3.65,material=brick)
                box(f"{tag}_side_{edge}", "Teaching room side wall",cx+edge*10,cy+row*9.75,z,.18,14.3,3.65,material=brick)
            door(tag+"_door","Classroom door",cx,face,z,1.35,2.1)
            for bank in (-1,1):
                for desk in range(3):
                    furniture(f"{tag}_desk_{bank}_{desk}","Pupil desk","desk",cx+bank*4,cy+row*(5.9+desk*3.3),
                               z)
            if row==-1:
                window(tag+"_window",cx,front,z)
    for dx,name,width in ((-26,"Assembly hall",43),(24,"Sports hall",46)):
        tag = key+("_assembly" if dx<0 else "_gym")
        fy = shell(box,tag,x+dx,y+24,z,width,23,7.5,3,brick,concrete)
        double_door(door,tag+"_entry",name+" entrance",x+dx,fy,z,3,2.35,"petrol")
        sign(tag+"_sign",name.upper(),x+dx,fy-.26,z+3.3)
        if dx<0:
            box(tag+"_stage","Assembly stage",x+dx,y+31,z,29,6,.7,material=materials["timber"])
        else:
            for side in (-1,1):
                box(f"{tag}_bench_{side}","Gym bench",x+dx+side*19,y+24,z,.6,13,.46,
                    material=materials["timber"])
    cache(key+"_office","School office supplies",x-44,cy+7,z,profile="OFFICE")
