'use client';
import { useState, useCallback } from 'react';
import { ACTOR_DEFS } from './Toolbar';
import type { SpriteEntry } from '../../lib/types';
import * as gameDataStore from '../../lib/game-data-store';

const NEEDED_SPRITE_BANKS = new Set([
  0, 1, 2, 3, 4, // parallax backgrounds
  ...ACTOR_DEFS.map(a => a.bank).filter((b): b is number => b != null),
  184,
  200, 201, 205,
  49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
]);

function getPalette(palBytes: Uint8Array, palIndex: number): Uint8Array {
  const pal = new Uint8Array(256 * 3);
  const base = 4 + palIndex * 772;
  for (let i = 0; i < 256; i++) {
    pal[i * 3]     = palBytes[base + i * 3]     << 2;
    pal[i * 3 + 1] = palBytes[base + i * 3 + 1] << 2;
    pal[i * 3 + 2] = palBytes[base + i * 3 + 2] << 2;
  }
  return pal;
}

// Parallax palette index for banks 0-4 (matches C++ SetParallaxColors)
const PARALLAX_PALETTE_IDX: Record<number, number> = { 0: 5, 1: 6, 2: 7, 3: 8, 4: 9 };

// C++ SetParallaxColors only overrides the last 30 entries (226-255) of palette 0.
// Replicate that: base = palette 0, override 226-255 with the parallax palette.
function getParallaxHybridPalette(palBytes: Uint8Array, parallaxBank: number): Uint8Array {
  const pal = getPalette(palBytes, 0);
  const parallaxPalIdx = PARALLAX_PALETTE_IDX[parallaxBank];
  const src = getPalette(palBytes, parallaxPalIdx);
  for (let i = 226; i < 256; i++) {
    pal[i * 3]     = src[i * 3];
    pal[i * 3 + 1] = src[i * 3 + 1];
    pal[i * 3 + 2] = src[i * 3 + 2];
  }
  return pal;
}

function spritePaletteIndex(bankNum: number): number {
  switch (bankNum) {
    case 6: return 1;
    case 7: return 2;
    default: return 0;
  }
}

