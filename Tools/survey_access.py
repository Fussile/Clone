"""Open walk-through breaches in the indexed Crossroads survey meshes, retaining UVs."""
import json
import struct


def prepare(source, directory, openings):
    data = source.read_bytes()
    length = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20+length])
    binary = bytearray(data[28+length:])
    for part in doc["meshes"][0]["primitives"]:
        def values(index, fmt):
            accessor = doc["accessors"][index]
            view = doc["bufferViews"][accessor["bufferView"]]
            offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
            stride = view.get("byteStride", struct.calcsize(fmt))
            return [struct.unpack_from(fmt, binary, offset+i*stride) for i in range(accessor["count"])]

        positions = values(part["attributes"]["POSITION"], "<fff")
        accessor = doc["accessors"][part["indices"]]
        fmt = {5123: "<H", 5125: "<I"}[accessor["componentType"]]
        indices = [v for v, in values(part["indices"], fmt)]
        kept = []
        for i in range(0, len(indices), 3):
            face = indices[i:i+3]
            points = [positions[j] for j in face]
            # Drop triangles touching the cut, including spans across a whole doorway.
            if any(all(min(p[k] for p in points) <= hi and max(p[k] for p in points) >= lo
                       for k, (lo, hi) in enumerate(box)) for box in openings):
                continue
            kept.extend(face)
        if not kept:
            part["_empty"] = True
            continue
        binary.extend(bytes(-len(binary)%4))
        offset = len(binary)
        binary.extend(struct.pack("<"+str(len(kept))+fmt[-1], *kept))
        accessor.update(bufferView=len(doc["bufferViews"]), byteOffset=0, count=len(kept),
                        min=[min(kept)], max=[max(kept)])
        doc["bufferViews"].append(dict(buffer=0, byteOffset=offset, byteLength=len(binary)-offset))
    mesh = doc["meshes"][0]
    mesh["primitives"] = [p for p in mesh["primitives"] if not p.pop("_empty", False)]
    binary.extend(bytes(-len(binary)%4))
    doc["buffers"][0]["byteLength"] = len(binary)
    header = json.dumps(doc,separators=(",",":")).encode()
    header += b" "*(-len(header)%4)
    directory.mkdir(parents=True, exist_ok=True)
    output = directory/source.name
    output.write_bytes(struct.pack("<4sIII4s", b"glTF", 2, 28+len(header)+len(binary),len(header),b"JSON")
                       +header+struct.pack("<I4s",len(binary),b"BIN\0")+binary)
    return output
