#!/usr/bin/env python3
from __future__ import annotations

import binascii
import math
import pathlib
import struct
import zlib
from PIL import Image as PILImage

ROOT = pathlib.Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "fixtures"


def png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    crc = binascii.crc32(chunk_type + data) & 0xFFFFFFFF
    return struct.pack("!I", len(data)) + chunk_type + data + struct.pack("!I", crc)


def write_png_gray(path: pathlib.Path, pixels: list[list[int]]) -> None:
    height = len(pixels)
    width = len(pixels[0]) if height else 0

    raw = bytearray()
    for row in pixels:
        raw.append(0)
        raw.extend(max(0, min(255, v)) for v in row)

    ihdr = struct.pack("!IIBBBBB", width, height, 8, 0, 0, 0, 0)
    payload = b"\x89PNG\r\n\x1a\n"
    payload += png_chunk(b"IHDR", ihdr)
    payload += png_chunk(b"IDAT", zlib.compress(bytes(raw), level=9))
    payload += png_chunk(b"IEND", b"")
    path.write_bytes(payload)

def write_jpeg_gray(path: pathlib.Path, pixels: list[list[int]]) -> None:
    height = len(pixels)
    width = len(pixels[0]) if height else 0
    payload = bytes(max(0, min(255, v)) for row in pixels for v in row)
    image = PILImage.frombytes("L", (width, height), payload)
    image.save(path, format="JPEG", quality=100, subsampling=0)


def main() -> None:
    FIXTURES.mkdir(parents=True, exist_ok=True)

    white = [[255 for _ in range(4)] for _ in range(4)]
    stripes = [[255, 255, 0, 0] for _ in range(2)]
    gradient = [[min(255, x * 32 + y * 4) for x in range(8)] for y in range(8)]
    checker = [[255 if ((x // 4 + y // 4) % 2 == 0) else 0 for x in range(48)] for y in range(32)]
    radial = []
    center_x = 36 / 2.0
    center_y = 36 / 2.0
    max_dist = math.hypot(center_x, center_y)
    for y in range(36):
        row = []
        for x in range(36):
            dist = math.hypot(x - center_x, y - center_y)
            row.append(int(max(0.0, 255.0 * (1.0 - dist / max_dist))))
        radial.append(row)

    write_png_gray(FIXTURES / "white.png", white)
    write_png_gray(FIXTURES / "stripes.png", stripes)
    write_png_gray(FIXTURES / "gradient.png", gradient)
    write_png_gray(FIXTURES / "checker.png", checker)
    write_png_gray(FIXTURES / "radial.png", radial)
    write_jpeg_gray(FIXTURES / "white.jpg", white)


if __name__ == "__main__":
    main()