function decompressSpriteFlat(bankData: Uint8Array, dataPos: number, size: number, width: number, height: number): Uint8Array {
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

function decompressSpriteTiled(bankData: Uint8Array, dataPos: number, width: number, height: number): { pixels: Uint8Array; newPos: number } {
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
              const bVal = (tempvalue >>> 16) & 0xFF;
              tempvalue = bVal | (bVal << 8) | (bVal << 16) | (bVal << 24);
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

function spriteToImageData(pixels: Uint8Array, palette: Uint8Array, width: number, height: number): ImageData {
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

async function loadSpriteBank(bankData: Uint8Array, bankNum: number, spriteCount: number, pal: Uint8Array): Promise<(SpriteEntry | null)[]> {
  const headerSize = 344 * spriteCount + 4;
  const hdr = new DataView(bankData.buffer, bankData.byteOffset, headerSize);
  let dataPos = headerSize;
  const sprites: (SpriteEntry | null)[] = [];

  for (let j = 0; j < spriteCount; j++) {
    const base = j * 344;
    const width   = hdr.getUint16(base,     true);
    const height  = hdr.getUint16(base + 2, true);
    const offsetX = hdr.getInt16 (base + 4, true);
    const offsetY = hdr.getInt16 (base + 6, true);
    const size    = hdr.getUint32(base + 12, true);
    const useTiled = hdr.getUint8(base + 20) !== 0;

    let pixels: Uint8Array;
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

function decodeTilePixels(bytes: Uint8Array, tileCount: number): Uint8Array {
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

function tileToImageData(pixels: Uint8Array, palette: Uint8Array): ImageData {
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

interface Setters {
  setPalette: (p: Uint8Array) => void;
  setTileBankCounts: (m: Map<number, number>) => void;
  setProgress: (p: { total: number; done: number }) => void;
  setTileImages: (m: Map<number, ImageBitmap[]>) => void;
  setSpriteImages: (m: Map<number, (SpriteEntry | null)[]>) => void;
  setLoaded: (v: boolean) => void;
}

async function processData(
  palBytes: Uint8Array,
  binTilBytes: Uint8Array,
  tilFileMap: Map<number, File>,
  binSprBytes: Uint8Array | null,
  sprFileMap: Map<number, File>,
  readFn: (f: File) => Promise<Uint8Array>,
  setters: Setters,
): Promise<void> {
  const { setPalette, setTileBankCounts, setProgress, setTileImages, setSpriteImages, setLoaded } = setters;

  const pal0 = getPalette(palBytes, 0);
  setPalette(pal0);

  const bankCounts = new Map<number, number>();
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

  const tileImages = new Map<number, ImageBitmap[]>();
  for (let fi = 0; fi < tileBankNums.length; fi++) {
    const bankNum   = tileBankNums[fi];
    const tileCount = bankCounts.get(bankNum)!;
    const handle    = tilFileMap.get(bankNum)!;
    const bytes     = await readFn(handle);
    const headerSize = 12 * tileCount + 4;
    const payload   = bytes.slice(headerSize);
    const pixels    = decodeTilePixels(payload, tileCount);

    const bitmaps: ImageBitmap[] = [];
    for (let t = 0; t < tileCount; t++) {
      const tilePixels = pixels.slice(t * 4096, (t + 1) * 4096);
      bitmaps.push(await createImageBitmap(tileToImageData(tilePixels, pal0)));
    }
    tileImages.set(bankNum, bitmaps);
    setProgress({ total: totalSteps, done: fi + 1 });
  }
  setTileImages(tileImages);

  const spriteImages = new Map<number, (SpriteEntry | null)[]>();
  for (let fi = 0; fi < sprBankNums.length; fi++) {
    const bankNum = sprBankNums[fi];
    const spriteCount = binSprBytes![bankNum * 64 + 2];
    const handle = sprFileMap.get(bankNum)!;
    const bankData = await readFn(handle);
    const pal = (bankNum in PARALLAX_PALETTE_IDX)
      ? getParallaxHybridPalette(palBytes, bankNum)
      : getPalette(palBytes, spritePaletteIndex(bankNum));
    const sprites = await loadSpriteBank(bankData, bankNum, spriteCount, pal);
    spriteImages.set(bankNum, sprites);
    setProgress({ total: totalSteps, done: tileBankNums.length + fi + 1 });
  }
  setSpriteImages(spriteImages);
  setLoaded(true);
}

export function useGameData() {
  const [loaded, setLoaded]             = useState(() => gameDataStore.isLoaded());
  const [error, setError]               = useState<string | null>(null);
  const [palette, setPalette]           = useState<Uint8Array | null>(null);
  const [tileImages, setTileImages]     = useState<Map<number, ImageBitmap[]>>(
    () => gameDataStore.get()?.tileImages ?? new Map(),
  );
  const [spriteImages, setSpriteImages] = useState<Map<number, (SpriteEntry | null)[]>>(
    () => gameDataStore.get()?.spriteImages ?? new Map(),
  );
  const [tileBankCounts, setTileBankCounts] = useState<Map<number, number>>(
    () => gameDataStore.get()?.tileBankCounts ?? new Map(),
  );
  const [progress, setProgress]         = useState({ total: 0, done: 0 });

  const reset = () => { setLoaded(false); setError(null); setProgress({ total: 0, done: 0 }); gameDataStore.clear(); };

  const loadFiles = useCallback(async (fileList: FileList) => {
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
      if (tilFiles.length === 0) throw new Error('No TIL_*.BIN files found — make sure you selected the assets/ folder');

      const palBytes    = new Uint8Array(await palFile.arrayBuffer());
      const binTilBytes = new Uint8Array(await binTilFile.arrayBuffer());
      const binSprBytes = binSprFile ? new Uint8Array(await binSprFile.arrayBuffer()) : null;

      const tilFileMap = new Map<number, File>(
        tilFiles.map(f => {
          const m = f.name.match(/TIL_(\d+)\.BIN/i);
          return m ? [parseInt(m[1], 10), f] as [number, File] : null;
        }).filter((x): x is [number, File] => x !== null)
      );
      const sprFileMap = new Map<number, File>(
        sprFiles.map(f => {
          const m = f.name.match(/SPR_(\d+)\.BIN/i);
          return m ? [parseInt(m[1], 10), f] as [number, File] : null;
        }).filter((x): x is [number, File] => x !== null)
      );

      // Capture final values so we can store them after processData finishes
      let capturedTile:   Map<number, ImageBitmap[]>             | null = null;
      let capturedSprite: Map<number, (SpriteEntry | null)[]>    | null = null;
      let capturedCounts: Map<number, number>                    | null = null;
      const wrappedSetters: Setters = {
        setPalette,
        setTileBankCounts: (m) => { capturedCounts = m; setTileBankCounts(m); },
        setProgress,
        setTileImages:     (m) => { capturedTile   = m; setTileImages(m); },
        setSpriteImages:   (m) => { capturedSprite  = m; setSpriteImages(m); },
        setLoaded: (v) => {
          if (v && capturedTile && capturedCounts) {
            gameDataStore.set({
              tileImages:     capturedTile,
              spriteImages:   capturedSprite ?? new Map(),
              tileBankCounts: capturedCounts,
            });
          }
          setLoaded(v);
        },
      };

      await processData(palBytes, binTilBytes, tilFileMap, binSprBytes, sprFileMap,
        async (f) => new Uint8Array(await f.arrayBuffer()), wrappedSetters);
    } catch (e) {
      setError((e as Error).message);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return { loaded, error, palette, tileImages, spriteImages, tileBankCounts, progress, loadFiles };
}
