"""Rebuild a self-contained GLB using the original texture bytes shipped with the game."""

import json
import lzma
from pathlib import Path
import struct
import zipfile


def unpack(source, name, kind, archive, output, glb):
    payload = lzma.decompress((source / "PortableFirearms" / (name + kind + ".xz")).read_bytes())
    (size,) = struct.unpack_from("<I", payload)
    doc = json.loads(payload[4 : 4 + size])
    binary = bytearray(payload[4 + size :])
    if len(binary) != doc["buffers"][0]["byteLength"]:
        raise ValueError(f"Incomplete {name} {kind} buffer")
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
