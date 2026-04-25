'use client';
import { useState, useCallback, useRef } from 'react';
import pako from 'pako';

const CELL_SIZE = 36; // bytes per map cell
const MAX_HISTORY = 50;

function parseHeader(dv) {
  const firstbyte   = dv.getUint8(0);
  const version     = dv.getUint8(1);
  const maxplayers  = dv.getUint8(2);
  const maxteams    = dv.getUint8(3);
  const width       = dv.getUint16(4, false);   // big-endian
  const height      = dv.getUint16(6, false);   // big-endian
  const parallax    = dv.getUint8(9);
  const ambience    = dv.getInt8(10);
  const flags       = dv.getUint32(13, false);  // big-endian
  const descBytes   = new Uint8Array(dv.buffer, dv.byteOffset + 17, 128);
  let descEnd = descBytes.indexOf(0);
  if (descEnd === -1) descEnd = 128;
  const description = new TextDecoder().decode(descBytes.slice(0, descEnd));

  const minimapCompressedSize = dv.getUint32(145, true);
  const minimapStart = 149;
  const minimapEnd   = minimapStart + minimapCompressedSize;
  const rawMinimap   = new Uint8Array(dv.buffer, dv.byteOffset + minimapStart, minimapCompressedSize);
  const levelSize    = dv.getUint32(minimapEnd, true);
  const levelStart   = minimapEnd + 4;

  return {
    header: { firstbyte, version, maxplayers, maxteams, parallax, ambience, flags, description },
    width, height,
    rawMinimap: rawMinimap.slice(),
    levelDataOffset: levelStart,
    levelDataSize: levelSize,
    minimapCompressedSize,
  };
}

function parseLevelData(raw, width, height) {
  const numCells = width * height;
  const layers = {
    bg: [new Array(numCells).fill(null), new Array(numCells).fill(null),
         new Array(numCells).fill(null), new Array(numCells).fill(null)],
    fg: [new Array(numCells).fill(null), new Array(numCells).fill(null),
         new Array(numCells).fill(null), new Array(numCells).fill(null)],
  };
  const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  for (let i = 0; i < numCells; i++) {
    const base = i * CELL_SIZE;
    for (let l = 0; l < 4; l++) {
      const off = base + l * 4;
      layers.bg[l][i] = { tile_id: dv.getUint16(off, true), flip: dv.getUint8(off + 2), lum: dv.getUint8(off + 3) };
    }
    for (let l = 0; l < 4; l++) {
      const off = base + 20 + l * 4;
      layers.fg[l][i] = { tile_id: dv.getUint16(off, true), flip: dv.getUint8(off + 2), lum: dv.getUint8(off + 3) };
    }
  }
  return layers;
}

function parseActors(dv, offset) {
  const numActors = dv.getUint32(offset, true);
  offset += 8;
  const actors = [];
  for (let i = 0; i < numActors; i++) {
    actors.push({
      id:         dv.getUint32(offset,      true),
      x:          dv.getUint32(offset + 4,  true),
      y:          dv.getUint32(offset + 8,  true),
      direction:  dv.getUint32(offset + 12, true),
      type:       dv.getInt32 (offset + 16, true),
      matchid:    dv.getUint32(offset + 20, true),
      subplane:   dv.getUint32(offset + 24, true),
      unknown:    dv.getUint32(offset + 28, true),
      securityid: dv.getUint32(offset + 32, true),
    });
    offset += 36;
  }
  return { actors, offset };
}

function parsePlatforms(dv, offset) {
  const numPlatforms = dv.getUint32(offset, true);
  offset += 8;
  const platforms = [];
  for (let i = 0; i < numPlatforms; i++) {
    const x1 = dv.getInt32(offset, true), y1 = dv.getInt32(offset + 4, true);
    const x2 = dv.getInt32(offset + 8, true), y2 = dv.getInt32(offset + 12, true);
    const type1 = dv.getInt32(offset + 16, true), type2 = dv.getInt32(offset + 20, true);
    platforms.push({ x1, y1, x2, y2, type1, type2, typeName: platformType(type1, type2) });
    offset += 24;
  }
  return { platforms, offset };
}

function platformType(type1, type2) {
  if (type1 === 0 && type2 === 0) return 'RECTANGLE';
  if (type1 === 1 && type2 === 0) return 'LADDER';
  if (type1 === 0 && type2 === 1) return 'STAIRSUP';
  if (type1 === 0 && type2 === 2) return 'STAIRSDOWN';
  if (type1 === 2 && type2 === 0) return 'TRACK';
  if (type1 === 3 && type2 === 0) return 'OUTSIDEROOM';
  if (type1 === 3 && type2 === 1) return 'SPECIFICROOM';
  return 'RECTANGLE';
}

