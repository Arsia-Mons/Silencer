// Export every "background-sized" sprite in the Silencer sprite banks to PNG,
// plus a self-contained HTML gallery.
//
//   bun tools/export_sprite_backgrounds.ts [outdir]
//
// Decoding mirrors Resources::LoadSprites in
// clients/silencer/src/resources/resources.cpp and the palette load in
// clients/silencer/src/render/palette.cpp. The per-bank palette page mapping
// mirrors `page_for_bank` in clients/silencer/src/game/ui/game_ui_pipeline.cpp
// (bank 0->5, 1->6, 2->7, 3->8, 4->9, 6->1, 7->2; every other bank -> page 0).
//
// Banks 0..4 are the parallax level backdrops: 240 sprites of 64x64 laid out as
// a 20x12 grid -> one 1280x768 image each. They're emitted as assembled grids.

import { deflateSync } from "node:zlib";
import { mkdirSync, writeFileSync, readFileSync } from "node:fs";
import { join } from "node:path";

const REPO = join(import.meta.dir, "..");
const ASSETS = join(REPO, "shared/assets");
const OUT = process.argv[2] ?? join(REPO, ".captures/sprite-backgrounds");

// A background is big enough to sit behind a whole menu/screen. Sprite areas
// cluster below 50k px (icons, actor frames, HUD widgets) and then jump to the
// 60k-307k px screen/panel artwork, so 55k px with both sides >= 90 lands in
// the empty band between the two groups.
const MIN_AREA = 55_000;
const MIN_SIDE = 90;

const PARALLAX_BANKS = [0, 1, 2, 3, 4];
const GRID_W = 20;
const GRID_H = 12;
const TILE = 64;

// Banks outside `page_for_bank`'s switch are drawn against whatever page the
// screen is on, so the base page (0) is the default. These few were resolved by
// scoring every page on mean adjacent-pixel color distance — the wrong page
// turns dithered art into rainbow speckle — and confirmed by eye.
const PAGE_OVERRIDE: Record<string, number> = {
  "8": 2, // rust-orange teleporter scene, matches the lobby cluster
  "208:0": 3, // WON.net splash has its own page
  "208:1": 1, // menu Mars backdrop
};

function pageForBank(bank: number, sprite = -1): number {
  const exact = PAGE_OVERRIDE[`${bank}:${sprite}`];
  if (exact !== undefined) return exact;
  const perBank = PAGE_OVERRIDE[String(bank)];
  if (perBank !== undefined) return perBank;
  if (bank >= 0 && bank <= 4) return 5 + bank;
  if (bank === 6) return 1;
  if (bank === 7) return 2;
  return 0;
}

// --- palette -----------------------------------------------------------------
const palRaw = new Uint8Array(readFileSync(join(ASSETS, "PALETTE.BIN")));
function readPalette(block: number): Uint8Array {
  const pal = new Uint8Array(256 * 3);
  let off = block * (768 + 4) + 4;
  for (let i = 0; i < 256; i++) {
    pal[i * 3] = palRaw[off++] << 2;
    pal[i * 3 + 1] = palRaw[off++] << 2;
    pal[i * 3 + 2] = palRaw[off++] << 2;
  }
  return pal;
}
const PALETTES = Array.from({ length: 11 }, (_, i) => readPalette(i));

// --- sprite bank decode ------------------------------------------------------
const counts = new Uint8Array(readFileSync(join(ASSETS, "BIN_SPR.DAT")));

type Sprite = { w: number; h: number; px: Uint8Array };

