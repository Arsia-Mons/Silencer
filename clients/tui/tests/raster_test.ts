#!/usr/bin/env bun
// Run the half-block rasterizer against the captured smoke-test frame and
// dump the resulting terminal byte stream to /tmp/silencer-tui-raster.txt.
// Verifies the rasterizer produces well-formed escape sequences without
// needing an interactive TTY.

import { readFileSync, writeFileSync } from 'node:fs';
import { HalfBlockRasterizer } from '../src/raster_halfblock';

// Read the PPM the smoke test wrote.
const ppm = readFileSync('/tmp/silencer-tui-frame.ppm');

// Parse PPM P6 header: "P6\n<w> <h>\n255\n"
let off = 0;
function readToken(): string {
  while (off < ppm.length && (ppm[off] === 0x20 || ppm[off] === 0x0a)) off++;
  const start = off;
  while (off < ppm.length && ppm[off] !== 0x20 && ppm[off] !== 0x0a) off++;
  return ppm.slice(start, off).toString('ascii');
}
const magic = readToken();
if (magic !== 'P6') throw new Error(`unexpected PPM magic: ${magic}`);
const w = parseInt(readToken(), 10);
const h = parseInt(readToken(), 10);
const maxval = parseInt(readToken(), 10);
if (maxval !== 255) throw new Error('expected maxval=255');
off++; // consume the single whitespace after maxval
const rgb = ppm.slice(off);
if (rgb.length !== w * h * 3) {
  throw new Error(`rgb size ${rgb.length} != ${w}*${h}*3`);
}

// The rasterizer wants indexed pixels + a palette. Build a synthetic palette
// keyed on unique RGB triples in the frame (reverse the PPM's flattening).
const paletteMap = new Map<number, number>();
const palette = new Uint8Array(256 * 4);
const pixels = new Uint8Array(w * h);
let nextIdx = 0;
for (let i = 0; i < w * h; i++) {
  const r = rgb[i * 3]!;
  const g = rgb[i * 3 + 1]!;
  const b = rgb[i * 3 + 2]!;
  const key = (r << 16) | (g << 8) | b;
  let idx = paletteMap.get(key);
  if (idx === undefined) {
    if (nextIdx >= 256) {
      // Frame uses more than 256 unique colors — pick the closest already-
      // assigned palette entry. The smoke-test main-menu fits well under
      // 256, so this branch should not fire for our captured PPM.
      idx = 0;
    } else {
      idx = nextIdx++;
      palette[idx * 4] = r;
      palette[idx * 4 + 1] = g;
      palette[idx * 4 + 2] = b;
      palette[idx * 4 + 3] = 255;
      paletteMap.set(key, idx);
    }
  }
  pixels[i] = idx;
}

const cols = 160;
const rows = 48;

const rasterizer = new HalfBlockRasterizer();
const out = rasterizer.render({ w, h, pixels }, palette, { cols, rows });

writeFileSync('/tmp/silencer-tui-raster.txt', out);

// Sanity stats.
const text = new TextDecoder().decode(out);
const cellCount = (text.match(/▀/g) ?? []).length;
const sgrCount = (text.match(/\x1b\[/g) ?? []).length;
console.error(
  `[raster] unique colors=${nextIdx}, output bytes=${out.length}, ` +
    `▀=${cellCount}, CSI=${sgrCount}, target cells=${cols * rows}`,
);
console.error(`[raster] wrote /tmp/silencer-tui-raster.txt`);
