"""Rebuild a self-contained GLB using the original texture bytes shipped with the game."""

import json
import lzma
from pathlib import Path
import runpy
import struct
import zipfile


def load_bake(source, name, kind):
    payload = lzma.decompress((source / "PortableFirearms" / (name + kind + ".xz")).read_bytes())
    (size,) = struct.unpack_from("<I", payload)
    doc = json.loads(payload[4 : 4 + size])
    binary = bytearray(payload[4 + size :])
    if len(binary) != doc["buffers"][0]["byteLength"]:
        raise ValueError(f"Incomplete {name} {kind} buffer")
    return doc, binary


def unpack(source, name, kind, archive, output, glb):
    doc, binary = load_bake(source, name, kind)
    tools = Path(__file__).parent
    actions = runpy.run_path(str(tools / "firearm_actions.py"))
    if name in ("AK12", "MP7", "Scorpion"):
        world, geometry = load_bake(source, name, "World") if kind == "Rig" else (doc, binary)
        islands = runpy.run_path(str(tools / "weapon_sections.py"))["islands"]
        actions["strip_staged_geometry"](name, doc, binary, world, geometry, islands)
    if kind == "Rig":
        actions["build_actions"](doc, binary, name)
    for material in doc["materials"]:
        if material["name"].startswith("Viewmodel"):
            material["pbrMetallicRoughness"].update(metallicFactor=0, roughnessFactor=.62)
    with zipfile.ZipFile(archive) as donor:
        textures = {Path(path).name: path for path in donor.namelist()}
        for image in doc["images"]:
            key = image["name"]
            data = (
                (source / "PlayerArms" / key).read_bytes()
                if key in ("Std_Skin_Arm.jpg", "Std_Nails.jpg")
                else donor.read(textures[key])
            )
            if data.startswith(b"\xff\xd8"):
                mime = "image/jpeg"
            elif data.startswith(b"\x89PNG"):
                mime = "image/png"
            else:
                raise ValueError(f"Unsupported texture {name}/{key}")
            binary.extend(bytes(-len(binary) % 4))
            image.update(bufferView=len(doc["bufferViews"]), mimeType=mime)
            doc["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(data)})
            binary.extend(data)
    binary.extend(bytes(-len(binary) % 4))
    output.parent.mkdir(exist_ok=True, parents=True)
    glb["write_glb"](output, doc, binary)
