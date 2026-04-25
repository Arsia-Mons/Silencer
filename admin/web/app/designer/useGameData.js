'use client';
import { useState, useCallback } from 'react';
import { ACTOR_DEFS } from './Toolbar.js';

// Sprite banks needed for actor rendering (from ACTOR_DEFS, plus terminal big variant)
const NEEDED_SPRITE_BANKS = new Set([
  ...ACTOR_DEFS.map(a => a.bank).filter(b => b != null),
  184,               // big terminal (type=1 variant of actor 54)
  200, 201, 205,     // powerup variants (super shield, jet pack, default)
  49, 50, 51, 52, 53, 54, 55, 56, 57, 58, // doodad variants (actor 47)
]);

// Palette stride: 772 bytes per palette (768 color bytes + 4 padding), header 4 bytes
// Colors are VGA 0-63 range, scaled <<2 to 0-252
function getPalette(palBytes, palIndex) {
  const pal = new Uint8Array(256 * 3);
  const base = 4 + palIndex * 772;
  for (let i = 0; i < 256; i++) {
    pal[i * 3]     = palBytes[base + i * 3]     << 2;
    pal[i * 3 + 1] = palBytes[base + i * 3 + 1] << 2;
    pal[i * 3 + 2] = palBytes[base + i * 3 + 2] << 2;
  }
  return pal;
}

// Which palette index each sprite bank uses
function spritePaletteIndex(bankNum) {
  switch (bankNum) {
    case 0: return 5;
    case 1: return 6;
    case 2: return 7;
    case 3: return 8;
    case 6: return 1;
    case 7: return 2;
    default: return 0;
  }
}

// Flat RLE decompressor (same algorithm as tiles, arbitrary output size)
function decompressSpriteFlat(bankData, dataPos, size, width, height) {
  const srcDV = new DataView(bankData.buffer, bankData.byteOffset + dataPos, size);
  const out = new Uint8Array(width * height);
  const outDV = new DataView(out.buffer);
  let k = 0;
  for (let j = 0; j < size / 4; j++, k++) {
    const val = srcDV.getUint32(j * 4, true);
    if (val >= 0xFF000000) {
      let cnt = val & 0x0000FFFF;
      const b = (val >>> 16) & 0xFF;
      const word = b | (b << 8) | (b << 16) | (b << 24);
      while (cnt > 0) {
        outDV.setUint32(k * 4, word, true);
        cnt -= 4;
        k++;
      }
      k--;
    } else {
      outDV.setUint32(k * 4, val, true);
    }
  }
  return out;
}

// Tiled RLE decompressor: reads in 64×64 block order, shared state across blocks
// Returns { pixels, newPos } where newPos is byte offset after consumed data
function decompressSpriteTiled(bankData, dataPos, width, height) {
  const dv = new DataView(bankData.buffer, bankData.byteOffset, bankData.byteLength);
  const out = new Uint8Array(width * height);
  const outDV = new DataView(out.buffer);
  let pos = dataPos;
  let tempvalue = 0;
  let count = 0;

  for (let y2 = 0; y2 < Math.ceil(height / 64); y2++) {
    for (let x2 = 0; x2 < Math.ceil(width / 64); x2++) {
      const ymax = Math.min(y2 * 64 + 64, height);
      const xmax = Math.min(x2 * 64 + 64, width);
      for (let y = y2 * 64; y < ymax; y++) {
        for (let x = x2 * 64; x < xmax; x += 4) {
          const destOff = y * width + x;
          if (count > 0) {
            outDV.setUint32(destOff, tempvalue, true);
            count -= 4;
          } else {
            tempvalue = dv.getUint32(pos, true);
            pos += 4;
            if (tempvalue >= 0xFF000000) {
              count = tempvalue & 0x0000FFFF;
              const b = (tempvalue >>> 16) & 0xFF;
              tempvalue = b | (b << 8) | (b << 16) | (b << 24);
              count -= 4;
            }
            outDV.setUint32(destOff, tempvalue, true);
          }
        }
      }
    }
  }
  return { pixels: out, newPos: pos };
}

