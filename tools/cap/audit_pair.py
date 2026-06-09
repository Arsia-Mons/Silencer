#!/usr/bin/env python3
# Audit helper for the 2K visual-parity loop.
#
#   python3 tools/cap/audit_pair.py side <name>            -> /tmp/parity_audit/<name>_sbs.png (golden | render, full res)
#   python3 tools/cap/audit_pair.py crop <name> x y w h    -> golden+render crops upscaled 3x, side by side
#   python3 tools/cap/audit_pair.py heat <name>            -> downscaled tolerant-diff heatmap (where the pixels differ)
#
# Golden:  tests/cli-agent/e2e/golden/<name>.png
# Render:  /tmp/cppx_renders/<name>.png
import sys, os
from PIL import Image, ImageDraw
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GOLDEN = os.path.join(ROOT, "tests/cli-agent/e2e/golden")
RENDERS = "/tmp/cppx_renders"
OUT = "/tmp/parity_audit"

def load(name):
    g = Image.open(f"{GOLDEN}/{name}.png").convert("RGB")
    r = Image.open(f"{RENDERS}/{name}.png").convert("RGB")
    return g, r

def main():
    os.makedirs(OUT, exist_ok=True)
    mode, name = sys.argv[1], sys.argv[2]
    g, r = load(name)
    if mode == "side":
        w, h = g.size
        canvas = Image.new("RGB", (w * 2 + 4, h), (40, 40, 40))
        canvas.paste(g, (0, 0)); canvas.paste(r, (w + 4, 0))
        p = f"{OUT}/{name}_sbs.png"; canvas.save(p); print(p)
    elif mode == "crop":
        x, y, w, h = map(int, sys.argv[3:7])
        s = 3
        gc = g.crop((x, y, x + w, y + h)).resize((w * s, h * s), Image.NEAREST)
        rc = r.crop((x, y, x + w, y + h)).resize((w * s, h * s), Image.NEAREST)
        canvas = Image.new("RGB", (w * s * 2 + 4, h * s), (40, 40, 40))
        canvas.paste(gc, (0, 0)); canvas.paste(rc, (w * s + 4, 0))
        p = f"{OUT}/{name}_crop_{x}_{y}.png"; canvas.save(p); print(p)
    elif mode == "heat":
        DW, DH = 480, 270
        a = np.asarray(g.resize((DW, DH), Image.BOX), dtype=int)
        b = np.asarray(r.resize((DW, DH), Image.BOX), dtype=int)
        d = np.abs(a - b).max(axis=2)
        heat = np.zeros((DH, DW, 3), dtype=np.uint8)
        heat[..., 0] = np.clip(d * 2, 0, 255)
        heat[..., 1] = np.where(d > 16, 0, np.asarray(b.mean(axis=2), dtype=np.uint8) // 2)
        img = Image.fromarray(heat).resize((DW * 2, DH * 2), Image.NEAREST)
        p = f"{OUT}/{name}_heat.png"; img.save(p); print(p)

if __name__ == "__main__":
    main()
