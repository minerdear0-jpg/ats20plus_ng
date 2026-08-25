#!/usr/bin/env python3
"""
S-meter: thin S from U8x8 7x14, bold 0-9/+ from 7x14B (X11 misc-fixed).
Baked into ATS_EX/font7x14smeter.h — do not hand-patch bytes.

To re-bake, clone olikraus/u8g2 next to this repo (or set U8G2) and run this
script. The full u8g2 tree is not kept in ATS-20Plus_next (~500 MB).
"""
from __future__ import annotations

import os
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
OUT_H = REPO / "ATS_EX" / "font7x14smeter.h"
U8X8 = Path(os.environ.get("U8G2", REPO / "u8g2")) / "csrc" / "u8x8_fonts.c"
GW, GH = 7, 16
CHARS = [chr(c) for c in range(ord("0"), ord("9") + 1)] + ["S", "+", " "]


def parse_c_strings(s: str) -> bytes:
    out = bytearray()
    i = 0
    n = len(s)
    while i < n:
        if s[i] != '"':
            i += 1
            continue
        i += 1
        while i < n and s[i] != '"':
            if s[i] == "\\":
                i += 1
                c = s[i]
                if c in "01234567":
                    octal = c
                    if i + 1 < n and s[i + 1] in "01234567":
                        i += 1
                        octal += s[i]
                        if i + 1 < n and s[i + 1] in "01234567":
                            i += 1
                            octal += s[i]
                    out.append(int(octal, 8) & 0xFF)
                elif c == "n":
                    out.append(10)
                elif c == "t":
                    out.append(9)
                elif c == "r":
                    out.append(13)
                else:
                    out.append(ord(c))
                i += 1
            else:
                out.append(ord(s[i]))
                i += 1
        i += 1
    return bytes(out)


def load_u8x8(name: str) -> bytes:
    if not U8X8.is_file():
        raise SystemExit(
            f"missing {U8X8}\nclone https://github.com/olikraus/u8g2.git or set U8G2="
        )
    src = U8X8.read_text(encoding="latin-1")
    m = re.search(
        rf"const uint8_t {name}\[1524\].*?=\s*(.*?);", src, re.S
    )
    if not m:
        raise SystemExit(f"{name} not found")
    data = parse_c_strings(m.group(1))
    if data[0] != 32 or data[2] != 1 or data[3] != 2:
        raise SystemExit(f"unexpected u8x8 header for {name}")
    return data


def glyph8(font: bytes, ch: str) -> bytes:
    first = font[0]
    off = 4 + (ord(ch) - first) * 16
    return font[off : off + 16]


def cols7(g8: bytes) -> list[int]:
    out = []
    for page in range(2):
        for x in range(GW):
            out.append(g8[page * 8 + x])
    return out


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
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def main():
    thin = load_u8x8("u8x8_font_7x14_1x2_r")
    bold = load_u8x8("u8x8_font_7x14B_1x2_r")
    tables = []
    for ch in CHARS:
        src = thin if ch == "S" else bold
        tables.append(cols7(glyph8(src, ch)))
    lines = [
        "// ----------------------------------------------------------------------",
        "// S-meter 7x14: S thin, 0-9/+ bold (U8x8 / X11 misc-fixed). Slots 7 px.",
        "// fonts/smeter_7x14.py — do not hand-patch bytes.",
        f"// Glyph data: {len(CHARS) * GW * 2} bytes PROGMEM.",
        "// ----------------------------------------------------------------------",
        "",
        "#include <avr/pgmspace.h>",
        "",
        "#define SMETER_7X14_W 7",
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

    scale, gap = 6, 2
    n = len(CHARS)
    img_w = n * (GW + gap) * scale
    img_h = GH * scale
    grids = []
    for cols in tables:
        g = [[0] * GW for _ in range(GH)]
        for page in range(2):
            for x in range(GW):
                b = cols[page * GW + x]
                for bit in range(8):
                    if b & (1 << bit):
                        g[page * 8 + bit][x] = 1
        grids.append(g)

    def pix(x, y):
        cell = x // ((GW + gap) * scale)
        if cell >= n:
            return (12, 12, 12)
        lx = (x // scale) - cell * (GW + gap)
        ly = y // scale
        if lx < 0 or lx >= GW or ly < 0 or ly >= GH:
            return (12, 12, 12)
        return (240, 240, 240) if grids[cell][ly][lx] else (20, 20, 20)

    write_png(ROOT / "smeter_7x14.png", img_w, img_h, pix)
    print("wrote", OUT_H, "and", ROOT / "smeter_7x14.png")


if __name__ == "__main__":
    main()
