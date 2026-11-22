"""Cut warehouse entries from the source shell, keeping the material and one mesh."""
import json
import math
import struct
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
WORLD_ART = TOOLS.parent / "SourceArt" / "Crossroads" / "World"
EPSILON = 1e-9
DEPTH_OFFSET = .01
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


def subtract_box(polygon, low, high):
    """Yield the disjoint portions outside the entrance volume."""
    if any(max(v[axis] for v in polygon) <= low[axis]
           or min(v[axis] for v in polygon) >= high[axis] for axis in range(3)):
        yield polygon
        return
    for axis in range(3):
        for plane, direction in ((low[axis], 1), (high[axis], -1)):
            if not polygon:
                return
            if any(direction * (v[axis] - plane) < 0 for v in polygon):
                yield clip(polygon, axis, plane, -direction)
            polygon = clip(polygon, axis, plane, direction)


def triangulate(polygon):
    for b, c in zip(polygon[1:], polygon[2:]):
        a = polygon[0]
        if area(a, b, c) > EPSILON:
            yield a, b, c


def opening_bounds(door):
    x, y = door["x"], door["y"]
    half = door["width"] / 2
    if door["yaw"] in (0, 180):
        return (x-half, y-.62, -.1), (x+half, y+.62, door["height"])
    return (x-.65, y-half, -.1), (x+.65, y+half, door["height"])


def bake():
    openings = json.loads((TOOLS / "warehouse_doors.json").read_text(encoding="utf-8"))
    bounds = [opening_bounds(door) for door in openings]
    vertices, indices = {}, []
    for triangle in source_triangles():
        faces = [[(x, -z-DEPTH_OFFSET, y, nx, ny, nz, *uv) for x,y,z,nx,ny,nz,*uv in triangle]]
        for lower, upper in bounds:
            faces = [piece for polygon in faces for piece in subtract_box(polygon, lower, upper)]
        for polygon in faces:
            for piece in triangulate(polygon):
                for x,y,z,nx,ny,nz,*uv in piece:
                    length = math.hypot(nx,ny,nz)
                    vertex = (x, z, -y, nx/length, ny/length, nz/length, *uv)
                    indices.append(vertices.setdefault(vertex, len(vertices)))

    source = (WORLD_ART / "SM_WarehouseExterior.glb").read_bytes()
    json_size, = struct.unpack_from("<I", source, 12)
    document = json.loads(source[20:20+json_size])
    image_view = document["bufferViews"][document["images"][0]["bufferView"]]
    start = 28+json_size+image_view["byteOffset"]
    texture = source[start:start+image_view["byteLength"]]
    data, views, accessors = bytearray(), [], []

    def append(payload, target=None):
        view = {"buffer": 0, "byteOffset": len(data), "byteLength": len(payload)}
        if target:
            view["target"] = target
        views.append(view)
        data.extend(payload)
        data.extend(b"\0" * (-len(data) % 4))
        return len(views)-1

    offset = 0
    for width in ATTRIBUTES.values():
        values = [vertex[offset:offset+width] for vertex in vertices]
        payload = b"".join(struct.pack("<" + "f"*width, *value) for value in values)
        accessor = {"bufferView": append(payload, 34962), "componentType": 5126,
                    "type": f"VEC{width}", "count": len(values)}
        if not offset:
            accessor["min"] = [min(axis) for axis in zip(*values)]
            accessor["max"] = [max(axis) for axis in zip(*values)]
        accessors.append(accessor)
        offset += width
    accessors.append({"bufferView": append(struct.pack(f"<{len(indices)}H", *indices), 34963),
                      "componentType": 5123, "type": "SCALAR", "count": len(indices)})
    name = "SM_WarehouseDoorwayShell"
    document["asset"]["generator"] = "Crossroads warehouse door bake"
    document["nodes"][0]["name"] = name
    document["meshes"][0]["name"] = name
    document["images"][0]["bufferView"] = append(texture)
    document["accessors"], document["bufferViews"] = accessors, views
    document["buffers"] = [{"byteLength": len(data)}]
    header = json.dumps(document, separators=(",", ":")).encode("utf-8")
    header += b" " * (-len(header) % 4)
    glb = (struct.pack("<4sIII4s", b"glTF", 2, 28+len(header)+len(data), len(header), b"JSON")
           + header + struct.pack("<I4s", len(data), b"BIN\0") + data)
    (WORLD_ART / f"{name}.glb").write_bytes(glb)


if __name__ == "__main__":
    bake()
