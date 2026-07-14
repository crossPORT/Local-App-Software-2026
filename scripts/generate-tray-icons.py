#!/usr/bin/env python3
"""Render cmake/icons from the tunnel-tray outline mark (box + capital R).

Mirrors apps/tunnel-tray/tray_icon_outline.cpp make_tray_mark() for light panels
(dark ink on transparent) so .exe / shortcuts match the tray.
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

# Light-panel ink from make_tray_mark(dark_panel=false): (22, 28, 38)
INK = (22, 28, 38, 255)


def set_px(px, w: int, h: int, x: int, y: int) -> None:
    if 0 <= x < w and 0 <= y < h:
        px[x, y] = INK


def fill_rect(px, w: int, h: int, x0: int, y0: int, x1: int, y1: int) -> None:
    if x1 < x0:
        x0, x1 = x1, x0
    if y1 < y0:
        y0, y1 = y1, y0
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            set_px(px, w, h, x, y)


def stroke_rect(px, w: int, h: int, x0: int, y0: int, x1: int, y1: int, t: int) -> None:
    for i in range(t):
        fill_rect(px, w, h, x0 + i, y0 + i, x1 - i, y0 + i)
        fill_rect(px, w, h, x0 + i, y1 - i, x1 - i, y1 - i)
        fill_rect(px, w, h, x0 + i, y0 + i, x0 + i, y1 - i)
        fill_rect(px, w, h, x1 - i, y0 + i, x1 - i, y1 - i)


def draw_letter_r(px, w: int, h: int, x0: int, y0: int, x1: int, y1: int) -> None:
    bw = max(1, x1 - x0 + 1)
    bh = max(1, y1 - y0 + 1)
    t = max(2, min(bw, bh) // 5)
    mid = y0 + bh * 48 // 100
    fill_rect(px, w, h, x0, y0, x0 + t - 1, y1)  # stem
    fill_rect(px, w, h, x0, y0, x1, y0 + t - 1)  # top
    fill_rect(px, w, h, x1 - t + 1, y0, x1, mid)  # bowl side
    fill_rect(px, w, h, x0, mid - t + 1, x1, mid)  # crossbar
    leg_h = max(1, y1 - mid)
    for row in range(leg_h + 1):
        x = x0 + t + row * (bw - 2 * t) // leg_h
        fill_rect(px, w, h, x, mid + row, min(x1, x + t - 1), mid + row)


def make_tray_mark(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = img.load()
    thick = max(2, size // 14)
    edge = max(thick + 1, size // 10)
    x0, y0 = edge, edge
    x1, y1 = size - 1 - edge, size - 1 - edge
    stroke_rect(px, size, size, x0, y0, x1, y1, thick)
    pad = max(thick + 2, size // 6)
    draw_letter_r(px, size, size, x0 + pad, y0 + pad, x1 - pad, y1 - pad)
    return img


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", type=Path, required=True)
    args = ap.parse_args()
    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    mark512 = make_tray_mark(512)
    mark256 = make_tray_mark(256)
    mark512.save(out / "rocketbox-mark-512.png")
    mark256.save(out / "rocketbox-256.png")
    print(f"Wrote {out / 'rocketbox-mark-512.png'} and {out / 'rocketbox-256.png'}")


if __name__ == "__main__":
    main()
