#!/usr/bin/env python3
"""
S-meter lab from fonts/GOST Type A/GOST_A.TTF → 9×16 slot.
No blur. Auto LANCZOS for most glyphs; 4/5/S/+ hand-tuned (clean type A).
Layout: pad2 + 9 + gap1 + 9 + 9 = 30 → SMETER_LAB_W.
Bakes ATS_EX/font7x14smeter.h — do not hand-patch bytes.
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
TTF = ROOT / "GOST Type A" / "GOST_A.TTF"
OUT_H = REPO / "ATS_EX" / "font7x14smeter.h"
GW, GH = 9, 16
SRC_PX = 144
STROKE = 1
ORDER = list("0123456789") + ["S", "+", " "]

THR = {
    "0": 118, "1": 125, "2": 118, "3": 118,
    "6": 118, "7": 120, "8": 112, "9": 118,
}

# 9×16 — open 4, smooth 5, S ≠ 5, even + . 2 px stems where it matters.
HAND: dict[str, list[str]] = {
    # Clean tall oval — auto had dirty top/asymmetric walls (SNR "0").
    "0": [
        ".........",
        "..#####..",
        ".##...##.",
        "##.....##",
        "##.....##",
        "##.....##",
        "##.....##",
        "##.....##",
        "##.....##",
        "##.....##",
        "##.....##",
        "##.....##",
        "##.....##",
        ".##...##.",
        "..#####..",
        ".........",
    ],
    "4": [
        ".........",
        ".##......",
        "##.......",
        "#.#......",
        "#..#.....",
        "#..##....",
        "#...##...",
        "#...##...",
        "#########",
        "....##...",
        "....##...",
        "....##...",
        "....##...",
        "....##...",
        "....##...",
        ".........",
    ],
    "5": [
        ".........",
        ".#######.",
        ".##......",
        ".#.......",
        ".#.......",
        ".#####...",
        ".######..",
        ".....##..",
        "......##.",
        ".......#.",
        ".......#.",
        "......##.",
        ".....##..",
        ".#####...",
        "..###....",
        ".........",
    ],
    "S": [
        ".........",
        "..#####..",
        ".##...##.",
        ".#.....#.",
        ".#.......",
        ".##......",
        "..###....",
        "...###...",
        "....###..",
        ".....##..",
        "......##.",
        ".#.....#.",
        ".##...##.",
        "..#####..",
        ".........",
        ".........",
    ],
    "+": [
        ".........",
        ".........",
        "....##...",
        "....##...",
        "....##...",
        "....##...",
        ".#######.",
        ".#######.",
        "....##...",
        "....##...",
        "....##...",
        "....##...",
        ".........",
        ".........",
        ".........",
        ".........",
    ],
}


def render_hi(ch: str) -> Image.Image:
    font = ImageFont.truetype(str(TTF), size=SRC_PX)
    img = Image.new("L", (SRC_PX * 2, SRC_PX * 2), 0)
    ImageDraw.Draw(img).text(
        (SRC_PX // 3, SRC_PX // 4),
        ch,
        font=font,
        fill=255,
        stroke_width=STROKE,
        stroke_fill=255,
    )
    bbox = img.getbbox()
    assert bbox
    return img.crop(bbox)  # no blur — blur = dirt on 1-bit


def autosample(ch: str) -> list[str]:
    hi = render_hi(ch)
    cw, chh = hi.size
    scale = min(GW / cw, GH / chh)
    nw = min(GW, max(1, int(round(cw * scale))))
    nh = min(GH, max(1, int(round(chh * scale))))
    small = hi.resize((nw, nh), Image.Resampling.LANCZOS)
    canvas = Image.new("L", (GW, GH), 0)
    canvas.paste(small, ((GW - nw) // 2, (GH - nh) // 2))
    thr = THR.get(ch, 118)
    bw = canvas.point(lambda p, t=thr: 255 if p >= t else 0)
    px = bw.load()
    return ["".join("#" if px[x, y] else "." for x in range(GW)) for y in range(GH)]


def glyph_rows(ch: str) -> list[str]:
    if ch == " ":
        return ["." * GW] * GH
    if ch in HAND:
        rows = HAND[ch]
        assert all(len(r) == GW for r in rows) and len(rows) == GH
        return rows
    return autosample(ch)


def grid_to_cols(rows: list[str]) -> list[int]:
    cols: list[int] = []
    for page in range(2):
        for x in range(GW):
            b = 0
            for bit in range(8):
                if rows[page * 8 + bit][x] == "#":
                    b |= 1 << bit
            cols.append(b)
    return cols


def write_png(path: Path, img_w: int, img_h: int, rgb_fn):
    raw = bytearray()
    for y in range(img_h):
        raw.append(0)
        for x in range(img_w):
            r, g, b = rgb_fn(x, y)
            raw += bytes((r, g, b))

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", img_w, img_h, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )


def main():
    if not TTF.is_file():
        raise SystemExit(f"missing {TTF}")
    grids = [glyph_rows(ch) for ch in ORDER]
    tables = [grid_to_cols(g) for g in grids]

    lines = [
        "// ----------------------------------------------------------------------",
        "// S-meter 9x16 from GOST Type A (GOST_A.TTF). 4/5/S/+ hand-tuned.",
        "// fonts/smeter_gost7x14.py — do not hand-patch bytes.",
        f"// Glyph data: {len(ORDER) * GW * 2} bytes PROGMEM.",
        "// ----------------------------------------------------------------------",
        "",
        "#include <avr/pgmspace.h>",
        "",
        "#define SMETER_7X14_W 9",
        "#define SMETER_7X14_BYTES (SMETER_7X14_W * 2)",
        "#define SMETER_7X14_S 10",
        "#define SMETER_7X14_PLUS 11",
        "#define SMETER_7X14_SPC 12",
        "",
        "const uint8_t ssd1306xled_font7x14smeter [] PROGMEM =",
        "{",
    ]
    names = list("0123456789") + ["S", "+", "spc"]
    for name, cols in zip(names, tables):
        hexb = ",".join(f"0x{b:02X}" for b in cols)
        lines.append(f"\t{hexb},  // {name}")
    lines.append("};")
    lines.append("")
    OUT_H.write_text("\n".join(lines) + "\n")

    scale, gap = 5, 2
    n = len(ORDER)
    img_w = n * (GW + gap) * scale
    img_h = GH * scale

    def pix(x, y):
        cell = x // ((GW + gap) * scale)
        if cell >= n:
            return (12, 12, 12)
        lx = (x // scale) - cell * (GW + gap)
        ly = y // scale
        if lx < 0 or lx >= GW or ly < 0 or ly >= GH:
            return (12, 12, 12)
        return (240, 240, 240) if grids[cell][ly][lx] == "#" else (20, 20, 20)

    write_png(ROOT / "smeter_gost7x14.png", img_w, img_h, pix)

    def chip_png(path: Path, unit: int, plus: bool):
        pad, s_gap = 2, 1
        sc = 5
        gs = [
            grids[ORDER.index("S")],
            grids[ORDER.index(str(unit))],
            grids[ORDER.index("+" if plus else " ")],
        ]
        xs = [pad, pad + GW + s_gap, pad + GW + s_gap + GW]
        bw = pad + GW + s_gap + GW + GW  # 30

        def rgb(x, y):
            lx, ly = x // sc, y // sc
            if lx >= bw or ly >= GH:
                return (0, 0, 0)
            for g, x0 in zip(gs, xs):
                if x0 <= lx < x0 + GW and g[ly][lx - x0] == "#":
                    return (240, 240, 240)
            return (20, 20, 20)

        write_png(path, bw * sc, GH * sc, rgb)

    chip_png(ROOT / "smeter_gost_chip_S5.png", 5, False)
    chip_png(ROOT / "smeter_gost_chip_S4.png", 4, False)
    chip_png(ROOT / "smeter_gost_chip_S9p.png", 9, True)
    print(f"slot {GW}x{GH} labW=30")
    print("wrote", OUT_H)
    for ch, g in zip(ORDER, grids):
        print(f"  {ch!r} ink={sum(r.count('#') for r in g)} {'HAND' if ch in HAND else 'auto'}")


if __name__ == "__main__":
    main()
