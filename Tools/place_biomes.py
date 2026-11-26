"""Bake Crossroads ground and vegetation into ordinary Landscape and World Partition assets."""
from collections import defaultdict

import unreal

from biome_layout import Clearances, Cover, Surface, scatter


PACKAGE = "/Game/Art/Crossroads/World/Biomes"
NAMES = ("BirchA","BirchB","Fern","Shrub","Weeds","Boulder","Stones")
COLORS = ("Meadow","Sand","DryPlain","CityLawn","Forest","Snow","Rock")
NORMALS = ("Meadow","Sand","Snow","Rock")


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
        connect(uv,"",node,"Coordinates")
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
    for idx,name in enumerate(COLORS):
        uv=custom(f"return P.xy/{repeats[name]*100:.1f};",vec.CMOT_FLOAT2,{"P":(basepos,"")},-1000,idx*210)
        texture=import_asset(source/f"T_{name}.jpg",PACKAGE,unreal.Texture2D,refresh_if_source_changed=True)
        bindings[name]=(sample(texture,uv,sampler.SAMPLERTYPE_COLOR,idx*210),"RGB")
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
        float3 n = lerp(Meadow,Sand, Zones.r*(1-City));
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
                              refresh_if_source_changed=True) for name in NAMES}
    for name in ("Boulder","Stones"):
        if not unreal.CrossroadsWorldBake.set_static_mesh_collision(meshes[name]) or not assets.save_loaded_asset(meshes[name]):
            raise RuntimeError(f"Could not save {name} collision")
    meshes["Trunks"]=unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
    batches=scatter(plan,height,cover,Clearances(plan))
    trunks=defaultdict(list)
    for (i,j,name),points in batches.items():
        if name.startswith("Birch"):
            trunks[i,j,"Trunks"].extend(points)
    batches.update(trunks)
    actor_class=unreal.load_class(None,"/Script/Survival.SurvivalFacade")
    limits={"BirchA":1100,"BirchB":1100,"Boulder":650,"Stones":300,"Shrub":210,"Fern":115,"Weeds":95}
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
