#!/usr/bin/env python3
# Perceptual/tolerant pixdiff for cppx<->origin parity.
#
# Raw byte-exact tools/pixdiff is ~38% even at perfect visual parity because
# origin point-upscales its low-res framebuffer (scanline striping + uneven
# row-doubling) while cppx renders crisp. This downsamples both to a low res
# (killing that high-frequency upscale/AA noise) and reports the structural
# difference, which is what the user's "<1%, ideally 0.5%" target is about.
#
#   python3 tools/cap/pixdiff_tolerant.py <render.png> <golden.png>
#
# Prints: tol% (channel-bytes off by >TOL at low-res) and mae (mean abs error).
import sys
from PIL import Image
import numpy as np

TOL = 16          # per-channel tolerance after downscale
DW, DH = 320, 180  # low-res grid that removes upscale/AA striping

def main():
    if len(sys.argv) != 3:
        print("usage: pixdiff_tolerant.py <render.png> <golden.png>", file=sys.stderr)
        print("100.0")
        return
    a = Image.open(sys.argv[1]).convert("RGB").resize((DW, DH), Image.BOX)
    b = Image.open(sys.argv[2]).convert("RGB").resize((DW, DH), Image.BOX)
    a = np.asarray(a, dtype=int); b = np.asarray(b, dtype=int)
    d = np.abs(a - b)
    tol = (d > TOL).mean() * 100.0
    mae = d.mean()
    print(f"{tol:.4f}  (mae={mae:.2f})")

if __name__ == "__main__":
    main()
