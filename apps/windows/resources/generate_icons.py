#!/usr/bin/env python3
"""Generate the MxU Slides app mark as a PNG and a Windows ICO.

The artwork mirrors apps/windows/src/ui/icons.cpp so the packaged executable,
taskbar, and in-app brand mark agree. Uses only the Python standard library.
"""
from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

SIZE = 256
SS = 3
W = SIZE * SS


def rounded_rect_distance(x: float, y: float, cx: float, cy: float,
                          half_w: float, half_h: float, radius: float) -> float:
    qx = abs(x - cx) - half_w + radius
    qy = abs(y - cy) - half_h + radius
    return min(max(qx, qy), 0.0) + math.hypot(max(qx, 0.0), max(qy, 0.0)) - radius


def inside_triangle(px: float, py: float, a: tuple[float, float],
                    b: tuple[float, float], c: tuple[float, float]) -> bool:
    def cross(p1, p2, p3):
        return (p2[0] - p1[0]) * (p3[1] - p1[1]) - (p2[1] - p1[1]) * (p3[0] - p1[0])
    d1 = cross(a, b, (px, py))
    d2 = cross(b, c, (px, py))
    d3 = cross(c, a, (px, py))
    neg = d1 < 0 or d2 < 0 or d3 < 0
    pos = d1 > 0 or d2 > 0 or d3 > 0
    return not (neg and pos)


def blend(dst, src):
    sa = src[3] / 255.0
    da = dst[3] / 255.0
    out_a = sa + da * (1 - sa)
    if out_a <= 0:
        return (0, 0, 0, 0)
    out = []
    for i in range(3):
        out.append(int((src[i] * sa + dst[i] * da * (1 - sa)) / out_a))
    return (out[0], out[1], out[2], int(out_a * 255))


def paint_pixel(x: float, y: float):
    s = W
    radius = s * 0.22
    if rounded_rect_distance(x, y, s / 2, s / 2, s / 2, s / 2, radius) > 0:
        return (0, 0, 0, 0)

    t = min(1.0, max(0.0, (x + y) / (2 * s)))
    start = (27, 45, 74)
    end = (10, 15, 24)
    color = tuple(int(start[i] + (end[i] - start[i]) * t) for i in range(3)) + (255,)

    # Two overlapping slide frames.
    thickness = max(2.0, s * 0.026)
    frame1 = rounded_rect_distance(x, y, s * (0.24 + 0.20), s * (0.27 + 0.14),
                                   s * 0.20, s * 0.14, s * 0.035)
    if abs(frame1) <= thickness:
        color = blend(color, (238, 242, 248, 225))
    frame2 = rounded_rect_distance(x, y, s * (0.36 + 0.20), s * (0.43 + 0.14),
                                   s * 0.20, s * 0.14, s * 0.035)
    if abs(frame2) <= thickness:
        color = blend(color, (91, 141, 239, 255))

    cx, cy = s * 0.64, s * 0.52
    a = (cx - s * 0.055, cy - s * 0.075)
    b = (cx + s * 0.095, cy)
    c = (cx - s * 0.055, cy + s * 0.075)
    if inside_triangle(x, y, a, b, c):
        color = blend(color, (91, 141, 239, 255))
    return color


def build_image():
    hi = []
    for y in range(W):
        row = []
        for x in range(W):
            row.append(paint_pixel(x + 0.5, y + 0.5))
        hi.append(row)

    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            r = g = b = a = 0
            for dy in range(SS):
                for dx in range(SS):
                    pr, pg, pb, pa = hi[y * SS + dy][x * SS + dx]
                    r += pr * pa
                    g += pg * pa
                    b += pb * pa
                    a += pa
            n = SS * SS
            if a:
                row.append((r // a, g // a, b // a, a // n))
            else:
                row.append((0, 0, 0, 0))
        pixels.append(row)
    return pixels


def chunk(tag: bytes, data: bytes) -> bytes:
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def png_bytes(pixels):
    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for px in row:
            raw.extend(px)
    ihdr = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def ico_bytes(png: bytes) -> bytes:
    header = struct.pack("<HHH", 0, 1, 1)
    entry = struct.pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(png), 22)
    return header + entry + png


def main():
    out = Path(__file__).resolve().parent / "icons"
    out.mkdir(parents=True, exist_ok=True)
    pixels = build_image()
    png = png_bytes(pixels)
    (out / "app-256.png").write_bytes(png)
    (Path(__file__).resolve().parent / "app.ico").write_bytes(ico_bytes(png))
    print(f"Wrote {out / 'app-256.png'} and {Path(__file__).resolve().parent / 'app.ico'}")


if __name__ == "__main__":
    main()
