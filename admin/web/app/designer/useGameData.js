'use client';
import { useState, useCallback } from 'react';

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

async function readFileBytes(fileHandle) {
  const file = await fileHandle.getFile();
  return new Uint8Array(await file.arrayBuffer());
}

async function processData(palBytes, binTilBytes, tilFileMap, readFn, setters) {
  const { setPalette, setTileBankCounts, setProgress, setTileImages, setLoaded } = setters;

  const rawPal = palBytes.slice(4, 4 + 256 * 3);
  const pal = new Uint8Array(256 * 3);
  for (let i = 0; i < 256 * 3; i++) pal[i] = rawPal[i] * 4;
  setPalette(pal);

  const bankCounts = new Map();
  for (let i = 0; i < 256; i++) {
    const count = binTilBytes[i * 64 + 2];
    if (count > 0) bankCounts.set(i, count);
  }
  setTileBankCounts(bankCounts);

  const bankNums = [...tilFileMap.keys()].filter(b => bankCounts.has(b));
  if (bankNums.length === 0) { setLoaded(true); return; }

  setProgress({ total: bankNums.length, done: 0 });
  const images = new Map();

  for (let fi = 0; fi < bankNums.length; fi++) {
    const bankNum   = bankNums[fi];
    const tileCount = bankCounts.get(bankNum);
    const handle    = tilFileMap.get(bankNum);
    const bytes     = await readFn(handle);
    const headerSize = 12 * tileCount + 4;
    const payload   = bytes.slice(headerSize);
    const pixels    = decodeTilePixels(payload, tileCount);

    const bitmaps = [];
    for (let t = 0; t < tileCount; t++) {
      const tilePixels = pixels.slice(t * 4096, (t + 1) * 4096);
      bitmaps.push(await createImageBitmap(tileToImageData(tilePixels, pal)));
    }
    images.set(bankNum, bitmaps);
    setProgress({ total: bankNums.length, done: fi + 1 });
  }

  setTileImages(images);
  setLoaded(true);
}

export function useGameData() {
  const [loaded, setLoaded]           = useState(false);
  const [error, setError]             = useState(null);
  const [palette, setPalette]         = useState(null);
  const [tileImages, setTileImages]   = useState(new Map());
  const [tileBankCounts, setTileBankCounts] = useState(new Map());
  const [progress, setProgress]       = useState({ total: 0, done: 0 });

  const setters = { setPalette, setTileBankCounts, setProgress, setTileImages, setLoaded };

  const reset = () => { setLoaded(false); setError(null); setProgress({ total: 0, done: 0 }); };

  const pickDirectory = useCallback(async () => {
    reset();
    try {
      const dirHandle = await window.showDirectoryPicker({ mode: 'read' });
      let palBytes = null, binTilBytes = null;
      const tilFileMap = new Map();

      for await (const [name, handle] of dirHandle.entries()) {
        if (handle.kind !== 'file') continue;
        const upper = name.toUpperCase();
        if (upper === 'PALETTE.BIN')  palBytes    = await readFileBytes(handle);
        if (upper === 'BIN_TIL.DAT') binTilBytes = await readFileBytes(handle);
      }
      if (!palBytes)    throw new Error('PALETTE.BIN not found in selected directory');
      if (!binTilBytes) throw new Error('BIN_TIL.DAT not found in selected directory');

      for await (const [name, handle] of dirHandle.entries()) {
        if (handle.kind !== 'directory') continue;
        if (name.toLowerCase() !== 'bin_til') continue;
        for await (const [tilName, tilHandle] of handle.entries()) {
          if (tilHandle.kind !== 'file') continue;
          const m = tilName.match(/TIL_(\d+)\.BIN/i);
          if (m) tilFileMap.set(parseInt(m[1], 10), tilHandle);
        }
        break;
      }

      await processData(palBytes, binTilBytes, tilFileMap, readFileBytes, setters);
    } catch (e) {
      if (e.name !== 'AbortError') setError(e.message);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const loadFiles = useCallback(async (fileList) => {
    reset();
    try {
      const files = Array.from(fileList);
      const palFile    = files.find(f => f.name.toUpperCase() === 'PALETTE.BIN');
      const binTilFile = files.find(f => f.name.toUpperCase() === 'BIN_TIL.DAT');
      const tilFiles   = files.filter(f => /^TIL_\d+\.BIN$/i.test(f.name));

      if (!palFile)    throw new Error('PALETTE.BIN not found — select it with the TIL_XXX.BIN files');
      if (!binTilFile) throw new Error('BIN_TIL.DAT not found — select it with the TIL_XXX.BIN files');

      const palBytes    = new Uint8Array(await palFile.arrayBuffer());
      const binTilBytes = new Uint8Array(await binTilFile.arrayBuffer());
      const tilFileMap  = new Map(
        tilFiles.map(f => {
          const m = f.name.match(/TIL_(\d+)\.BIN/i);
          return m ? [parseInt(m[1], 10), f] : null;
        }).filter(Boolean)
      );

      await processData(palBytes, binTilBytes, tilFileMap,
        async (f) => new Uint8Array(await f.arrayBuffer()), setters);
    } catch (e) {
      setError(e.message);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return { loaded, error, palette, tileImages, tileBankCounts, progress, loadFiles, pickDirectory };
}
