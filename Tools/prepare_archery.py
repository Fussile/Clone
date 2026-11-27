"""Prepare matching static and animated field weapons from the same rest geometry."""
import copy
import lzma
from pathlib import Path
import runpy
import zipfile

PACKS = {
    "PrimitiveBow": "cool_bow_primitive.glb",
    "WoodenBow": "wooden_bow_free.glb",
    "CompoundBow": "compound-bow.zip",
    "Crossbow": "crossbow_rigged_and_animated.glb",
    "Knife": "knife-animated_fps1.zip",
}


def build_models(name, directory, output, glb):
    output.mkdir(parents=True, exist_ok=True)
    path = output / "Action.glb"
    path.write_bytes(lzma.decompress((directory / (name + ".glb.xz")).read_bytes()))
    model, binary = glb["read_glb"](path)
    def image_data(data, mime, label):
        binary.extend(bytes(-len(binary) % 4))
        index = len(model["bufferViews"])
        model["bufferViews"].append(dict(buffer=0, byteOffset=len(binary), byteLength=len(data)))
        binary.extend(data)
        return dict(bufferView=index, mimeType=mime, name=label)
    pack = directory / PACKS[name]
    if pack.suffix == ".glb":
        donor, pixels = glb["read_glb"](pack)
        material = donor["materials"][0].copy()
        # Source samplers, texture indices and texture coordinates stay intact.
        for key in ("samplers", "textures", "extensionsUsed"):
            if key in donor: model[key] = donor[key]
        model["images"] = []
        for i, image in enumerate(donor["images"]):
            view = donor["bufferViews"][image["bufferView"]]
            start = view.get("byteOffset", 0)
            model["images"].append(image_data(pixels[start:start+view["byteLength"]],image["mimeType"],name+str(i)))
    else:
        base = "textures/knifeColor.png" if name == "Knife" else "textures/blinn1_Base_Color.png"
        with zipfile.ZipFile(pack) as archive:
            model["images"] = [image_data(archive.read(base),"image/png",name+"Color")]
        for suffix in ("Normal", "MetalRough"):
            model["images"].append(image_data((directory / (name+suffix+".jpg")).read_bytes(),"image/jpeg",name+suffix))
        model["textures"] = [{"source":i} for i in range(3)]
        material = {"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":2}},
                    "normalTexture":{"index":1}}
    for section in model["materials"]:
        section.update(material, name=section["name"], doubleSided=True)
    binary.extend(bytes(-len(binary) % 4))
    glb["write_glb"](path,model,binary)
    world = copy.deepcopy(model)
    world["nodes"] = [{"name":"World","mesh":0}]
    world["scenes"] = [{"nodes":[0]}]
    for key in ("skins","animations"): del world[key]
    for prim in world["meshes"][0]["primitives"]:
        for key in ("JOINTS_0","WEIGHTS_0"): del prim["attributes"][key]
    glb["write_glb"](output / "World.glb",world,binary)


def prepare(tools, source, art, import_asset):
    import unreal
    glb = runpy.run_path(str(tools/"prepare_sidearm_glb.py"))
    import_rig = runpy.run_path(str(tools/"prepare_arsenal.py"))["import_rig"]
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    directory = source/"Archery"
    for name in PACKS:
        dest = f"{art}/Arsenal/{name}/Field"
        rig = unreal.load_asset(dest+"/Rig0") if assets.does_asset_exist(dest+"/Rig0") else None
        world = unreal.load_asset(dest+"/World") if assets.does_asset_exist(dest+"/World") else None
        if not (rig and world and assets.does_asset_exist(dest+"/Presentation0")):
            output = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/"FieldWeapons"/name
            build_models(name,directory,output,glb)
            world = import_asset(output/"World.glb",dest,unreal.StaticMesh)
            import_rig(output/"Action.glb",dest,{})
            rig = unreal.load_asset(dest+"/Rig0")
        if assets.get_metadata_tag(world,"FieldMounts") != "1":
            if not unreal.CrossroadsWorldBake.prepare_field_mounts(world,rig):
                raise RuntimeError(f"Missing rest landmarks for {name}")
            assets.set_metadata_tag(world,"FieldMounts","1")
            if not assets.save_loaded_asset(world) or not assets.save_loaded_asset(rig):
                raise RuntimeError(f"Could not save {name} landmarks")
