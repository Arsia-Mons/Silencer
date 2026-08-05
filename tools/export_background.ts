// One-off: export a parallax level-background sprite bank to a single PNG.
// Replicates the sprite RLE+tile decode in
// clients/silencer/src/resources/resources.cpp (LoadSprites) and the
// palette load in clients/silencer/src/render/palette.cpp.
//
//   bun tools/export_background.ts <bank> [out.png]
//
// Backgrounds are banks 0..4 (parallax 0..4), each 240 frames laid out as a
// 20x12 grid of 64px tiles -> 1280x768. Palette for parallax N is block 5+N.

import { deflateSync } from "node:zlib";

const ASSETS = "shared/assets";
const bank = Number(process.argv[2] ?? "4");
const outPath = process.argv[3] ?? `bank${String(bank).padStart(3, "0")}_background.png`;

const GRID_W = 20;
const GRID_H = 12;
const TILE = 64;

// --- read the per-bank index: headers[256][64], sprite count at byte +2 ---
const spr = new Uint8Array(await Bun.file(`${ASSETS}/BIN_SPR.DAT`).bytes());
const count = spr[bank * 64 + 2];
if (!count) throw new Error(`bank ${bank} is empty (0 sprites)`);
console.log(`bank ${bank}: ${count} sprites`);

const binFile = `${ASSETS}/bin_spr/SPR_${String(bank).padStart(3, "0")}.BIN`;
const buf = new Uint8Array(await Bun.file(binFile).bytes());
const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);

const headerBytes = 344 * count + 4; // resources.cpp:62
let cursor = headerBytes; // byte cursor into the pixel-data region

function rdU32(off: number) {
  return dv.getUint32(off, true) >>> 0;
}
function wrU32(dst: Uint8Array, byteOff: number, v: number) {
  dst[byteOff] = v & 0xff;
  dst[byteOff + 1] = (v >>> 8) & 0xff;
  dst[byteOff + 2] = (v >>> 16) & 0xff;
  dst[byteOff + 3] = (v >>> 24) & 0xff;
}

function decodeSprite(j: number): { w: number; h: number; px: Uint8Array } {
  const base = j * 344;
  const w = dv.getUint16(base + 0, true);
  const h = dv.getUint16(base + 2, true);
  const size = rdU32(base + 12);
  const mode = buf[base + 20];
  const px = new Uint8Array(w * h);

  if (mode) {
    // tile mode: read u32s inline during 64x64 tile traversal
    let tempvalue = 0;
    let run = 0;
    for (let y2 = 0; y2 < (h + 63) >> 6; y2++) {
      for (let x2 = 0; x2 < (w + 63) >> 6; x2++) {
        const ymax = Math.min(y2 * 64 + 64, h);
        const xmax = Math.min(x2 * 64 + 64, w);
        for (let y = y2 * 64; y < ymax; y++) {
          for (let x = x2 * 64; x < xmax; x += 4) {
            if (run) {
              wrU32(px, y * w + x, tempvalue);
              run -= 4;
            } else {
              tempvalue = rdU32(cursor);
              cursor += 4;
              if (tempvalue >= 0xff000000) {
                run = tempvalue & 0x0000ffff;
                tempvalue &= 0x00ff0000;
                tempvalue = (tempvalue | (tempvalue << 8)) >>> 0;
                tempvalue = (tempvalue | (tempvalue >>> 16)) >>> 0;
                run -= 4;
              }
              wrU32(px, y * w + x, tempvalue);
            }
          }
        }
      }
    }
  } else {
    // linear mode: consume exactly `size` bytes
    let k = 0; // pixel index *4 handled below via byte offset
    const end = cursor + size;
    for (let p = cursor; p < end; p += 4) {
      let tempvalue = rdU32(p);
      if (tempvalue >= 0xff000000) {
        let run = tempvalue & 0x0000ffff;
        tempvalue &= 0x00ff0000;
        tempvalue = (tempvalue | (tempvalue << 8)) >>> 0;
        tempvalue = (tempvalue | (tempvalue >>> 16)) >>> 0;
        while (run) {
          wrU32(px, k * 4, tempvalue);
          run -= 4;
          k++;
        }
        k--;
      } else {
        wrU32(px, k * 4, tempvalue);
      }
      k++;
    }
    cursor = end;
  }
  return { w, h, px };
}