function decodeBank(bank: number): Sprite[] {
  const n = counts[bank * 64 + 2];
  if (!n) return [];
  const buf = new Uint8Array(readFileSync(join(ASSETS, `bin_spr/SPR_${String(bank).padStart(3, "0")}.BIN`)));
  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const rdU32 = (o: number) => dv.getUint32(o, true) >>> 0;
  const wrU32 = (dst: Uint8Array, o: number, v: number) => {
    dst[o] = v & 0xff;
    dst[o + 1] = (v >>> 8) & 0xff;
    dst[o + 2] = (v >>> 16) & 0xff;
    dst[o + 3] = (v >>> 24) & 0xff;
  };

  let cursor = 344 * n + 4; // pixel data starts right after the header block
  const out: Sprite[] = [];

  for (let j = 0; j < n; j++) {
    const base = j * 344;
    const w = dv.getUint16(base + 0, true);
    const h = dv.getUint16(base + 2, true);
    const size = rdU32(base + 12);
    const tiled = buf[base + 20];
    const px = new Uint8Array(w * h);

    if (tiled) {
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
      const end = cursor + size;
      let k = 0;
      for (let p = cursor; p < end; p += 4) {
        let tempvalue = rdU32(p);
        if (tempvalue >= 0xff000000) {
          let run = tempvalue & 0x0000ffff;
          tempvalue &= 0x00ff0000;
          tempvalue = (tempvalue | (tempvalue << 8)) >>> 0;
          tempvalue = (tempvalue | (tempvalue >>> 16)) >>> 0;
          while (run > 0) {
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
    out.push({ w, h, px });
  }
  return out;
}

// --- PNG encode (RGBA, index 0 = transparent) --------------------------------
function crc32(b: Uint8Array): number {
  let c = ~0;
  for (let i = 0; i < b.length; i++) {
    c ^= b[i];
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
  }
  return ~c >>> 0;
}
function chunk(type: string, data: Uint8Array): Uint8Array {
  const body = new Uint8Array(4 + data.length);
  for (let i = 0; i < 4; i++) body[i] = type.charCodeAt(i);
  body.set(data, 4);
  const out = new Uint8Array(8 + body.length);
  const dv = new DataView(out.buffer);
  dv.setUint32(0, data.length);
  out.set(body, 4);
  dv.setUint32(4 + body.length, crc32(body));
  return out;
}
function encodePng(w: number, h: number, rgba: Uint8Array): Uint8Array {
  const stride = w * 4;
  const raw = new Uint8Array(h * (1 + stride));
  for (let y = 0; y < h; y++) {
    raw[y * (1 + stride)] = 0;
    raw.set(rgba.subarray(y * stride, (y + 1) * stride), y * (1 + stride) + 1);
  }
  const idat = deflateSync(raw, { level: 9 });
  const ihdr = new Uint8Array(13);
  const dv = new DataView(ihdr.buffer);
  dv.setUint32(0, w);
  dv.setUint32(4, h);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 6; // RGBA
  const sig = new Uint8Array([137, 80, 78, 71, 13, 10, 26, 10]);
  const parts = [sig, chunk("IHDR", ihdr), chunk("IDAT", new Uint8Array(idat)), chunk("IEND", new Uint8Array(0))];
  const total = parts.reduce((a, p) => a + p.length, 0);
  const png = new Uint8Array(total);
  let o = 0;
  for (const p of parts) {
    png.set(p, o);
    o += p.length;
  }
  return png;
}

function toRgba(w: number, h: number, px: Uint8Array, pal: Uint8Array): Uint8Array {
  const rgba = new Uint8Array(w * h * 4);
  for (let i = 0; i < w * h; i++) {
    const idx = px[i];
    rgba[i * 4] = pal[idx * 3];
    rgba[i * 4 + 1] = pal[idx * 3 + 1];
    rgba[i * 4 + 2] = pal[idx * 3 + 2];
    rgba[i * 4 + 3] = idx === 0 ? 0 : 255; // index 0 is the transparent key
  }
  return rgba;
}

// --- collect -----------------------------------------------------------------
type Entry = {
  label: string;
  file: string;
  w: number;
  h: number;
  bank: number;
  idx: number; // -1 for assembled grids
  page: number;
  opaquePct: number;
  distinct: number;
  idxLo: number;
  idxHi: number;
};

mkdirSync(OUT, { recursive: true });
const entries: Entry[] = [];
const nearMisses: string[] = [];
const failures: string[] = [];

function stats(px: Uint8Array) {
  const seen = new Set<number>();
  let opaque = 0;
  let lo = 256;
  let hi = -1;
  for (let i = 0; i < px.length; i++) {
    const v = px[i];
    seen.add(v);
    if (v !== 0) {
      opaque++;
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
  }
  return { distinct: seen.size, opaquePct: (opaque / px.length) * 100, lo: lo === 256 ? 0 : lo, hi: hi < 0 ? 0 : hi };
}

for (let bank = 0; bank < 256; bank++) {
  if (!counts[bank * 64 + 2]) continue;
  let sprites: Sprite[];
  try {
    sprites = decodeBank(bank);
  } catch (e) {
    failures.push(`bank ${bank}: ${(e as Error).message}`);
    continue;
  }
  if (PARALLAX_BANKS.includes(bank)) {
    const page = pageForBank(bank);
    const pal = PALETTES[page];
    // assemble the 20x12 tile grid into one 1280x768 parallax backdrop
    const W = GRID_W * TILE;
    const H = GRID_H * TILE;
    const px = new Uint8Array(W * H);
    for (let j = 0; j < Math.min(sprites.length, GRID_W * GRID_H); j++) {
      const s = sprites[j];
      const gx = (j % GRID_W) * TILE;
      const gy = Math.floor(j / GRID_W) * TILE;
      for (let y = 0; y < Math.min(s.h, TILE); y++)
        for (let x = 0; x < Math.min(s.w, TILE); x++) px[(gy + y) * W + gx + x] = s.px[y * s.w + x];
    }
    const st = stats(px);
    const name = `bank${String(bank).padStart(3, "0")}_parallax.png`;
    writeFileSync(join(OUT, name), encodePng(W, H, toRgba(W, H, px, pal)));
    entries.push({
      label: `bank${String(bank).padStart(3, "0")} parallax`,
      file: name,
      w: W,
      h: H,
      bank,
      idx: -1,
      page,
      opaquePct: st.opaquePct,
      distinct: st.distinct,
      idxLo: st.lo,
      idxHi: st.hi,
    });
    continue;
  }

  for (let j = 0; j < sprites.length; j++) {
    const { w, h, px } = sprites[j];
    const area = w * h;
    if (area < MIN_AREA || Math.min(w, h) < MIN_SIDE) {
      if (area >= 25_000) nearMisses.push(`bank${String(bank).padStart(3, "0")} s${String(j).padStart(3, "0")} ${w}x${h} area=${area}`);
      continue;
    }
    const st = stats(px);
    const page = pageForBank(bank, j);
    const pal = PALETTES[page];
    const name = `bank${String(bank).padStart(3, "0")}_s${String(j).padStart(3, "0")}.png`;
    writeFileSync(join(OUT, name), encodePng(w, h, toRgba(w, h, px, pal)));
    entries.push({
      label: `bank${String(bank).padStart(3, "0")} s${String(j).padStart(3, "0")}`,
      file: name,
      w,
      h,
      bank,
      idx: j,
      page,
      opaquePct: st.opaquePct,
      distinct: st.distinct,
      idxLo: st.lo,
      idxHi: st.hi,
    });
  }
}

entries.sort((a, b) => a.bank - b.bank || a.idx - b.idx);

// --- gallery -----------------------------------------------------------------
const cards = entries
  .map((e) => {
    const b64 = readFileSync(join(OUT, e.file)).toString("base64");
    return `<section class="card" id="${e.label.replace(/[ ]/g, "-")}">
  <header>
    <h2>${e.label}</h2>
    <div class="meta"><span class="dim">${e.w} &times; ${e.h}</span><span>palette page ${e.page}</span><span>${e.opaquePct.toFixed(0)}% opaque</span><span>${e.distinct} colors</span><span class="file">${e.file}</span></div>
  </header>
  <div class="frame"><img alt="${e.label}" src="data:image/png;base64,${b64}"></div>
</section>`;
  })
  .join("\n");

const index = entries.map((e) => `<a href="#${e.label.replace(/ /g, "-")}">${e.label}</a>`).join("");

const html = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Silencer sprite backgrounds</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin: 0; padding: 24px; background: #101114; color: #e6e6e6;
         font: 14px/1.5 ui-sans-serif, system-ui, "Segoe UI", sans-serif; }
  h1 { font-size: 20px; margin: 0 0 4px; letter-spacing: .04em; }
  .sub { color: #8b8f98; margin: 0 0 20px; }
  .toc { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 28px; }
  .toc a { font: 12px/1 ui-monospace, "Cascadia Code", Consolas, monospace;
           color: #9fd39f; background: #191b20; border: 1px solid #2a2d35;
           padding: 6px 8px; border-radius: 4px; text-decoration: none; }
  .toc a:hover { background: #23262e; color: #d6ffd6; }
  .card { border: 1px solid #2a2d35; border-radius: 8px; background: #16181d;
          margin-bottom: 22px; overflow: hidden; }
  .card header { padding: 12px 16px; border-bottom: 1px solid #2a2d35; background: #1b1e24; }
  .card h2 { margin: 0; font: 700 22px/1.2 ui-monospace, "Cascadia Code", Consolas, monospace;
             color: #b7e5b7; letter-spacing: .06em; }
  .meta { display: flex; flex-wrap: wrap; gap: 14px; margin-top: 6px;
          font: 12px/1 ui-monospace, Consolas, monospace; color: #8b8f98; }
  .meta .dim { color: #e6c07b; }
  .meta .file { color: #5f636c; }
  .frame { padding: 16px; overflow-x: auto;
           background-color: #0b0c0e;
           background-image: linear-gradient(45deg, #17181c 25%, transparent 25%),
                             linear-gradient(-45deg, #17181c 25%, transparent 25%),
                             linear-gradient(45deg, transparent 75%, #17181c 75%),
                             linear-gradient(-45deg, transparent 75%, #17181c 75%);
           background-size: 16px 16px;
           background-position: 0 0, 0 8px, 8px -8px, -8px 0; }
  img { display: block; max-width: 100%; height: auto; image-rendering: pixelated; }
</style>
</head>
<body>
<h1>Silencer sprite backgrounds</h1>
<p class="sub">Every sprite at or above ${MIN_AREA.toLocaleString()} px area with both sides &ge; ${MIN_SIDE} px, decoded from
<code>shared/assets/bin_spr/SPR_*.BIN</code>. Banks 000&ndash;004 are the parallax level backdrops, assembled from their
240 &times; 64px tiles into one 1280&times;768 image. Transparent pixels (palette index 0) show the checkerboard.
Refer to an image by its label, e.g. &ldquo;use bank006 s000&rdquo;.</p>
<nav class="toc">${index}</nav>
${cards}
</body>
</html>
`;

writeFileSync(join(OUT, "gallery.html"), html);

console.log(`exported ${entries.length} images -> ${OUT}`);
for (const e of entries) {
  console.log(
    `  ${e.label.padEnd(18)} ${String(e.w).padStart(4)}x${String(e.h).padStart(4)}  page=${e.page}  opaque=${e.opaquePct.toFixed(0)}%  colors=${e.distinct}  idx=${e.idxLo}..${e.idxHi}`,
  );
}
if (nearMisses.length) {
  console.log(`\nnear misses (25k <= area < ${MIN_AREA} or thin):`);
  for (const m of nearMisses) console.log(`  ${m}`);
}
if (failures.length) {
  console.log(`\nfailed banks:`);
  for (const f of failures) console.log(`  ${f}`);
}
