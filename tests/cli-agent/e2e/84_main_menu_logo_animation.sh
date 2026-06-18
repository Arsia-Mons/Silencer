#!/usr/bin/env bash
# SIL-195: the main-menu SILENCER logo must play the full origin/main
# bank-208 frame sequence at native per-frame size inside a fixed union stage.
# Sparse 8-frame/stretched playback produced 15 distinct crop hashes in the
# repro span; the full 29..60 sequence should clear this threshold comfortably.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

OUT_DIR="${SIL195_OUT_DIR:-/tmp/sil195_after/logo_animation}"
CAPTURES="${SIL195_LOGO_CAPTURE_COUNT:-72}"
DELAY_MS="${SIL195_LOGO_CAPTURE_DELAY_MS:-120}"
# The pre-fix sparse reveal produced 15 distinct crops in this same capture span.
# Keep margin above that without depending on exact screenshot-loop timing.
THRESHOLD="${SIL195_LOGO_DISTINCT_THRESHOLD:-18}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" resize --w 1280 --h 720 >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null

for i in $(seq 0 $((CAPTURES - 1))); do
  printf -v idx "%03d" "$i"
  cli --port "$PORT" screenshot --out "$OUT_DIR/frame_$idx.png" >/dev/null
  cli --port "$PORT" wait_ms --n "$DELAY_MS" >/dev/null
done

bun -e '
import { readdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { inflateSync } from "node:zlib";

function readPng(path) {
  const bytes = new Uint8Array(readFileSync(path));
  const u32 = (o) => ((bytes[o] << 24) | (bytes[o + 1] << 16) | (bytes[o + 2] << 8) | bytes[o + 3]) >>> 0;
  let width = 0, height = 0, bitDepth = 0, colorType = 0;
  const idat = [];
  for (let o = 8; o < bytes.length;) {
    const len = u32(o);
    const type = String.fromCharCode(...bytes.slice(o + 4, o + 8));
    if (type === "IHDR") {
      width = u32(o + 8);
      height = u32(o + 12);
      bitDepth = bytes[o + 16];
      colorType = bytes[o + 17];
    } else if (type === "IDAT") {
      idat.push(bytes.slice(o + 8, o + 8 + len));
    }
    o += len + 12;
  }
  const bpp = colorType === 2 ? 3 : colorType === 6 ? 4 : 0;
  if (bitDepth !== 8 || !bpp)
    throw new Error(`unsupported PNG bitDepth=${bitDepth} colorType=${colorType}`);
  const raw = inflateSync(Buffer.concat(idat.map((c) => Buffer.from(c))));
  const stride = width * bpp;
  const pixels = new Uint8Array(width * height * bpp);
  const prev = new Uint8Array(stride);
  const cur = new Uint8Array(stride);
  let p = 0;
  const paeth = (a, b, c) => {
    const q = a + b - c;
    const pa = Math.abs(q - a), pb = Math.abs(q - b), pc = Math.abs(q - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
  };
  for (let y = 0; y < height; y++) {
    const f = raw[p++];
    for (let x = 0; x < stride; x++) {
      const v = raw[p++];
      const left = x >= bpp ? cur[x - bpp] : 0;
      const up = prev[x];
      const ul = x >= bpp ? prev[x - bpp] : 0;
      cur[x] = f === 0 ? v
             : f === 1 ? (v + left) & 255
             : f === 2 ? (v + up) & 255
             : f === 3 ? (v + Math.floor((left + up) / 2)) & 255
             : (v + paeth(left, up, ul)) & 255;
    }
    pixels.set(cur, y * stride);
    prev.set(cur);
  }
  return { width, height, bpp, pixels };
}

function hashCrop(png, crop) {
  let h = 2166136261 >>> 0;
  for (let y = crop.y; y < crop.y + crop.h; y++) {
    for (let x = crop.x; x < crop.x + crop.w; x++) {
      const o = (y * png.width + x) * png.bpp;
      for (let c = 0; c < 3; c++)
        h = Math.imul((h ^ png.pixels[o + c]) >>> 0, 16777619) >>> 0;
    }
  }
  return h.toString(16).padStart(8, "0");
}

const dir = process.argv[1];
const threshold = Number(process.argv[2]);
const crop = { x: 160, y: 230, w: 500, h: 260 };
const files = readdirSync(dir).filter((f) => /^frame_[0-9]+\.png$/.test(f)).sort();
if (!files.length)
  throw new Error("no logo animation screenshots captured");
const hashes = files.map((file) => hashCrop(readPng(join(dir, file)), crop));
const distinct = new Set(hashes).size;
writeFileSync(join(dir, "hashes.txt"), hashes.map((h, i) => `${files[i]} ${h}`).join("\n") + "\n");
writeFileSync(join(dir, "summary.json"), JSON.stringify({
  captures: files.length,
  crop,
  distinct_logo_crops: distinct,
  threshold,
}, null, 2) + "\n");
console.log(JSON.stringify({ captures: files.length, distinct_logo_crops: distinct, threshold, crop }));
if (distinct < threshold) {
  console.error(`logo animation too sparse: distinct=${distinct}, threshold=${threshold}`);
  process.exit(1);
}
' "$OUT_DIR" "$THRESHOLD"

echo "PASS 84_main_menu_logo_animation ($OUT_DIR)"