// --- palette block 5+(bank as parallax); colors stored 0..63, <<2 to 0..252 ---
const palBlock = 5 + bank; // parallax N -> palette 5+N (palette.cpp SetParallaxColors)
const palRaw = new Uint8Array(await Bun.file(`${ASSETS}/PALETTE.BIN`).bytes());
const palDv = new DataView(palRaw.buffer);
const pal = new Uint8Array(256 * 3);
{
  let off = palBlock * (768 + 4) + 4; // palette.cpp:44
  for (let i = 0; i < 256; i++) {
    const r = palRaw[off++],
      g = palRaw[off++],
      b = palRaw[off++];
    pal[i * 3] = r << 2;
    pal[i * 3 + 1] = g << 2;
    pal[i * 3 + 2] = b << 2;
  }
}
console.log(`palette block ${palBlock}`);

// --- assemble the 20x12 grid into one RGB image ---
const W = GRID_W * TILE;
const H = GRID_H * TILE;
const rgb = new Uint8Array(W * H * 3);
const histo = new Uint32Array(256);

for (let j = 0; j < Math.min(count, GRID_W * GRID_H); j++) {
  const { w, h, px } = decodeSprite(j);
  const gx = (j % GRID_W) * TILE;
  const gy = Math.floor(j / GRID_W) * TILE;
  for (let y = 0; y < h && y < TILE; y++) {
    for (let x = 0; x < w && x < TILE; x++) {
      const idx = px[y * w + x];
      histo[idx]++;
      const o = ((gy + y) * W + (gx + x)) * 3;
      rgb[o] = pal[idx * 3];
      rgb[o + 1] = pal[idx * 3 + 1];
      rgb[o + 2] = pal[idx * 3 + 2];
    }
  }
}

// index-range diagnostic (verifies background lives in the parallax range)
let lo = 256,
  hi = -1;
for (let i = 0; i < 256; i++) if (histo[i]) (lo = Math.min(lo, i)), (hi = Math.max(hi, i));
console.log(`pixel index range used: ${lo}..${hi}`);

// --- minimal PNG encoder (RGB, 8-bit, color type 2) ---
function crc32(b: Uint8Array): number {
  let c = ~0;
  for (let i = 0; i < b.length; i++) {
    c ^= b[i];
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
  }
  return ~c >>> 0;
}
function chunk(type: string, data: Uint8Array): Uint8Array {
  const t = new Uint8Array(4);
  for (let i = 0; i < 4; i++) t[i] = type.charCodeAt(i);
  const body = new Uint8Array(t.length + data.length);
  body.set(t);
  body.set(data, 4);
  const out = new Uint8Array(8 + body.length);
  const dvc = new DataView(out.buffer);
  dvc.setUint32(0, data.length);
  out.set(body, 4);
  dvc.setUint32(4 + body.length, crc32(body));
  return out;
}

// filter each row with filter type 0
const raw = new Uint8Array(H * (1 + W * 3));
for (let y = 0; y < H; y++) {
  raw[y * (1 + W * 3)] = 0;
  raw.set(rgb.subarray(y * W * 3, (y + 1) * W * 3), y * (1 + W * 3) + 1);
}
const idat = deflateSync(raw, { level: 9 });

const ihdr = new Uint8Array(13);
const ihdrDv = new DataView(ihdr.buffer);
ihdrDv.setUint32(0, W);
ihdrDv.setUint32(4, H);
ihdr[8] = 8; // bit depth
ihdr[9] = 2; // color type RGB
// 10,11,12 = 0 (deflate, adaptive filter, no interlace)

const sig = new Uint8Array([137, 80, 78, 71, 13, 10, 26, 10]);
const png = Buffer.concat([
  sig,
  chunk("IHDR", ihdr),
  chunk("IDAT", new Uint8Array(idat)),
  chunk("IEND", new Uint8Array(0)),
]);

await Bun.write(outPath, png);
console.log(`wrote ${outPath}  ${W}x${H}  (${png.length} bytes)`);
