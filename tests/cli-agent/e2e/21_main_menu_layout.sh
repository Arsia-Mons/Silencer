#!/usr/bin/env bash
# SIL-96: the MainMenu restores the origin/main design (starfield + SILENCER logo
# sprite + right-anchored staggered green oval buttons + version footer). It
# asserts the screen's structure via the retained cppx tree (inspect → nodes with
# role/label/bounds), that the composite is a real non-blank frame (headless UI
# compositing), and that the layout stays in-bounds + re-flows responsively.
# Exact legacy sprite-pixel placements are built with idiomatic flex, not pinned.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

OUT_DIR="$(mktemp -d)"

check_layout() {
  local width="$1" height="$2" inspect_out="$3" screenshot_out="$4"
  cli --port "$PORT" resize --w "$width" --h "$height" >/dev/null
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  cli --port "$PORT" inspect > "$inspect_out"
  cli --port "$PORT" screenshot --out "$screenshot_out" >/dev/null

  bun -e '
  import { inflateSync } from "node:zlib";
  import { readFileSync } from "node:fs";

  function readPng(path) {
    const bytes = new Uint8Array(readFileSync(path));
    const u32 = (o) => ((bytes[o] << 24) | (bytes[o+1] << 16) | (bytes[o+2] << 8) | bytes[o+3]) >>> 0;
    let width = 0, height = 0, bitDepth = 0, colorType = 0;
    const idat = [];
    for (let o = 8; o < bytes.length;) {
      const len = u32(o);
      const type = String.fromCharCode(...bytes.slice(o+4, o+8));
      if (type === "IHDR") { width = u32(o+8); height = u32(o+12); bitDepth = bytes[o+16]; colorType = bytes[o+17]; }
      else if (type === "IDAT") idat.push(bytes.slice(o+8, o+8+len));
      o += len + 12;
    }
    const bpp = colorType === 2 ? 3 : colorType === 6 ? 4 : 0;
    if (bitDepth !== 8 || bpp === 0) throw new Error(`unsupported PNG bitDepth=${bitDepth} colorType=${colorType}`);
    const raw = inflateSync(Buffer.concat(idat.map((c) => Buffer.from(c))));
    const stride = width * bpp;
    const pixels = new Uint8Array(width * height * bpp);
    const prev = new Uint8Array(stride), cur = new Uint8Array(stride);
    let p = 0;
    const paeth = (a,b,c)=>{const q=a+b-c,pa=Math.abs(q-a),pb=Math.abs(q-b),pc=Math.abs(q-c);return pa<=pb&&pa<=pc?a:pb<=pc?b:c;};
    for (let y = 0; y < height; y++) {
      const f = raw[p++];
      for (let x = 0; x < stride; x++) {
        const v = raw[p++];
        const left = x >= bpp ? cur[x-bpp] : 0, up = prev[x], ul = x >= bpp ? prev[x-bpp] : 0;
        cur[x] = f===0?v:f===1?(v+left)&255:f===2?(v+up)&255:f===3?(v+Math.floor((left+up)/2))&255:(v+paeth(left,up,ul))&255;
      }
      pixels.set(cur, y*stride); prev.set(cur);
    }
    return { width, height, bpp, pixels };
  }

  const [inspectPath, shotPath, vw, vh] = [process.argv[1], process.argv[2], Number(process.argv[3]), Number(process.argv[4])];

  // 1) The composite is a real, non-blank frame at the requested viewport size.
  const png = readPng(shotPath);
  if (png.width !== vw || png.height !== vh) {
    console.error(`screenshot size mismatch: expected ${vw}x${vh}, got ${png.width}x${png.height}`); process.exit(1);
  }
  let nonBlack = 0;
  for (let i = 0; i < png.width * png.height; i++) {
    const o = i * png.bpp;
    if (png.pixels[o] > 40 || png.pixels[o+1] > 40 || png.pixels[o+2] > 40) nonBlack++;
  }
  if (nonBlack < 2000) { console.error(`composite looks blank: nonBlack=${nonBlack}`); process.exit(1); }

  // 2) The retained tree exposes the three menu buttons, in order, in-bounds.
  const data = JSON.parse(readFileSync(inspectPath, "utf8"));
  const order = ["Tutorial", "Connect To Lobby", "Exit"];
  const buttons = order.map((label) =>
    (data.nodes ?? []).find((n) => n.role === "button" && n.label === label));
  if (buttons.some((b) => !b)) {
    console.error(`missing main-menu buttons: ${JSON.stringify(buttons)}`); process.exit(1);
  }
  for (const b of buttons) {
    if (!b.focusable) { console.error(`button not focusable: ${b.label}`); process.exit(1); }
    if (b.x < 0 || b.y < 0 || b.x + b.w > vw || b.y + b.h > vh) {
      console.error(`button out of bounds in ${vw}x${vh}: ${JSON.stringify(b)}`); process.exit(1);
    }
  }
  // Vertical stack with the legacy right-anchored horizontal stagger
  // (origin/main: −40/0/+40/0 normalized), so x varies within a small band.
  const [play, tut, quit] = buttons;
  if (!(play.y < tut.y && tut.y < quit.y)) {
    console.error(`buttons not stacked top-to-bottom: ${buttons.map((b)=>b.y)}`); process.exit(1);
  }
  const xs = buttons.map((b) => b.x);
  if (Math.max(...xs) - Math.min(...xs) > 60) {
    console.error(`buttons exceed the expected stagger band: ${xs}`); process.exit(1);
  }
  console.log(JSON.stringify({ vw, vh, nonBlack, play: [Math.round(play.x), Math.round(play.y)] }));
  ' "$inspect_out" "$screenshot_out" "$width" "$height"
}

SMALL="$OUT_DIR/inspect-640x480.json"
LARGE="$OUT_DIR/inspect-960x720.json"
check_layout 640 480 "$SMALL" "$OUT_DIR/main-640x480.png"
check_layout 960 720 "$LARGE" "$OUT_DIR/main-960x720.png"

# The menu re-centers with the viewport (idiomatic flex centering, not a
# pixel-pinned legacy layout): the larger viewport pushes the stack right + down.
bun -e '
import { readFileSync } from "node:fs";
const small = JSON.parse(readFileSync(process.argv[1], "utf8")).nodes;
const large = JSON.parse(readFileSync(process.argv[2], "utf8")).nodes;
const play = (ns) => ns.find((n) => n.role === "button" && n.label === "Connect To Lobby");
const s = play(small), l = play(large);
if (!(l.x > s.x + 50)) { console.error(`menu did not re-center horizontally: ${s.x} -> ${l.x}`); process.exit(1); }
if (!(l.y > s.y + 50)) { console.error(`menu did not re-center vertically: ${s.y} -> ${l.y}`); process.exit(1); }
' "$SMALL" "$LARGE"

echo "PASS 21_main_menu_layout ($OUT_DIR)"
