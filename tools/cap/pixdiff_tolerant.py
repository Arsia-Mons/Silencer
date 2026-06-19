#!/usr/bin/env python3
# Tolerant pixdiff for cppx<->origin parity: global % + per-tile worst-case.
#
# Why not byte-exact at full res: origin point-upscales a low-res framebuffer
# (scanline striping) while cppx renders crisp, so raw pixdiff reads ~38% at
# perfect visual parity. Why not a plain global average: a shifted button or
# extra hairline is <0.2% of screen area and sails under any global threshold
# (calibrated 2026-06-11 — the old 320x180 global-only metric passed renders
# with 30px-misplaced buttons).
#
# Design (calibrated against injected defects vs noise proxies):
# - compare at 640x360 BOX: kills 1px-offset / resample-striping noise
#   (noise proxies max ~9.6%/tile) while real defects stay hot
#   (6px shift ~20%/tile, palette flatten ~61%/tile).
# - tile the working image into ~26px tiles (~80px regions at 1920x1080) and
#   report the worst tiles with full-res coords for direct inspection.
#
# Gate: PASS requires global < 1.0% AND no tile > 5% (user-tightened from 10%,
# 2026-06-11: prefer false FAILs over false PASSes). 5% catches typeface-drift
# (~6.4%/tile) but sits below the synthetic noise ceiling (~9.6%/tile resample
# proxy) — tiles in the 5-10% band can be renderer grain; adjudicate those by
# opening both images at the printed coords. 1px hairline errors (~2%/tile)
# remain below the floor — eyes + critic workflow own those.
#
#   python3 tools/cap/pixdiff_tolerant.py <render.png> <golden.png> [--mask x0,y0,x1,y1 ...]
#
# --mask (repeatable): zero the rect [x0,x1)x[y0,y1) (full-res input coords) in
# BOTH images before scoring — for known-nondeterministic regions (rain,
# minimap). No mask = unchanged behavior.
import argparse
import sys
from PIL import Image
import numpy as np

TOL = 16            # per-channel tolerance after downscale
MAX_DW, MAX_DH = 640, 360  # working-res cap: lowest that separates defects from noise
TILE = 26           # ~80px region at 1920x1080
MAX_TILE_PCT = 5.0  # any tile hotter than this = localized defect (or grain to adjudicate)
MAX_GLOBAL_PCT = 1.0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("render")
    ap.add_argument("golden")
    ap.add_argument("--mask", action="append", default=[],
                    help="x0,y0,x1,y1 full-res rect zeroed in both images (repeatable)")
    args = ap.parse_args()
    ra = Image.open(args.render).convert("RGB")
    ga = Image.open(args.golden).convert("RGB")
    fw, fh = ga.size
    if args.mask:
        rn = np.array(ra, dtype=np.uint8)
        gn = np.array(ga, dtype=np.uint8)
        for m in args.mask:
            x0, y0, x1, y1 = (int(v) for v in m.split(","))
            rn[y0:y1, x0:x1] = 0
            gn[y0:y1, x0:x1] = 0
        ra = Image.fromarray(rn)
        ga = Image.fromarray(gn)
    # Working res = min(input, 640x360): 1920x1080 menus collapse 3:1 as
    # calibrated; small inputs (640x480 in-game) never upscale.
    DW, DH = min(fw, MAX_DW), min(fh, MAX_DH)
    a = np.asarray(ra.resize((DW, DH), Image.BOX), dtype=int)
    b = np.asarray(ga.resize((DW, DH), Image.BOX), dtype=int)
    d = np.abs(a - b)
    glob = (d > TOL).mean() * 100.0
    mae = d.mean()

    off = d > TOL
    tiles = []
    for y in range(0, DH, TILE):
        for x in range(0, DW, TILE):
            pct = off[y:y+TILE, x:x+TILE].mean() * 100.0
            tiles.append((pct, x, y))
    tiles.sort(reverse=True)
    hot = [t for t in tiles if t[0] > MAX_TILE_PCT]

    sx, sy = fw / DW, fh / DH
    verdict = "PASS" if glob < MAX_GLOBAL_PCT and not hot else "FAIL"
    print(f"{glob:.4f}  (mae={mae:.2f} maxtile={tiles[0][0]:.1f}% hot_tiles={len(hot)} {verdict})")
    for pct, x, y in tiles[:5]:
        if pct <= 1.0:
            break
        print(f"  tile {pct:5.1f}%  @ {int(x*sx)},{int(y*sy)} "
              f"({int(TILE*sx)}x{int(TILE*sy)}px region at full res)")

if __name__ == "__main__":
    main()
