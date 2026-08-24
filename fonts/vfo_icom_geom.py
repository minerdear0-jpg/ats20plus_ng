#!/usr/bin/env python3
"""
Icom-inspired VFO digits: 16x24, 3 px stroke, open counters.
Not 7-segment. Geometry is capsules + ellipse rings.

SSD1306 bake is column-major, 3 pages, trimmed like Kenwood:
  [page0 cols][page1 cols][page2 cols]

Fixed 16 px blit slots (1 vs 8 must not slide the row).
Ink is ~14 px; 1 is narrow and centered in the slot.
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

W, H = 16, 24
TH = 3  # integer stroke

ROOT = Path(__file__).resolve().parent

# Shared 14×22 frame, 1 px bearing. Bars are inclusive.
XL, XR, YT, YB = 1, 12, 1, 20  # left/right bar origin, top/bottom bar origin


def blank():
    return [[0] * W for _ in range(H)]


def plot(g, x, y):
    if 0 <= x < W and 0 <= y < H:
        g[y][x] = 1


def hbar(g, y0, x0, x1, th=TH):
    for y in range(y0, y0 + th):
        for x in range(x0, x1 + 1):
            plot(g, x, y)


def vbar(g, x0, y0, y1, th=TH):
    for x in range(x0, x0 + th):
        for y in range(y0, y1 + 1):
            plot(g, x, y)


def chamfer(g, x, y):
    if 0 <= x < W and 0 <= y < H:
        g[y][x] = 0


def frame(g, x0, y0, x1, y1):
    """3 px ring. x0/y0 = outer top-left, x1/y1 = outer bottom-right inclusive."""
    hbar(g, y0, x0, x1)
    hbar(g, y1 - TH + 1, x0, x1)
    vbar(g, x0, y0, y1)
    vbar(g, x1 - TH + 1, y0, y1)
    chamfer(g, x0, y0)
    chamfer(g, x1, y0)
    chamfer(g, x0, y1)
    chamfer(g, x1, y1)


def digit_0():
    g = blank()
    frame(g, XL, YT, XL + 13, YB + 2)
    return g


def digit_1():
    g = blank()
    # Stem cols 6–8: optical centre of the 16 px slot.
    vbar(g, 6, 1, 22)
    plot(g, 5, 2)
    plot(g, 5, 3)
    plot(g, 4, 3)
    plot(g, 4, 4)
    plot(g, 5, 4)
    return g


def digit_2():
    g = blank()
    hbar(g, 1, XL, XL + 13)
    vbar(g, XR, 1, 7)
    # Neck starts high, under the cap — not down at the base.
    x = 11
    for y in range(8, 16):
        vbar(g, x, y, y, th=4)
        x -= 1
    # Last neck row is cols 4–7. Flatten left into a foot, 2 px air above the base.
    vbar(g, 2, 16, 16, th=5)
    vbar(g, XL, 17, 19, th=4)
    hbar(g, 20, XL, XL + 13)
    chamfer(g, XL, 1)
    chamfer(g, XL + 13, 1)
    chamfer(g, XL, 22)
    chamfer(g, XL + 13, 22)
    return g


def digit_3():
    g = blank()
    hbar(g, 1, XL, XL + 13)
    hbar(g, 10, 5, XL + 13)
    hbar(g, 20, XL, XL + 13)
    vbar(g, XR, 1, 22)
    chamfer(g, XL, 1)
    chamfer(g, XL + 13, 1)
    chamfer(g, XL, 22)
    chamfer(g, XL + 13, 22)
    return g


def digit_4():
    g = blank()
    vbar(g, XL, 1, 14)
    hbar(g, 12, XL, XL + 13)
    vbar(g, XR, 1, 22)
    return g


def digit_5():
    g = blank()
    hbar(g, 1, XL, XL + 13)
    vbar(g, XL, 1, 12)
    hbar(g, 10, XL, XR + 2)
    vbar(g, XR, 12, 22)
    hbar(g, 20, XL, XL + 13)
    chamfer(g, XL, 1)
    chamfer(g, XL + 13, 1)
    chamfer(g, XL, 22)
    chamfer(g, XL + 13, 22)
    return g


def digit_6():
    g = blank()
    hbar(g, 1, XL, XL + 13)
    vbar(g, XL, 1, 22)
    hbar(g, 10, XL, XL + 13)
    vbar(g, XR, 10, 22)
    hbar(g, 20, XL, XL + 13)
    chamfer(g, XL, 1)
    chamfer(g, XL + 13, 1)
    chamfer(g, XL, 22)
    chamfer(g, XL + 13, 22)
    return g


def digit_7():
    g = blank()
    hbar(g, 1, XL, XL + 13)
    # Mild stair, not a 1.5 px disk slash.
    x = 12
    for y in range(4, 23):
        vbar(g, x, y, y)
        if (y - 4) % 3 == 2 and x > 5:
            x -= 1
    return g


def digit_8():
    g = blank()
    hbar(g, 1, XL, XL + 13)
    hbar(g, 10, XL, XL + 13)  # shared 3 px waist
    hbar(g, 20, XL, XL + 13)
    vbar(g, XL, 1, 22)
    vbar(g, XR, 1, 22)
    chamfer(g, XL, 1)
    chamfer(g, XL + 13, 1)
    chamfer(g, XL, 22)
    chamfer(g, XL + 13, 22)
    return g


def digit_9():
    g = blank()
    hbar(g, 1, XL, XL + 13)
    vbar(g, XL, 1, 12)
    hbar(g, 10, XL, XL + 13)
    vbar(g, XR, 1, 22)
    hbar(g, 20, XL, XL + 13)
    chamfer(g, XL, 1)
    chamfer(g, XL + 13, 1)
    chamfer(g, XL, 22)
    chamfer(g, XL + 13, 22)
    return g


DIGITS = [
    digit_0, digit_1, digit_2, digit_3, digit_4,
    digit_5, digit_6, digit_7, digit_8, digit_9,
]


def ascii_art(g, ch="."):
    rows = []
    for y in range(H):
        rows.append("".join(ch if p else "." for p in g[y]))
    return "\n".join(rows)


def ink_bbox(g):
    xs = [x for y in range(H) for x in range(W) if g[y][x]]
    if not xs:
        return 0, 0
    return min(xs), max(xs) - min(xs) + 1


def ssd1306_pages(g):
    pages = []
    for page in range(3):
        cols = []
        for x in range(W):
            b = 0
            for bit in range(8):
                y = page * 8 + bit
                if g[y][x]:
                    b |= 1 << bit
            cols.append(b)
        pages.append(cols)
    return pages


def write_png(path, img_w, img_h, rgb_fn):
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
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(
        b"IDAT", zlib.compress(bytes(raw), 9)
    ) + chunk(b"IEND", b"")
    path.write_bytes(png)


def render_sheet(path, grids, scale=6, gap=2, cell_w=None):
    cw = W if cell_w is None else cell_w
    n = len(grids)
    img_w = n * (cw + gap) * scale
    img_h = H * scale

    def pix(x, y):
        cell = x // ((cw + gap) * scale)
        if cell >= n:
            return (12, 12, 12)
        lx = (x // scale) - cell * (cw + gap)
        ly = y // scale
        if lx < 0 or lx >= cw or ly < 0 or ly >= H:
            return (12, 12, 12)
        if lx >= W:
            return (12, 12, 12)
        on = grids[cell][ly][lx]
        return (236, 248, 236) if on else (10, 12, 10)

    write_png(path, img_w, img_h, pix)


def bake_header(grids):
    widths = []
    offsets = []
    blobs = []
    running = 0
    byte_offs = []
    for g in grids:
        pages = ssd1306_pages(g)
        used = [c for c in range(W) if any(pages[p][c] for p in range(3))]
        off = min(used)
        w = max(used) - min(used) + 1
        # Center ink in the 16 px slot so a narrow 1 does not sit on the right.
        pad = (W - w) // 2
        offsets.append(pad)
        widths.append(w)
        byte_offs.append(running)
        blob = []
        for p in range(3):
            blob.extend(pages[p][off : off + w])
        blobs.append(blob)
        running += len(blob)

    lines = [
        "// ----------------------------------------------------------------------",
        "// Icom-inspired VFO digits for ATS_EX, 16x24, 3 px stroke.",
        "// Integer 3 px bars, 1 px corner cut. No disk r=1.5.",
        "// fonts/vfo_icom_geom.py — edit geometry there, do not hand-patch bytes.",
        "// SSD1306 column-major, 3 pages. Blit into a 16 px slot (kFreqColOffKW).",
        f"// Glyph data: {running} bytes PROGMEM.",
        "// ----------------------------------------------------------------------",
        "",
        "#include <avr/pgmspace.h>",
        "",
        "const uint8_t ssd1306xled_font16x24vfoIcom [] PROGMEM =",
        "{",
    ]
    for d, blob in enumerate(blobs):
        hx = ",".join(f"0x{b:02X}" for b in blob)
        lines.append(f"\t{hx},  // {d}")
    lines += [
        "};",
        "",
        "const uint8_t kFreqColOffKW[] PROGMEM = { "
        + ", ".join(str(o) for o in offsets)
        + " };",
        "const uint8_t kFreqWidthKW[] PROGMEM = { "
        + ", ".join(str(w) for w in widths)
        + " };",
        "const uint16_t kKenwoodOff[] PROGMEM = { "
        + ", ".join(str(o) for o in byte_offs)
        + " };",
        "#define FREQ_DOT_W 4",
        "const uint8_t kFreqDot[] PROGMEM = {",
        "\t0x00, 0x00, 0x00, 0x00,",
        "\t0x00, 0x00, 0x00, 0x00,",
        "\t0x00, 0x30, 0x30, 0x00",
        "};",
        "",
    ]
    text = "\n".join(lines)
    (ROOT / "font16x24vfoIcom.h").write_text(text)
    (ROOT.parent / "ATS_EX" / "font16x24vfoIcom.h").write_text(text)
    return widths, offsets, running


def main():
    grids = [fn() for fn in DIGITS]
    print("ink offset,width:")
    for i, g in enumerate(grids):
        o, w = ink_bbox(g)
        print(f"  {i}: off={o} w={w}")
        print(ascii_art(g, "#"))
        print()
    glyph_dir = ROOT / "vfo_glyphs"
    glyph_dir.mkdir(exist_ok=True)
    for i, g in enumerate(grids):
        def pix(x, y, gg=g):
            return (0, 0, 0) if gg[y][x] else (255, 255, 255)

        write_png(glyph_dir / f"{i}.png", W, H, pix)
    d = blank()
    d[20][1] = d[20][2] = d[21][1] = d[21][2] = 1

    def pix_dot(x, y):
        return (0, 0, 0) if d[y][x] else (255, 255, 255)

    write_png(glyph_dir / "dot.png", 4, H, lambda x, y: pix_dot(x, y) if x < 4 else (255, 255, 255))
    # 16 px slots + 4 px gap, same as idle VFO (1 vs 8 does not slide).
    sample = "14.074.00"
    sgrids = []
    for c in sample:
        if c == ".":
            d = blank()
            d[20][1] = d[20][2] = d[21][1] = d[21][2] = 1
            sgrids.append(d)
        else:
            sgrids.append(grids[int(c)])
    render_sheet(ROOT / "vfo_icom_freq.png", sgrids, gap=4)
    w, o, nby = bake_header(grids)
    print("widths", w)
    print("offsets", o)
    print("bytes", nby)


if __name__ == "__main__":
    main()
