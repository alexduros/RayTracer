#!/usr/bin/env python3
"""Write models/dimples.png, the bundled height map: a golf ball's skin.

Round pits in a flat surface, white high and black deep, packed in offset
rows (a hexagonal packing squeezed by 0.2 % so that it tiles the square
exactly: texture coordinates wrap). 512 x 512, 8-bit grey; 32 pits across, 16
texels each, so that the slopes bump mapping takes from neighbouring texels
are smooth. Standard library only.

    scripts/make-dimples.py [models/dimples.png]
"""
import math
import struct
import sys
import zlib

SIZE = 512
COLUMNS, ROWS = 32, 37  # ROWS / COLUMNS = 2 / sqrt(3), rounded to the nearest whole number
RADIUS = 0.42           # of a pit, in cells: neighbours do not touch, a flat rim remains


def height(x, y):
    """Texel (x, y): 1 on the skin, falling as a paraboloid to 0 at a pit's centre."""
    cell_w, cell_h = SIZE / COLUMNS, SIZE / ROWS
    nearest = math.inf
    row = int((y + 0.5) / cell_h)
    for j in (row - 1, row, row + 1):
        shift = 0.5 * cell_w if j % 2 else 0.0  # every other row is offset by half a cell
        column = int((x + 0.5 - shift) / cell_w)
        for i in (column - 1, column, column + 1):
            dx = abs(x + 0.5 - ((i + 0.5) * cell_w + shift))
            dy = abs(y + 0.5 - (j + 0.5) * cell_h)
            dx, dy = min(dx, SIZE - dx), min(dy, SIZE - dy)  # the picture wraps
            nearest = min(nearest, math.hypot(dx, dy) / cell_w)
    return 1.0 if nearest >= RADIUS else (nearest / RADIUS) ** 2


def write_grey_png(path, size, value):
    rows = b"".join(b"\x00" + bytes(round(255 * value(x, y)) for x in range(size)) for y in range(size))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 0, 0, 0, 0)))  # 8-bit, greyscale
        out.write(chunk(b"IDAT", zlib.compress(rows, 9)))
        out.write(chunk(b"IEND", b""))


if __name__ == "__main__":
    write_grey_png(sys.argv[1] if len(sys.argv) > 1 else "models/dimples.png", SIZE, height)
