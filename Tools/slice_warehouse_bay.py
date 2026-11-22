"""Bake the sample warehouse wall into textured-grid geometry for the region.

Run with Python 3 from any directory. The editor imports the resulting meshes and
applies the original warehouse material to all of them.
"""
import base64
import json
import math
import struct
from pathlib import Path

WORLD_ART = Path(__file__).resolve().parents[1] / "SourceArt" / "Crossroads" / "World"
CELL = 1.05
COLUMNS = 3
ROWS = 3
LEFT = -5.16
WALL_START = 2.99
DEPTH_ORIGIN = 2.82
EPSILON = 1e-9
ATTRIBUTES = {"POSITION": 3, "NORMAL": 3, "TEXCOORD_0": 2, "TEXCOORD_1": 2}


def source_triangles():
    data = (WORLD_ART / "SM_WarehouseExterior.glb").read_bytes()
    json_size, = struct.unpack_from("<I", data, 12)
    gltf = json.loads(data[20:20 + json_size])
    binary = memoryview(data)[28 + json_size:]
    node, = gltf["nodes"]
    if set(node) - {"name", "mesh"}:
        raise ValueError("The warehouse GLB must have its transform applied before slicing.")
    primitive, = gltf["meshes"][node["mesh"]]["primitives"]
    if primitive.get("mode", 4) != 4:
        raise ValueError("The warehouse GLB must contain triangles.")

    def read(index, code, width, component_type):
        accessor = gltf["accessors"][index]
        view = gltf["bufferViews"][accessor["bufferView"]]
        fmt = struct.Struct("<" + code * width)
        stride = view.get("byteStride", fmt.size)
        offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        if accessor["componentType"] != component_type or "sparse" in accessor:
            raise ValueError("Unsupported warehouse mesh accessor.")
        return [fmt.unpack_from(binary, offset + i * stride) for i in range(accessor["count"])]

    streams = [read(primitive["attributes"][name], "f", width, 5126)
               for name, width in ATTRIBUTES.items()]
    vertices = [sum(attributes, ()) for attributes in zip(*streams, strict=True)]
    index_accessor = gltf["accessors"][primitive["indices"]]
    index_type = index_accessor["componentType"]
    indices = read(primitive["indices"], {5123: "H", 5125: "I"}[index_type], 1, index_type)
    return [tuple(vertices[index] for index, in indices[i:i + 3])
            for i in range(0, len(indices), 3)]


def clip(polygon, axis, plane, direction):
    if not polygon:
        return []
    clipped = []
    previous = polygon[-1]
    previous_inside = direction * (previous[axis] - plane) >= 0
    for vertex in polygon:
        inside = direction * (vertex[axis] - plane) >= 0
        if inside != previous_inside:
            fraction = (plane - previous[axis]) / (vertex[axis] - previous[axis])
            cut = [a + fraction * (b - a) for a, b in zip(previous, vertex)]
            cut[axis] = plane
            clipped.append(tuple(cut))
        if inside:
            clipped.append(vertex)
        previous, previous_inside = vertex, inside
    return clipped


def area(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    return math.hypot(u[1]*v[2] - u[2]*v[1], u[2]*v[0] - u[0]*v[2],
                      u[0]*v[1] - u[1]*v[0]) / 2


def slice_wall(triangles, left, right, bottom, top):
    for triangle in triangles:
        # A polygon lying entirely on a boundary belongs to the cell above/right.
        if (min(v[0] for v in triangle) >= right
                or min(v[1] for v in triangle) >= top):
            continue
        polygon = clip(triangle, 2, WALL_START, 1)
        for axis, plane, direction in ((0, left, 1), (0, right, -1), (1, bottom, 1), (1, top, -1)):
            polygon = clip(polygon, axis, plane, direction)
        for b, c in zip(polygon[1:], polygon[2:]):
            a = polygon[0]
            if area(a, b, c) > EPSILON:
                yield a, b, c


def write_panel(path, triangles, center, bottom):
    vertices = []
    for triangle in triangles:
        for x, y, z, nx, ny, nz, *uvs in triangle:
            length = math.hypot(nx, ny, nz)
            vertices.append((x - center, y - bottom, z - DEPTH_ORIGIN,
                             nx / length, ny / length, nz / length, *uvs))
    if not vertices:
        raise ValueError(f"No source geometry for {path.name}.")
    name = path.stem
    doc = {
        "asset": {"version": "2.0", "generator": "Crossroads warehouse slicer"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [{"attributes": {}, "material": 0}]}],
        "materials": [{"name": "WarehousePanelSurface", "doubleSided": True}],
        "accessors": [], "bufferViews": [],
    }
    primitive = doc["meshes"][0]["primitives"][0]
    payload = bytearray()
    component = 0
    for index, (attribute, width) in enumerate(ATTRIBUTES.items()):
        values = [vertex[component:component + width] for vertex in vertices]
        packed = b"".join(struct.pack("<" + "f" * width, *value) for value in values)
        doc["bufferViews"].append({"buffer": 0, "byteOffset": len(payload),
                                   "byteLength": len(packed), "target": 34962})
        accessor = {"bufferView": index, "componentType": 5126,
                    "count": len(values), "type": f"VEC{width}"}
        if attribute == "POSITION":
            accessor["min"] = [min(value) for value in zip(*values)]
            accessor["max"] = [max(value) for value in zip(*values)]
        doc["accessors"].append(accessor)
        primitive["attributes"][attribute] = index
        payload.extend(packed)
        component += width
    indices = struct.pack(f"<{len(vertices)}H", *range(len(vertices)))
    primitive["indices"] = len(doc["accessors"])
    doc["accessors"].append({"bufferView": len(doc["bufferViews"]), "componentType": 5123,
                            "count": len(vertices), "type": "SCALAR"})
    doc["bufferViews"].append({"buffer": 0, "byteOffset": len(payload),
                               "byteLength": len(indices), "target": 34963})
    payload.extend(indices)
    uri = "data:application/octet-stream;base64," + base64.b64encode(payload).decode("ascii")
    doc["buffers"] = [{"uri": uri, "byteLength": len(payload)}]
    path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")


def main():
    triangles = source_triangles()
    output = WORLD_ART / "WarehouseBay"
    output.mkdir(exist_ok=True)
    for row in range(ROWS):
        for column in range(COLUMNS):
            left = LEFT + column * CELL
            right = LEFT + (column + 1) * CELL
            bottom, top = row * CELL, (row + 1) * CELL
            panel = list(slice_wall(triangles, left, right, bottom, top))
            write_panel(output / f"SM_WarehouseBay_{column}_{row}.gltf", panel,
                        (left + right) / 2, bottom)


if __name__ == "__main__":
    main()