export function platformTypeNums(typeName) {
  const map = {
    RECTANGLE: [0,0], LADDER: [1,0], STAIRSUP: [0,1],
    STAIRSDOWN: [0,2], TRACK: [2,0], OUTSIDEROOM: [3,0], SPECIFICROOM: [3,1],
  };
  return map[typeName] ?? [0, 0];
}

export function useSilMap() {
  const [mapData, setMapData] = useState(null);
  const [canUndo, setCanUndo] = useState(false);
  const [canRedo, setCanRedo] = useState(false);

  // History stacks stored in refs to avoid triggering re-renders on push/pop
  const historyRef   = useRef([]); // past states
  const futureRef    = useRef([]); // states after current (for redo)
  const prePaintRef  = useRef(null); // snapshot before a paint stroke begins

  const syncUndoRedo = () => {
    setCanUndo(historyRef.current.length > 0);
    setCanRedo(futureRef.current.length > 0);
  };

  const pushHistory = useCallback((snapshot) => {
    if (!snapshot) return;
    historyRef.current = [...historyRef.current.slice(-(MAX_HISTORY - 1)), snapshot];
    futureRef.current = [];
    syncUndoRedo();
  }, []);

  const undo = useCallback(() => {
    setMapData(current => {
      const history = historyRef.current;
      if (history.length === 0) return current;
      const prev = history[history.length - 1];
      historyRef.current = history.slice(0, -1);
      if (current) futureRef.current = [current, ...futureRef.current.slice(0, MAX_HISTORY - 1)];
      syncUndoRedo();
      return prev;
    });
  }, []);

  const redo = useCallback(() => {
    setMapData(current => {
      const future = futureRef.current;
      if (future.length === 0) return current;
      const next = future[0];
      futureRef.current = future.slice(1);
      if (current) historyRef.current = [...historyRef.current.slice(-(MAX_HISTORY - 1)), current];
      syncUndoRedo();
      return next;
    });
  }, []);

  const createMap = useCallback((width, height, description) => {
    const numCells = width * height;
    historyRef.current = [];
    futureRef.current = [];
    syncUndoRedo();
    setMapData({
      header: { firstbyte: 0, version: 0, maxplayers: 8, maxteams: 2, parallax: 0, ambience: 0, flags: 0, description: description || 'New Map' },
      width, height,
      fileName: 'new_map.SIL',
      layers: {
        bg: [new Array(numCells).fill(null), new Array(numCells).fill(null), new Array(numCells).fill(null), new Array(numCells).fill(null)],
        fg: [new Array(numCells).fill(null), new Array(numCells).fill(null), new Array(numCells).fill(null), new Array(numCells).fill(null)],
      },
      actors: [],
      platforms: [],
      rawMinimap: new Uint8Array(0),
      minimapCompressedSize: 0,
    });
  }, []);


  const openMap = useCallback(async (file) => {
    try {
      const buf = await file.arrayBuffer();
      const bytes = new Uint8Array(buf);
      const dv = new DataView(bytes.buffer);
      const parsed = parseHeader(dv);
      const { header, width, height, rawMinimap, levelDataOffset, minimapCompressedSize } = parsed;

      const compressedLevel = bytes.slice(levelDataOffset);
      const levelRaw = pako.inflate(compressedLevel);
      const levelDV = new DataView(levelRaw.buffer);
      const numCells = width * height;
      const tileSectionSize = numCells * CELL_SIZE;
      const layers = parseLevelData(levelRaw.slice(0, tileSectionSize), width, height);

      let offset = tileSectionSize;
      const { actors, offset: off2 } = parseActors(levelDV, offset);
      const { platforms } = parsePlatforms(levelDV, off2);

      historyRef.current = [];
      futureRef.current = [];
      syncUndoRedo();
      setMapData({ header, width, height, layers, actors, platforms, rawMinimap, minimapCompressedSize, fileName: file.name });
    } catch (e) {
      console.error('Failed to open SIL map:', e);
      alert('Failed to parse map: ' + e.message);
    }
  }, []);

  const saveMap = useCallback(async () => {
    if (!mapData) return;
    const { header, width, height, layers, actors, platforms, rawMinimap, fileName } = mapData;
    const numCells = width * height;
    const tileSectionSize = numCells * CELL_SIZE;
    const actorsSectionSize = 8 + actors.length * 36;
    const platformsSectionSize = 8 + platforms.length * 24;
    const levelBuf = new ArrayBuffer(tileSectionSize + actorsSectionSize + platformsSectionSize);
    const ldv = new DataView(levelBuf);

    for (let i = 0; i < numCells; i++) {
      const base = i * CELL_SIZE;
      for (let l = 0; l < 4; l++) {
        const off = base + l * 4;
        const cell = layers.bg[l][i] ?? { tile_id: 0, flip: 0, lum: 0 };
        ldv.setUint16(off, cell.tile_id, true);
        ldv.setUint8(off + 2, cell.flip);
        ldv.setUint8(off + 3, cell.lum);
      }
      for (let l = 0; l < 4; l++) {
        const off = base + 20 + l * 4;
        const cell = layers.fg[l][i] ?? { tile_id: 0, flip: 0, lum: 0 };
        ldv.setUint16(off, cell.tile_id, true);
        ldv.setUint8(off + 2, cell.flip);
        ldv.setUint8(off + 3, cell.lum);
      }
    }

    let off = tileSectionSize;
    ldv.setUint32(off, actors.length, true); off += 4;
    ldv.setUint32(off, 0, true); off += 4;
    for (const a of actors) {
      ldv.setUint32(off,      a.id,         true);
      ldv.setUint32(off + 4,  a.x,          true);
      ldv.setUint32(off + 8,  a.y,          true);
      ldv.setUint32(off + 12, a.direction,  true);
      ldv.setInt32 (off + 16, a.type,       true);
      ldv.setUint32(off + 20, a.matchid,    true);
      ldv.setUint32(off + 24, a.subplane,   true);
      ldv.setUint32(off + 28, a.unknown,    true);
      ldv.setUint32(off + 32, a.securityid, true);
      off += 36;
    }
    ldv.setUint32(off, platforms.length, true); off += 4;
    ldv.setUint32(off, 0, true); off += 4;
    for (const p of platforms) {
      ldv.setInt32(off,      p.x1,    true);
      ldv.setInt32(off + 4,  p.y1,    true);
      ldv.setInt32(off + 8,  p.x2,    true);
      ldv.setInt32(off + 12, p.y2,    true);
      ldv.setInt32(off + 16, p.type1, true);
      ldv.setInt32(off + 20, p.type2, true);
      off += 24;
    }

    const levelCompressed = pako.deflate(new Uint8Array(levelBuf));
    const descBytes = new TextEncoder().encode(header.description);
    const descBuf = new Uint8Array(128);
    descBuf.set(descBytes.slice(0, 127));
    const totalSize = 149 + rawMinimap.length + 4 + levelCompressed.length;
    const fileBuf = new ArrayBuffer(totalSize);
    const fdv = new DataView(fileBuf);
    const fBytes = new Uint8Array(fileBuf);

    fdv.setUint8(0, header.firstbyte);
    fdv.setUint8(1, header.version);
    fdv.setUint8(2, header.maxplayers);
    fdv.setUint8(3, header.maxteams);
    fdv.setUint16(4, width, false);
    fdv.setUint16(6, height, false);
    fdv.setUint8(9, header.parallax);
    fdv.setInt8(10, header.ambience);
    fdv.setUint32(13, header.flags, false);
    fBytes.set(descBuf, 17);
    fdv.setUint32(145, rawMinimap.length, true);
    fBytes.set(rawMinimap, 149);
    const afterMinimap = 149 + rawMinimap.length;
    fdv.setUint32(afterMinimap, levelCompressed.length, true);
    fBytes.set(levelCompressed, afterMinimap + 4);

    const blob = new Blob([fileBuf], { type: 'application/octet-stream' });
    const suggestedName = fileName || 'map.SIL';

    if (window.showSaveFilePicker) {
      try {
        const handle = await window.showSaveFilePicker({
          suggestedName,
          types: [{ description: 'Silencer Map', accept: { 'application/octet-stream': ['.SIL'] } }],
        });
        const writable = await handle.createWritable();
        await writable.write(blob);
        await writable.close();
        return;
      } catch (e) {
        if (e.name === 'AbortError') return; // user cancelled
      }
    }
    // Fallback: trigger download
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = suggestedName; a.click();
    URL.revokeObjectURL(url);
  }, [mapData]);

  // Called at the start of a paint stroke — saves snapshot for commit
  const beginPaint = useCallback(() => {
    setMapData(current => { prePaintRef.current = current; return current; });
  }, []);

  // Called at end of paint stroke — pushes pre-stroke snapshot to history
  const commitPaint = useCallback(() => {
    if (prePaintRef.current) {
      pushHistory(prePaintRef.current);
      prePaintRef.current = null;
    }
  }, [pushHistory]);

  const updateTile = useCallback((layerType, layerIdx, x, y, tile_id, flip = 0, lum = 255) => {
    setMapData(prev => {
      if (!prev) return prev;
      const { width, height, layers } = prev;
      if (x < 0 || x >= width || y < 0 || y >= height) return prev;
      const idx = y * width + x;
      const layerArr = layerType === 'fg' ? layers.fg : layers.bg;
      const newLayer = layerArr[layerIdx].slice();
      newLayer[idx] = { tile_id, flip, lum };
      const newLayers = {
        bg: layerType === 'bg' ? layers.bg.map((l, i) => i === layerIdx ? newLayer : l) : layers.bg,
        fg: layerType === 'fg' ? layers.fg.map((l, i) => i === layerIdx ? newLayer : l) : layers.fg,
      };
      return { ...prev, layers: newLayers };
    });
  }, []);

  const addPlatform = useCallback((platform) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, platforms: [...prev.platforms, platform] };
    });
  }, [pushHistory]);

  const removePlatform = useCallback((idx) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, platforms: prev.platforms.filter((_, i) => i !== idx) };
    });
  }, [pushHistory]);

  const addActor = useCallback((actor) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, actors: [...prev.actors, actor] };
    });
  }, [pushHistory]);

  const removeActor = useCallback((idx) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, actors: prev.actors.filter((_, i) => i !== idx) };
    });
  }, [pushHistory]);

  const updateActor = useCallback((idx, patch) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const actors = prev.actors.map((a, i) => i === idx ? { ...a, ...patch } : a);
      return { ...prev, actors };
    });
  }, [pushHistory]);

  const resizeMap = useCallback((newWidth, newHeight) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const { width, height, layers } = prev;
      const newSize = newWidth * newHeight;
      const empty = { tile_id: 0, flip: 0, lum: 0 };

      const resizeLayer = (layer) => {
        const next = new Array(newSize).fill(null);
        const copyW = Math.min(width, newWidth);
        const copyH = Math.min(height, newHeight);
        for (let row = 0; row < copyH; row++) {
          for (let col = 0; col < copyW; col++) {
            next[row * newWidth + col] = layer[row * width + col];
          }
        }
        return next;
      };

      const newLayers = {
        bg: layers.bg.map(resizeLayer),
        fg: layers.fg.map(resizeLayer),
      };

      // Clamp actors and platforms to new bounds (in world pixels)
      const maxX = newWidth * 64;
      const maxY = newHeight * 64;
      const actors    = prev.actors.filter(a => a.x < maxX && a.y < maxY);
      const platforms = prev.platforms.filter(p => p.x1 < maxX && p.y1 < maxY);

      return { ...prev, width: newWidth, height: newHeight, layers: newLayers, actors, platforms };
    });
  }, [pushHistory]);

  const updateHeader = useCallback((patch) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, header: { ...prev.header, ...patch } };
    });
  }, [pushHistory]);

  const moveActor = useCallback((idx, x, y) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const actors = prev.actors.map((a, i) => i === idx ? { ...a, x, y } : a);
      return { ...prev, actors };
    });
  }, [pushHistory]);

  const updatePlatform = useCallback((idx, x1, y1, x2, y2) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const platforms = prev.platforms.map((p, i) => i === idx ? { ...p, x1, y1, x2, y2 } : p);
      return { ...prev, platforms };
    });
  }, [pushHistory]);

  const patchTile = useCallback((layerType, layerIdx, x, y, patch) => {
    setMapData(prev => {
      if (!prev) return prev;
      const { width, height, layers } = prev;
      if (x < 0 || x >= width || y < 0 || y >= height) return prev;
      pushHistory(prev);
      const idx = y * width + x;
      const layerArr = layerType === 'fg' ? layers.fg : layers.bg;
      const existing = layerArr[layerIdx][idx] ?? { tile_id: 0, flip: 0, lum: 0 };
      const newLayer = layerArr[layerIdx].slice();
      newLayer[idx] = { ...existing, ...patch };
      const newLayers = {
        bg: layerType === 'bg' ? layers.bg.map((l, i) => i === layerIdx ? newLayer : l) : layers.bg,
        fg: layerType === 'fg' ? layers.fg.map((l, i) => i === layerIdx ? newLayer : l) : layers.fg,
      };
      return { ...prev, layers: newLayers };
    });
  }, [pushHistory]);

  return {
    map: mapData, openMap, saveMap, createMap,
    updateTile, patchTile, beginPaint, commitPaint,
    addPlatform, removePlatform, updatePlatform,
    addActor, removeActor, updateActor, moveActor,
    updateHeader,
    undo, redo, canUndo, canRedo,
    resizeMap,
  };
}