function spriteToImageData(pixels, palette, width, height) {
  const rgba = new Uint8ClampedArray(width * height * 4);
  for (let i = 0; i < width * height; i++) {
    const idx = pixels[i];
    if (idx === 0) { rgba[i * 4 + 3] = 0; continue; }
    rgba[i * 4]     = palette[idx * 3];
    rgba[i * 4 + 1] = palette[idx * 3 + 1];
    rgba[i * 4 + 2] = palette[idx * 3 + 2];
    rgba[i * 4 + 3] = 255;
  }
  return new ImageData(rgba, width, height);
}

async function loadSpriteBank(bankData, bankNum, spriteCount, pal) {
  const headerSize = 344 * spriteCount + 4;
  const hdr = new DataView(bankData.buffer, bankData.byteOffset, headerSize);
  let dataPos = headerSize;
  const sprites = [];

  for (let j = 0; j < spriteCount; j++) {
    const base = j * 344;
    const width   = hdr.getUint16(base,     true);
    const height  = hdr.getUint16(base + 2, true);
    const offsetX = hdr.getInt16 (base + 4, true);
    const offsetY = hdr.getInt16 (base + 6, true);
    const size    = hdr.getUint32(base + 12, true);
    const useTiled = hdr.getUint8(base + 20) !== 0;

    let pixels;
    if (useTiled) {
      const result = decompressSpriteTiled(bankData, dataPos, width, height);
      pixels = result.pixels;
      dataPos = result.newPos;
    } else {
      pixels = decompressSpriteFlat(bankData, dataPos, size, width, height);
      dataPos += size;
    }

    if (width > 0 && height > 0) {
      const imageData = spriteToImageData(pixels, pal, width, height);
      const bitmap = await createImageBitmap(imageData);
      sprites.push({ bitmap, offsetX, offsetY, width, height });
    } else {
      sprites.push(null);
    }
  }
  return sprites;
}

function decodeTilePixels(bytes, tileCount) {
  const src = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const out = new Uint8Array(4096 * tileCount);
  const outDV = new DataView(out.buffer);
  let k = 0;
  const srcWords = Math.floor(bytes.byteLength / 4);
  for (let j = 0; j < srcWords; j++, k++) {
    const val = src.getUint32(j * 4, true);
    if (val >= 0xFF000000) {
      let count = val & 0x0000FFFF;
      const color = (val >>> 16) & 0xFF;
      const word = color | (color << 8) | (color << 16) | (color << 24);
      while (count > 0) {
        outDV.setUint32(k * 4, word, true);
        count -= 4;
        k++;
      }
      k--;
    } else {
      outDV.setUint32(k * 4, val, true);
    }
  }
  return out;
}

function tileToImageData(pixels, palette) {
  const rgba = new Uint8ClampedArray(4096 * 4);
  for (let i = 0; i < 4096; i++) {
    const idx = pixels[i];
    if (idx === 0) { rgba[i * 4 + 3] = 0; continue; }
    rgba[i * 4]     = palette[idx * 3];
    rgba[i * 4 + 1] = palette[idx * 3 + 1];
    rgba[i * 4 + 2] = palette[idx * 3 + 2];
    rgba[i * 4 + 3] = 255;
  }
  return new ImageData(rgba, 64, 64);
}

