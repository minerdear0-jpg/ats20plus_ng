#!/usr/bin/env python3
"""
OLED fleeing-wall via RECTANGLES only.
Concept (trapezoids) stays in smeter_wall_concept.png.
On 16 px: pick H/W so the row optically reads as that wall — no slanted edges.
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent
W, H = 128, 16
LAB_W, LAB_GAP, LAB_X = 28, 3, 0
NEEDLE_W = 3
CUBES = 8
SCALE = 6

# Optical perspective: far→near. Even gaps (no break hole).
# Heights: gentle then open (reads as wall flare without 1px jagged tops).
SEG_W = (3, 3, 4, 5, 6, 8, 10, 13)
SEG_GAP = (2, 2, 2, 2, 2, 2, 2)
HORN_H = (4, 5, 6, 8, 10, 12, 14, 14)


def write_png(path: Path, img_w: int, img_h: int, rgb_fn):
    raw = bytearray()
    for y in range(img_h):
        raw.append(0)
        for x in range(img_w):
            r, g, b = rgb_fn(x, y)
            raw += bytes((r, g, b))

    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", img_w, img_h, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )


def cube_edge(cur: int) -> int:
    if not cur:
        return 0
    x = 0
    for i in range(cur):
        x += SEG_W[i]
        if i != cur - 1:
            x += SEG_GAP[i]
    return x


def paint(cur: int, plus: bool) -> list[list[int]]:
    g = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(LAB_X, LAB_X + LAB_W):
            d = min(x - LAB_X, LAB_X + LAB_W - 1 - x)
            if y == 0 or y == H - 1:
                on = d >= 3
            elif y == 1 or y == H - 2:
                on = d >= 2
            else:
                on = d >= 1
            g[y][x] = 1 if on else 0
    x0 = LAB_X + LAB_W + LAB_GAP
    col = 0
    for i in range(CUBES):
        fill = i < cur
        h = HORN_H[i]
        y0 = (H - h) // 2
        y1 = y0 + h - 1
        sw = SEG_W[i]
        for w in range(sw):
            side = w == 0 or w == sw - 1
            x = x0 + col
            for y in range(H):
                if fill or side:
                    g[y][x] = 1 if y0 <= y <= y1 else 0
                else:
                    g[y][x] = 1 if y in (y0, y1) else 0
            col += 1
        if i != CUBES - 1:
            col += SEG_GAP[i]
    bar_w = cube_edge(CUBES)
    bar_max = bar_w - NEEDLE_W
    live = bar_max if (plus or cur >= CUBES) else cube_edge(cur)
    if live > bar_max:
        live = bar_max
    for i in range(NEEDLE_W):
        x = x0 + live + i
        if x >= W:
            break
        for y in range(H):
            g[y][x] = 1 if i == 1 else 0
    return g


def save(name: str, cur: int, plus: bool):
    g = paint(cur, plus)
    iw, ih = W * SCALE, H * SCALE

    def pix(x, y):
        return (236, 248, 236) if g[y // SCALE][x // SCALE] else (10, 12, 10)

    write_png(ROOT / name, iw, ih, pix)


def main():
    print("barW", cube_edge(CUBES), "W", SEG_W, "H", HORN_H)
    save("smeter_wall_S1.png", 1, False)
    save("smeter_wall_S5.png", 5, False)
    save("smeter_wall_S9p.png", 8, True)
    print("wrote OLED rect mocks (concept trapezoid unchanged)")


if __name__ == "__main__":
    main()
