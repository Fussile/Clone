"""Generate an original, tileable timber color texture for the build pieces."""

from math import sin, tau
from pathlib import Path
from random import Random
import struct
import zlib


def write_timber_texture(path: Path):
    size = 256
    random = Random(882)
    board_values = [random.randrange(-18, 19) for _ in range(8)]
    pixels = bytearray()
    for y in range(size):
        board, row = divmod(y, 32)
        pixels.append(0)  # PNG filter type for this row.
        for x in range(size):
            u = x / size
            v = row / 32
            wave = 0.35 * sin(tau * (2 * u + board / 7))
            grain = 9 * sin(tau * (10 * v + wave)) + 6 * sin(tau * (17 * v + u))
            shade = board_values[board] + round(grain) + random.randrange(-5, 6)
            if row == 0:
                shade -= 45
            elif row == 1:
                shade -= 20
            elif row == 31:
                shade += 13
            pixels.extend(max(0, min(255, channel + shade)) for channel in (142, 101, 63))

    def chunk(tag, content):
        return struct.pack(">I", len(content)) + tag + content + struct.pack(">I", zlib.crc32(tag + content))

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(pixels))
        + chunk(b"IEND", b"")
    )