async function processData(palBytes, binTilBytes, tilFileMap, binSprBytes, sprFileMap, readFn, setters) {
  const { setPalette, setTileBankCounts, setProgress, setTileImages, setSpriteImages, setLoaded } = setters;

  const pal0 = getPalette(palBytes, 0);
  setPalette(pal0);

  // --- Tile loading ---
  const bankCounts = new Map();
  for (let i = 0; i < 256; i++) {
    const count = binTilBytes[i * 64 + 2];
    if (count > 0) bankCounts.set(i, count);
  }
  setTileBankCounts(bankCounts);

  const tileBankNums = [...tilFileMap.keys()].filter(b => bankCounts.has(b));
  const sprBankNums = binSprBytes
    ? [...sprFileMap.keys()].filter(b => NEEDED_SPRITE_BANKS.has(b) && binSprBytes[b * 64 + 2] > 0)
    : [];

  const totalSteps = tileBankNums.length + sprBankNums.length;
  if (totalSteps === 0) { setLoaded(true); return; }
  setProgress({ total: totalSteps, done: 0 });

  const tileImages = new Map();
  for (let fi = 0; fi < tileBankNums.length; fi++) {
    const bankNum   = tileBankNums[fi];
    const tileCount = bankCounts.get(bankNum);
    const handle    = tilFileMap.get(bankNum);
    const bytes     = await readFn(handle);
    const headerSize = 12 * tileCount + 4;
    const payload   = bytes.slice(headerSize);
    const pixels    = decodeTilePixels(payload, tileCount);

    const bitmaps = [];
    for (let t = 0; t < tileCount; t++) {
      const tilePixels = pixels.slice(t * 4096, (t + 1) * 4096);
      bitmaps.push(await createImageBitmap(tileToImageData(tilePixels, pal0)));
    }
    tileImages.set(bankNum, bitmaps);
    setProgress({ total: totalSteps, done: fi + 1 });
  }
  setTileImages(tileImages);

  // --- Sprite loading ---
  const spriteImages = new Map();
  for (let fi = 0; fi < sprBankNums.length; fi++) {
    const bankNum = sprBankNums[fi];
    const spriteCount = binSprBytes[bankNum * 64 + 2];
    const handle = sprFileMap.get(bankNum);
    const bankData = await readFn(handle);
    const palIdx = spritePaletteIndex(bankNum);
    const pal = getPalette(palBytes, palIdx);
    const sprites = await loadSpriteBank(bankData, bankNum, spriteCount, pal);
    spriteImages.set(bankNum, sprites);
    setProgress({ total: totalSteps, done: tileBankNums.length + fi + 1 });
  }
  setSpriteImages(spriteImages);
  setLoaded(true);
}

export function useGameData() {
  const [loaded, setLoaded]           = useState(false);
  const [error, setError]             = useState(null);
  const [palette, setPalette]         = useState(null);
  const [tileImages, setTileImages]   = useState(new Map());
  const [spriteImages, setSpriteImages] = useState(new Map());
  const [tileBankCounts, setTileBankCounts] = useState(new Map());
  const [progress, setProgress]       = useState({ total: 0, done: 0 });

  const setters = { setPalette, setTileBankCounts, setProgress, setTileImages, setSpriteImages, setLoaded };

  const reset = () => { setLoaded(false); setError(null); setProgress({ total: 0, done: 0 }); };

  const loadFiles = useCallback(async (fileList) => {
    reset();
    try {
      const files = Array.from(fileList);
      const palFile    = files.find(f => f.name.toUpperCase() === 'PALETTE.BIN');
      const binTilFile = files.find(f => f.name.toUpperCase() === 'BIN_TIL.DAT');
      const binSprFile = files.find(f => f.name.toUpperCase() === 'BIN_SPR.DAT');
      const tilFiles   = files.filter(f => /^TIL_\d+\.BIN$/i.test(f.name));
      const sprFiles   = files.filter(f => /^SPR_\d+\.BIN$/i.test(f.name));

      if (!palFile)    throw new Error('PALETTE.BIN not found in selected folder');
      if (!binTilFile) throw new Error('BIN_TIL.DAT not found in selected folder');
      if (tilFiles.length === 0) throw new Error('No TIL_*.BIN files found — make sure you selected the data/ folder');

      const palBytes    = new Uint8Array(await palFile.arrayBuffer());
      const binTilBytes = new Uint8Array(await binTilFile.arrayBuffer());
      const binSprBytes = binSprFile ? new Uint8Array(await binSprFile.arrayBuffer()) : null;

      const tilFileMap = new Map(
        tilFiles.map(f => {
          const m = f.name.match(/TIL_(\d+)\.BIN/i);
          return m ? [parseInt(m[1], 10), f] : null;
        }).filter(Boolean)
      );
      const sprFileMap = new Map(
        sprFiles.map(f => {
          const m = f.name.match(/SPR_(\d+)\.BIN/i);
          return m ? [parseInt(m[1], 10), f] : null;
        }).filter(Boolean)
      );

      await processData(palBytes, binTilBytes, tilFileMap, binSprBytes, sprFileMap,
        async (f) => new Uint8Array(await f.arrayBuffer()), setters);
    } catch (e) {
      setError(e.message);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return { loaded, error, palette, tileImages, spriteImages, tileBankCounts, progress, loadFiles };
}

