"""Bake Crossroads ground and vegetation into ordinary Landscape and World Partition assets."""
from collections import defaultdict
import math

import unreal

from biome_layout import Clearances, Cover, Surface, forage, scatter


PACKAGE = "/Game/Art/Crossroads/World/Biomes"
NAMES = ("BirchA","BirchB","Fern","Shrub","Weeds","Boulder","Stones","FirSapling")
COLORS = ("Meadow","Sand","DryPlain","CityLawn","Forest","Snow","Rock")
NORMALS = ("Meadow","Sand","Snow","Rock","DryPlain","Forest")


def ground_material(plan, mask_path, source, import_asset, west, north):
    assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    editing=unreal.MaterialEditingLibrary
    path=PACKAGE+"/M_CrossroadsGround"
    mat=assets.load_asset(path) if assets.does_asset_exist(path) else None
    if mat:
        editing.delete_all_material_expressions(mat)
    else:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_CrossroadsGround",PACKAGE,unreal.Material,unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("Could not create the regional ground material")

    def connect(node, output, target, pin):
        if not editing.connect_material_expressions(node,output,target,pin):
            raise RuntimeError(f"Could not connect {pin} in the regional ground material")

    def custom(code, kind, bindings, x,y):
        node=editing.create_material_expression(mat,unreal.MaterialExpressionCustom,x,y)
        inputs=[]
        for name in bindings:
            pin=unreal.CustomInput()
            pin.set_editor_property("input_name",unreal.Name(name))
            inputs.append(pin)
        node.set_editor_property("inputs",inputs)
        node.set_editor_property("output_type",kind)
        node.set_editor_property("code",code)
        for name,(value,output) in bindings.items():
            connect(value,output,node,name)
        return node

    mask=import_asset(mask_path,PACKAGE,unreal.Texture2D,refresh_if_source_changed=True)
    mask.set_editor_property("srgb",False)
    mask.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_MASKS)
    mask.set_editor_property("address_x",unreal.TextureAddress.TA_CLAMP)
    mask.set_editor_property("address_y",unreal.TextureAddress.TA_CLAMP)
    if not assets.save_loaded_asset(mask):
        raise RuntimeError("Could not save the regional biome weights")
    vec=unreal.CustomMaterialOutputType
    basepos=editing.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-1450,0)
    w,d=plan["size_m"];cx,cy=plan["downtown_xy_m"]
    map_uv=custom(f"return (P.xy-float2({west:.3f},{north:.3f}))/float2({w*100.}, {d*100.});",
                  vec.CMOT_FLOAT2,{"P":(basepos,"")},-1200,0)
    def sample(texture,uv,kind,y):
        node=editing.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-520,y)
        node.set_editor_property("texture",texture)
        node.set_editor_property("sampler_type",kind)
        if kind != unreal.MaterialSamplerType.SAMPLERTYPE_MASKS:
            node.set_editor_property("sampler_source",unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        connect(uv,"",node,"UVs")
        return node

    sampler=unreal.MaterialSamplerType
    weights=sample(mask,map_uv,sampler.SAMPLERTYPE_MASKS,-320)
    urban=custom(f"float2 m=float2(UV.x*{w}., (1-UV.y)*{d}.); "
                 f"return 1-smoothstep(.68,1.25,length((m-float2({cx:.1f},{cy:.1f}))/float2(790.,610.)));",
                 vec.CMOT_FLOAT1,{"UV":(map_uv,"")},-850,-400)
    bindings={"Zones":(weights,"RGB"),"Exposure":(weights,"A"),"City":(urban,"")}
    normals=dict(bindings)
    # UVs are in metres. World projection keeps the texture scale independent of the 5 km Landscape.
    repeats={"Meadow":4,"DryPlain":4,"Sand":5,"Snow":4,"Rock":5,"Forest":2.5,"CityLawn":1.4}
    broad_uv=custom("return float2(dot(P.xy,float2(.8,.6)),dot(P.xy,float2(-.6,.8)))/2250.;",
                    vec.CMOT_FLOAT2,{"P":(basepos,"")},-1100,-600)
    for idx,name in enumerate(COLORS):
        uv=custom(f"return P.xy/{repeats[name]*100:.1f};",vec.CMOT_FLOAT2,{"P":(basepos,"")},-1000,idx*210)
        texture=import_asset(source/f"T_{name}.jpg",PACKAGE,unreal.Texture2D,refresh_if_source_changed=True)
        detail=sample(texture,uv,sampler.SAMPLERTYPE_COLOR,idx*210)
        broad=sample(texture,broad_uv,sampler.SAMPLERTYPE_COLOR,idx*210+90)
        bindings[name]=(custom("return Detail*.8+Broad*.2;",vec.CMOT_FLOAT3,
                               {"Detail":(detail,"RGB"),"Broad":(broad,"RGB")},-320,idx*210),"")
        if name in NORMALS:
            texture=import_asset(source/f"T_{name}_Normal.jpg",PACKAGE,unreal.Texture2D,refresh_if_source_changed=True)
            texture.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb",False)
            if not assets.save_loaded_asset(texture):
                raise RuntimeError(f"Could not save {name} normal map")
            normals[name]=(sample(texture,uv,sampler.SAMPLERTYPE_NORMAL,1650+idx*200),"RGB")
    color_code="""
        float steep = saturate((Exposure*255-32)/223);
        float3 soil = lerp(Meadow, DryPlain, smoothstep(0,.65,Zones.r));
        soil = lerp(soil,Sand,smoothstep(.59,.98,Zones.r));
        soil = lerp(soil,Forest,Zones.g);
        soil = lerp(soil,CityLawn,City);
        soil = lerp(soil,Rock,steep);
        return lerp(soil,Snow,Zones.b*(1-.82*steep));
    """
    normal_code="""
        float steep = saturate((Exposure*255-32)/223);
        float3 n = lerp(Meadow,DryPlain,smoothstep(0,.65,Zones.r));
        n = lerp(n,Sand,smoothstep(.59,.98,Zones.r));
        n = lerp(n,Forest,Zones.g);
        n = lerp(n,Meadow,City);
        n = lerp(n,Rock,steep);
        return normalize(lerp(n,Snow,Zones.b*(1-.82*steep)));
    """
    color=custom(color_code,vec.CMOT_FLOAT3,bindings,-80,0)
    normal=custom(normal_code,vec.CMOT_FLOAT3,normals,-80,800)
    rough=editing.create_material_expression(mat,unreal.MaterialExpressionConstant,-80,1450)
    rough.set_editor_property("r",.87)
    for node,prop in ((color,unreal.MaterialProperty.MP_BASE_COLOR),(normal,unreal.MaterialProperty.MP_NORMAL),
                      (rough,unreal.MaterialProperty.MP_ROUGHNESS)):
        if not editing.connect_material_property(node,"",prop):
            raise RuntimeError(f"Could not connect ground material output {prop}")
    editing.recompile_material(mat)
    if not assets.save_loaded_asset(mat):
        raise RuntimeError("Could not save the regional ground material")
    return mat


def water_geometry(plan, height):
    lakes = []
    for lake in plan["lakes"]:
        x,y = lake["xy_m"]
        rx,ry = lake["radii_m"]
        level = height(x,y)+3
        lakes.append((x,y,rx,ry,level))
        yield lake["id"], "Disk", x,y,level, 2*rx,2*ry,1,0,0

    def surface(x,y):
        # Match the level basin at a river mouth before following the channel inland.
        bed = height(x,y)+2.4
        shore,level = min(((math.hypot((x-cx)/rx,(y-cy)/ry)-1)*min(rx,ry),z)
                          for cx,cy,rx,ry,z in lakes)
        blend = min(1,max(0,shore/24))
        return level+(bed-level)*blend

    for river in plan["rivers"]:
        width = river["width_m"]
        points = river["points_m"]
        for i,(a,b) in enumerate(zip(points,points[1:])):
            dx,dy = b[0]-a[0],b[1]-a[1]
            length = math.hypot(dx,dy)
            if not length:
                continue
            count = math.ceil(length/20)
            run = length/count
            yaw = math.degrees(math.atan2(-dy,dx))
            z = surface(*a)
            for j in range(count):
                x,y = a[0]+dx*(j+1)/count,a[1]+dy*(j+1)/count
                end = surface(x,y)
                yield (f"{river['id']}_{i}_{j}", "Plane", x-dx/(2*count),y-dy/(2*count),
                       (z+end)/2,math.hypot(run,end-z)+.04,width,1,
                       math.degrees(math.atan2(end-z,run)),yaw)
                z=end
            if i+1<len(points)-1:
                yield f"{river['id']}_bend_{i}", "Disk", *b,z-.015,width,width,1,0,0


def place_water(plan,height,actors,existing,west,north,base_z,make_material,disk):
    mat = make_material(PACKAGE,"M_CrossroadsOpenWater",(),tint=unreal.LinearColor(.032,.12,.127,1),
                        scalars=[(.13,unreal.MaterialProperty.MP_ROUGHNESS)])
    editing = unreal.MaterialEditingLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.get_metadata_tag(mat,"WaterRipples"):
        mat.set_editor_property("blend_mode",unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property("two_sided",True)
        mat.set_editor_property("translucency_lighting_mode",unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        fade=editing.create_material_expression(mat,unreal.MaterialExpressionDepthFade,-600,-250)
        fade.set_editor_property("opacity_default",.78)
        fade.set_editor_property("fade_distance_default",185.0)
        fresnel=editing.create_material_expression(mat,unreal.MaterialExpressionFresnel,-600,-100)
        opacity=editing.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate,-240,-250)
        opacity.set_editor_property("const_b",.94)
        if not (editing.connect_material_expressions(fade,"",opacity,"A") and
                editing.connect_material_expressions(fresnel,"",opacity,"Alpha") and
                editing.connect_material_property(opacity,"",unreal.MaterialProperty.MP_OPACITY)):
            raise RuntimeError("Could not build shoreline and reflection falloff")
        pos=editing.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-800,200)
        time=editing.create_material_expression(mat,unreal.MaterialExpressionTime,-800,340)
        ripple=editing.create_material_expression(mat,unreal.MaterialExpressionCustom,-400,200)
        inputs=[]
        for name in ("P","T"):
            pin=unreal.CustomInput()
            pin.set_editor_property("input_name",unreal.Name(name))
            inputs.append(pin)
        ripple.set_editor_property("inputs",inputs)
        ripple.set_editor_property("output_type",unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        ripple.set_editor_property("code","""
            float2 p=P.xy*.01;
            float swell=sin(dot(p,float2(.83,.46))+.82*T);
            float2 ripples=float2(sin(dot(p,float2(3.6,1.4))+1.8*T+.9*swell),
                                  cos(dot(p,float2(-1.9,4.7))-1.4*T+.65*swell));
            float2 wind=float2(cos(p.x*8.7+p.y*4.2+2.7*T),sin(p.y*7.4-p.x*3.2+2.1*T));
            return normalize(float3(.11*ripples+.034*wind+.05*swell,1));
        """)
        if not (editing.connect_material_expressions(pos,"",ripple,"P") and
                editing.connect_material_expressions(time,"",ripple,"T") and
                editing.connect_material_property(ripple,"",unreal.MaterialProperty.MP_NORMAL)):
            raise RuntimeError("Could not connect the water ripple material")
        editing.recompile_material(mat)
        assets.set_metadata_tag(mat,"WaterRipples","1")
        if not assets.save_loaded_asset(mat):
            raise RuntimeError("Could not save water material")
    meshes={"Plane":unreal.load_asset("/Engine/BasicShapes/Plane.Plane"),"Disk":disk}
    groups=defaultdict(list)
    for key,shape,x,y,z,w,d,h,pitch,yaw in water_geometry(plan,height):
        i,j=int(x//256),int(y//256)
        pos=unreal.Vector((x-i*256)*100,(j*256-y)*100,z*100)
        groups[i,j,shape].append(unreal.Transform(pos,unreal.Rotator(pitch=pitch,yaw=yaw),unreal.Vector(w,d,h)))
    old={str(t):a for a in existing for t in a.tags if str(t).startswith("CrossroadsSurfaceWater_")}
    cls=unreal.load_class(None,"/Script/Survival.SurvivalFacade")
    for (i,j,shape),instances in groups.items():
        key=f"CrossroadsSurfaceWater_{i}_{j}_{shape}"
        at=unreal.Vector(west+i*25600,north+(plan["size_m"][1]-j*256)*100,base_z)
        actor=old.pop(key,None)
        if actor is None:
            actor=actors.spawn_actor_from_class(cls,at)
            actor.set_editor_property("tags",[unreal.Name(key)])
            actor.set_actor_label(f"Surface water ({i}, {j})")
        else:
            actor.set_actor_location(at,False,False)
        actor.set_static_instances(meshes[shape],instances)
        component=actor.get_component_by_class(unreal.InstancedStaticMeshComponent)
        component.set_editor_property("disallow_nanite",True)
        component.set_material(0,mat)
        component.set_collision_profile_name("CrossroadsWater")
        component.set_editor_property("cast_shadow",False)
    for actor in old.values():
        actors.destroy_actor(actor)


def place_resources(plan,height,cover,clear,meshes,existing,actors,west,north,base_z):
    cls=unreal.load_class(None,"/Script/Survival.SurvivalSupplyCrate")
    old={str(tag):actor for actor in existing for tag in actor.tags if str(tag).startswith("CrossroadsForage_")}
    labels={"Deadwood":"Fallen timber","Saguaro":"Dry cactus ribs","Geode":"Exposed quartz geode"}
    for i,j,name,x,y,z,yaw,scale in forage(plan,height,cover,clear):
        key=f"CrossroadsForage_{i}_{j}_{'stone' if name=='Geode' else 'wood'}"
        at=unreal.Vector(west+x*100,north+(plan["size_m"][1]-y)*100,base_z+100*z)
        actor=old.pop(key,None)
        if actor is None:
            actor=actors.spawn_actor_from_class(cls,at)
            actor.set_editor_property("tags",[unreal.Name(key)])
        actor.modify()
        actor.set_actor_label(labels[name])
        actor.set_actor_location_and_rotation(at,unreal.Rotator(yaw=yaw),False,False)
        actor.set_actor_scale3d(unreal.Vector(scale,scale,scale))
        actor.set_editor_property("loot_name",labels[name])
        actor.set_editor_property("timber_amount",{"Saguaro":4,"Deadwood":8,"Geode":0}[name])
        actor.set_editor_property("stone_amount",6 if name=="Geode" else 0)
        comp=actor.get_component_by_class(unreal.StaticMeshComponent)
        comp.set_static_mesh(meshes[name])
        comp.set_collision_profile_name("BlockAll")
    for actor in old.values():
        actors.destroy_actor(actor)


def place(plan, raw, output, source, import_asset, make_material):
    height=Surface(raw,plan)
    cover=Cover(plan)
    mask=output/"T_CrossroadsBiomes.png"
    cover.write_mask(mask,plan,height)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    existing=actors.get_all_level_actors()
    landscapes=[a for a in existing if isinstance(a,unreal.LandscapeProxy)]
    bounds=[a.get_actor_bounds(False) for a in landscapes]
    west=min(p.x-e.x for p,e in bounds)
    north=min(p.y-e.y for p,e in bounds)
    east=max(p.x+e.x for p,e in bounds)
    south=max(p.y+e.y for p,e in bounds)
    w,d=plan["size_m"]
    if abs(east-west-w*100)>400 or abs(south-north-d*100)>400:
        raise RuntimeError("The landscape bounds are incomplete for the biome bake.")
    material=ground_material(plan,mask,source,import_asset,west,north)
    turf=unreal.load_asset(PACKAGE+"/T_CityLawn")
    make_material("/Game/Art/Crossroads/World","M_Region_grass",
                  [(turf,unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,"RGB",unreal.MaterialProperty.MP_BASE_COLOR)],
                  world_repeat_cm=140,scalars=[(.87,unreal.MaterialProperty.MP_ROUGHNESS)])
    prefix="CrossroadsVegetation_"
    cells={str(tag):actor for actor in existing for tag in actor.tags if str(tag).startswith(prefix)}
    for terrain in landscapes:
        if terrain.get_editor_property("landscape_material")!=material:
            terrain.set_editor_property("landscape_material",material)
    root=next(a for a in existing if isinstance(a,unreal.Landscape))
    origin=root.get_actor_location()
    meshes={name:import_asset(source/f"SM_{name}.glb",f"{PACKAGE}/{name}",unreal.StaticMesh,
                              refresh_if_source_changed=True) for name in (*NAMES,"WaterDisk","Deadwood","Saguaro","Geode")}
    for name in ("Boulder","Stones","WaterDisk","Deadwood","Saguaro","Geode"):
        if not unreal.CrossroadsWorldBake.set_static_mesh_collision(meshes[name]) or not assets.save_loaded_asset(meshes[name]):
            raise RuntimeError(f"Could not save {name} collision")
    meshes["Trunks"]=unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
    resources=[a for a in existing if any(str(tag).startswith("CrossroadsForage_") for tag in a.tags)]
    place_water(plan,height,actors,existing,west,north,origin.z,make_material,meshes["WaterDisk"])
    clear=Clearances(plan)
    place_resources(plan,height,cover,clear,meshes,resources,actors,west,north,origin.z)
    batches=scatter(plan,height,cover,clear)
    trunks=defaultdict(list)
    for (i,j,name),points in batches.items():
        if name.startswith("Birch"):
            trunks[i,j,"Trunks"].extend(points)
    batches.update(trunks)
    actor_class=unreal.load_class(None,"/Script/Survival.SurvivalFacade")
    limits={"BirchA":1100,"BirchB":1100,"FirSapling":165,"Boulder":650,"Stones":300,"Shrub":210,"Fern":115,"Weeds":95}
    for (i,j,name),points in sorted(batches.items()):
        key=f"{prefix}{i}_{j}_{name}"
        # World Partition assigns an actor to a cell by its bounds. Local instance positions
        # keep the actor and its geometry together when a client streams a district.
        bx,by=i*256,j*256
        position=unreal.Vector(west+bx*100,north+(d-by)*100,origin.z)
        actor=cells.pop(key,None)
        if actor is None:
            actor=actors.spawn_actor_from_class(actor_class,position)
            if not actor:
                raise RuntimeError(f"Could not stamp {key}")
            actor.set_editor_property("tags",[unreal.Name(key)])
            actor.set_actor_label(f"{name} ({i}, {j})")
        else:
            actor.set_actor_location(position,False,False)
        transforms=[]
        for x,y,z,yaw,size in points:
            if name=="Trunks":
                scale=unreal.Vector(.36*size,.36*size,3.2*size)
                z+=1.6*size
            else:
                scale=unreal.Vector(size,size,size)
                if name in {"Boulder","Stones"}:
                    z-=.11*size
            at=unreal.Vector((x-bx)*100,(by-y)*100,z*100)
            transforms.append(unreal.Transform(at,unreal.Rotator(yaw=yaw),scale))
        actor.set_static_instances(meshes[name],transforms)
        component=actor.get_component_by_class(unreal.InstancedStaticMeshComponent)
        if component.get_instance_count()!=len(transforms):
            raise RuntimeError(f"Incomplete vegetation batch {key}")
        component.set_collision_profile_name("BlockAll" if name in {"Boulder","Stones","Trunks"} else "NoCollision")
        if name=="Trunks":
            component.set_visibility(False)
            component.set_editor_property("cast_shadow",False)
        else:
            distance=limits[name]*100
            component.set_cull_distances(int(distance*.8),distance)
    for actor in cells.values():
        if not actors.destroy_actor(actor):
            raise RuntimeError("Could not remove an obsolete vegetation batch")
    unreal.log(f"Crossroads: baked {sum(len(v) for k,v in batches.items() if k[2]!='Trunks')} landscape instances.")
