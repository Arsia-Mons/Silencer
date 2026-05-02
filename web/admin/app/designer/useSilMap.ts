'use client';
import { useState, useCallback, useRef, useEffect } from 'react';
import pako from 'pako';
import type { SilMapData, MapActor, MapPlatform, MapShadowZone, TileCell, MapHeader, MapLayers } from '../../lib/types';
import * as mapStore from '../../lib/map-store';
import { bakeMapLightMasks } from './lightBaker';

const CELL_SIZE = 36; // bytes per map cell
const MAX_HISTORY = 50;

interface ParsedHeader {
  header: MapHeader;
  width: number;
  height: number;
  rawMinimap: Uint8Array;
  levelDataOffset: number;
  levelDataSize: number;
  minimapCompressedSize: number;
}

function parseHeader(dv: DataView): ParsedHeader {
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

function parseLevelData(raw: Uint8Array, width: number, height: number): MapLayers {
  const numCells = width * height;
  const layers: MapLayers = {
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

function parseActors(dv: DataView, offset: number): { actors: MapActor[]; offset: number } {
  const numActors = dv.getUint32(offset, true);
  offset += 8;
  const actors: MapActor[] = [];
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

function parsePlatforms(dv: DataView, offset: number): { platforms: MapPlatform[]; offset: number } {
  const numPlatforms = dv.getUint32(offset, true);
  offset += 8;
  const platforms: MapPlatform[] = [];
  for (let i = 0; i < numPlatforms; i++) {
    const x1 = dv.getInt32(offset, true), y1 = dv.getInt32(offset + 4, true);
    const x2 = dv.getInt32(offset + 8, true), y2 = dv.getInt32(offset + 12, true);
    const type1 = dv.getInt32(offset + 16, true), type2 = dv.getInt32(offset + 20, true);
    platforms.push({ x1, y1, x2, y2, type1, type2, typeName: platformType(type1, type2) });
    offset += 24;
  }
  return { platforms, offset };
}

function parseShadowZones(dv: DataView, offset: number): { shadowZones: MapShadowZone[]; offset: number } {
  const shadowZones: MapShadowZone[] = [];
  if (offset + 8 > dv.byteLength) return { shadowZones, offset };
  const numZones = dv.getUint32(offset, true);
  offset += 8; // count + padding
  for (let i = 0; i < numZones && offset + 16 <= dv.byteLength; i++) {
    const x1 = dv.getInt32(offset, true), y1 = dv.getInt32(offset + 4, true);
    const x2 = dv.getInt32(offset + 8, true), y2 = dv.getInt32(offset + 12, true);
    shadowZones.push({ x1, y1, x2, y2 });
    offset += 16;
  }
  return { shadowZones, offset };
}

function platformType(type1: number, type2: number): string {
  if (type1 === 0 && type2 === 0) return 'RECTANGLE';
  if (type1 === 1 && type2 === 0) return 'LADDER';
  if (type1 === 0 && type2 === 1) return 'STAIRSUP';
  if (type1 === 0 && type2 === 2) return 'STAIRSDOWN';
  if (type1 === 2 && type2 === 0) return 'TRACK';
  if (type1 === 3 && type2 === 0) return 'OUTSIDEROOM';
  if (type1 === 3 && type2 === 1) return 'SPECIFICROOM';
  return 'RECTANGLE';
}

export function platformTypeNums(typeName: string): [number, number] {
  const map: Record<string, [number, number]> = {
    RECTANGLE: [0,0], LADDER: [1,0], STAIRSUP: [0,1],
    STAIRSDOWN: [0,2], TRACK: [2,0], OUTSIDEROOM: [3,0], SPECIFICROOM: [3,1],
  };
  return map[typeName] ?? [0, 0];
}

export interface UseSilMapReturn {
  map: SilMapData | null;
  openMap: (file: File) => Promise<SilMapData | null>;
  saveMap: () => Promise<void>;
  publishMap: (opts: { author: string; apiUrl: string; apiKey: string }) => Promise<{ ok: boolean; meta?: Record<string, unknown>; error?: string }>;
  createMap: (width: number, height: number, description: string) => void;
  updateTile: (layerType: 'bg' | 'fg', layerIdx: number, x: number, y: number, tile_id: number, flip?: number, lum?: number) => void;
  patchTile: (layerType: 'bg' | 'fg', layerIdx: number, x: number, y: number, patch: Partial<TileCell>) => void;
  applyTileBatch: (layerType: 'bg' | 'fg', layerIdx: number, updates: Array<{ x: number; y: number; tile_id: number; flip: number; lum: number }>) => void;
  applyAllLayersBatch: (patches: Array<{ layerType: 'bg' | 'fg'; layerIdx: number; updates: Array<{ x: number; y: number; tile_id: number; flip: number; lum: number }> }>) => void;
  beginPaint: () => void;
  commitPaint: () => void;
  addPlatform: (platform: MapPlatform) => void;
  removePlatform: (idx: number) => void;
  updatePlatform: (idx: number, x1: number, y1: number, x2: number, y2: number) => void;
  addActor: (actor: MapActor) => void;
  removeActor: (idx: number) => void;
  updateActor: (idx: number, patch: Partial<MapActor>) => void;
  moveActor: (idx: number, x: number, y: number) => void;
  addShadowZone: (zone: MapShadowZone) => void;
  removeShadowZone: (idx: number) => void;
  updateHeader: (patch: Partial<MapHeader>) => void;
  undo: () => void;
  redo: () => void;
  canUndo: boolean;
  canRedo: boolean;
  resizeMap: (newWidth: number, newHeight: number) => void;
}

export function useSilMap(): UseSilMapReturn {
  const [mapData, setMapData] = useState<SilMapData | null>(() => mapStore.get());
  const [canUndo, setCanUndo] = useState(false);
  const [canRedo, setCanRedo] = useState(false);

  // Sync map state to module store on every change so it survives navigation
  useEffect(() => { if (mapData) mapStore.set(mapData); else mapStore.clear(); }, [mapData]);

  // History stacks stored in refs to avoid triggering re-renders on push/pop
  const historyRef   = useRef<SilMapData[]>([]); // past states
  const futureRef    = useRef<SilMapData[]>([]); // states after current (for redo)
  const prePaintRef  = useRef<SilMapData | null>(null); // snapshot before a paint stroke begins

  const syncUndoRedo = () => {
    setCanUndo(historyRef.current.length > 0);
    setCanRedo(futureRef.current.length > 0);
  };

  const pushHistory = useCallback((snapshot: SilMapData) => {
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

  const createMap = useCallback((width: number, height: number, description: string) => {
    const numCells = width * height;
    historyRef.current = [];
    futureRef.current = [];
    syncUndoRedo();
    setMapData({
      header: { firstbyte: 0, version: 0, maxplayers: 8, maxteams: 2, parallax: 0, ambience: -20, flags: 0, description: description || 'New Map' },
      width, height,
      fileName: 'new_map.SIL',
      layers: {
        bg: [new Array(numCells).fill(null), new Array(numCells).fill(null), new Array(numCells).fill(null), new Array(numCells).fill(null)],
        fg: [new Array(numCells).fill(null), new Array(numCells).fill(null), new Array(numCells).fill(null), new Array(numCells).fill(null)],
      },
      actors: [],
      platforms: [],
      shadowZones: [],
      rawMinimap: new Uint8Array(0),
      minimapCompressedSize: 0,
    });
  }, []);


  const openMap = useCallback(async (file: File): Promise<SilMapData | null> => {
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
      const { platforms, offset: off3 } = parsePlatforms(levelDV, off2);
      const { shadowZones } = parseShadowZones(levelDV, off3);

      historyRef.current = [];
      futureRef.current = [];
      syncUndoRedo();
      const loaded: SilMapData = { header, width, height, layers, actors, platforms, shadowZones, rawMinimap, minimapCompressedSize, fileName: file.name };
      setMapData(loaded);
      return loaded;
    } catch (e) {
      console.error('Failed to open SIL map:', e);
      alert('Failed to parse map: ' + (e as Error).message);
      return null;
    }
  }, []);

  const saveMap = useCallback(async () => {
    if (!mapData) return;
    const { header, width, height, layers, actors, platforms, shadowZones, rawMinimap, fileName } = mapData;
    const numCells = width * height;
    const tileSectionSize = numCells * CELL_SIZE;
    const actorsSectionSize = 8 + actors.length * 36;
    const platformsSectionSize = 8 + platforms.length * 24;
    const shadowZonesSectionSize = 8 + shadowZones.length * 16; // always write header so C++ can find light masks

    // Bake shadow masks for all placed map lights fresh at save time
    const lightMasks = bakeMapLightMasks(actors, platforms, shadowZones);
    const lightMasksSectionSize = lightMasks.length > 0
      ? 8 + lightMasks.reduce((sum, m) => sum + 12 + m.data.length, 0)
      : 0;

    const levelBuf = new ArrayBuffer(tileSectionSize + actorsSectionSize + platformsSectionSize + shadowZonesSectionSize + lightMasksSectionSize);
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

    // Always write shadow zone header so the C++ parser can find trailing sections
    ldv.setUint32(off, shadowZones.length, true); off += 4;
    ldv.setUint32(off, 0, true); off += 4;
    for (const z of shadowZones) {
      ldv.setInt32(off,      z.x1, true);
      ldv.setInt32(off + 4,  z.y1, true);
      ldv.setInt32(off + 8,  z.x2, true);
      ldv.setInt32(off + 12, z.y2, true);
      off += 16;
    }

    // Light shadow masks section — baked per-pixel wall-occlusion for each placed map light
    if (lightMasks.length > 0) {
      ldv.setUint32(off, lightMasks.length, true); off += 4;
      ldv.setUint32(off, 0, true); off += 4;
      const levelBytes = new Uint8Array(levelBuf);
      for (const m of lightMasks) {
        ldv.setInt32(off,  m.x,    true); off += 4;
        ldv.setInt32(off,  m.y,    true); off += 4;
        ldv.setUint32(off, m.diam, true); off += 4;
        levelBytes.set(m.data, off);
        off += m.data.length;
      }
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

    // TODO: type File System Access API (non-standard browser API)
    if ((window as unknown as Record<string, unknown>).showSaveFilePicker) {
      try {
        const handle = await (window as unknown as {
          showSaveFilePicker: (opts: unknown) => Promise<{
            createWritable: () => Promise<{ write: (b: Blob) => Promise<void>; close: () => Promise<void> }>;
          }>;
        }).showSaveFilePicker({
          suggestedName,
          types: [{ description: 'Silencer Map', accept: { 'application/octet-stream': ['.SIL'] } }],
        });
        const writable = await handle.createWritable();
        await writable.write(blob);
        await writable.close();
        return;
      } catch (e) {
        if ((e as Error).name === 'AbortError') return; // user cancelled
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

  const updateTile = useCallback((layerType: 'bg' | 'fg', layerIdx: number, x: number, y: number, tile_id: number, flip = 0, lum = 255) => {
    setMapData(prev => {
      if (!prev) return prev;
      const { width, height, layers } = prev;
      if (x < 0 || x >= width || y < 0 || y >= height) return prev;
      const idx = y * width + x;
      const layerArr = layerType === 'fg' ? layers.fg : layers.bg;
      const newLayer = layerArr[layerIdx].slice();
      newLayer[idx] = { tile_id, flip, lum };
      const newLayers: typeof layers = {
        bg: layerType === 'bg' ? layers.bg.map((l, i) => i === layerIdx ? newLayer : l) : layers.bg,
        fg: layerType === 'fg' ? layers.fg.map((l, i) => i === layerIdx ? newLayer : l) : layers.fg,
      };
      return { ...prev, layers: newLayers };
    });
  }, []);

  const addPlatform = useCallback((platform: MapPlatform) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, platforms: [...prev.platforms, platform] };
    });
  }, [pushHistory]);

  const removePlatform = useCallback((idx: number) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, platforms: prev.platforms.filter((_, i) => i !== idx) };
    });
  }, [pushHistory]);

  const addActor = useCallback((actor: MapActor) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, actors: [...prev.actors, actor] };
    });
  }, [pushHistory]);

  const removeActor = useCallback((idx: number) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, actors: prev.actors.filter((_, i) => i !== idx) };
    });
  }, [pushHistory]);

  const updateActor = useCallback((idx: number, patch: Partial<MapActor>) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const actors = prev.actors.map((a, i) => i === idx ? { ...a, ...patch } : a);
      return { ...prev, actors };
    });
  }, [pushHistory]);

  const resizeMap = useCallback((newWidth: number, newHeight: number) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const { width, height, layers } = prev;
      const newSize = newWidth * newHeight;

      const resizeLayer = (layer: (TileCell | null)[]) => {
        const next: (TileCell | null)[] = new Array(newSize).fill(null);
        const copyW = Math.min(width, newWidth);
        const copyH = Math.min(height, newHeight);
        for (let row = 0; row < copyH; row++) {
          for (let col = 0; col < copyW; col++) {
            next[row * newWidth + col] = layer[row * width + col];
          }
        }
        return next;
      };

      const newLayers: typeof layers = {
        bg: layers.bg.map(resizeLayer),
        fg: layers.fg.map(resizeLayer),
      };

      // Clamp actors and platforms to new bounds (in world pixels)
      const maxX = newWidth * 64;
      const maxY = newHeight * 64;
      const actors      = prev.actors.filter(a => a.x < maxX && a.y < maxY);
      const platforms   = prev.platforms.filter(p => p.x1 < maxX && p.y1 < maxY);
      const shadowZones = prev.shadowZones.filter(z => z.x1 < maxX && z.y1 < maxY);

      return { ...prev, width: newWidth, height: newHeight, layers: newLayers, actors, platforms, shadowZones };
    });
  }, [pushHistory]);

  const updateHeader = useCallback((patch: Partial<MapHeader>) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, header: { ...prev.header, ...patch } };
    });
  }, [pushHistory]);

  const moveActor = useCallback((idx: number, x: number, y: number) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const actors = prev.actors.map((a, i) => i === idx ? { ...a, x, y } : a);
      return { ...prev, actors };
    });
  }, [pushHistory]);

  const updatePlatform = useCallback((idx: number, x1: number, y1: number, x2: number, y2: number) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const platforms = prev.platforms.map((p, i) => i === idx ? { ...p, x1, y1, x2, y2 } : p);
      return { ...prev, platforms };
    });
  }, [pushHistory]);

  const addShadowZone = useCallback((zone: MapShadowZone) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, shadowZones: [...prev.shadowZones, zone] };
    });
  }, [pushHistory]);

  const removeShadowZone = useCallback((idx: number) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      return { ...prev, shadowZones: prev.shadowZones.filter((_, i) => i !== idx) };
    });
  }, [pushHistory]);

  const applyTileBatch = useCallback((
    layerType: 'bg' | 'fg', layerIdx: number,
    updates: Array<{ x: number; y: number; tile_id: number; flip: number; lum: number }>
  ) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const { width, height, layers } = prev;
      const srcArr = layerType === 'fg' ? layers.fg : layers.bg;
      const newArr = srcArr.map((l, i) => i === layerIdx ? l.slice() : l);
      for (const { x, y, tile_id, flip, lum } of updates) {
        if (x >= 0 && x < width && y >= 0 && y < height)
          newArr[layerIdx][y * width + x] = { tile_id, flip, lum };
      }
      return { ...prev, layers: layerType === 'fg'
        ? { bg: layers.bg, fg: newArr }
        : { bg: newArr, fg: layers.fg } };
    });
  }, [pushHistory]);

  const applyAllLayersBatch = useCallback((
    patches: Array<{ layerType: 'bg' | 'fg'; layerIdx: number; updates: Array<{ x: number; y: number; tile_id: number; flip: number; lum: number }> }>
  ) => {
    setMapData(prev => {
      if (!prev) return prev;
      pushHistory(prev);
      const { width, height } = prev;
      let newBg = prev.layers.bg.map(l => l.slice());
      let newFg = prev.layers.fg.map(l => l.slice());
      for (const { layerType, layerIdx, updates } of patches) {
        const arr = layerType === 'fg' ? newFg : newBg;
        for (const { x, y, tile_id, flip, lum } of updates) {
          if (x >= 0 && x < width && y >= 0 && y < height)
            arr[layerIdx][y * width + x] = { tile_id, flip, lum };
        }
        if (layerType === 'fg') newFg = arr; else newBg = arr;
      }
      return { ...prev, layers: { bg: newBg as typeof prev.layers.bg, fg: newFg as typeof prev.layers.fg } };
    });
  }, [pushHistory]);

  const patchTile = useCallback((layerType: 'bg' | 'fg', layerIdx: number, x: number, y: number, patch: Partial<TileCell>) => {
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
      const newLayers: typeof layers = {
        bg: layerType === 'bg' ? layers.bg.map((l, i) => i === layerIdx ? newLayer : l) : layers.bg,
        fg: layerType === 'fg' ? layers.fg.map((l, i) => i === layerIdx ? newLayer : l) : layers.fg,
      };
      return { ...prev, layers: newLayers };
    });
  }, [pushHistory]);

  const publishMap = useCallback(async ({ author, apiUrl, apiKey }: { author: string; apiUrl: string; apiKey: string }): Promise<{ ok: boolean; meta?: Record<string, unknown>; error?: string }> => {
    if (!mapData) return { ok: false, error: 'No map loaded' };
    const { header, width, height, layers, actors, platforms, shadowZones, rawMinimap, fileName } = mapData;
    const numCells = width * height;
    const tileSectionSize = numCells * CELL_SIZE;
    const actorsSectionSize = 8 + actors.length * 36;
    const platformsSectionSize = 8 + platforms.length * 24;
    const shadowZonesSectionSize = 8 + shadowZones.length * 16; // always write header
    const lightMasks = bakeMapLightMasks(actors, platforms, shadowZones);
    const lightMasksSectionSize = lightMasks.length > 0
      ? 8 + lightMasks.reduce((sum, m) => sum + 12 + m.data.length, 0)
      : 0;
    const levelBuf = new ArrayBuffer(tileSectionSize + actorsSectionSize + platformsSectionSize + shadowZonesSectionSize + lightMasksSectionSize);
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

    // Always write shadow zone header so the C++ parser can find trailing sections
    ldv.setUint32(off, shadowZones.length, true); off += 4;
    ldv.setUint32(off, 0, true); off += 4;
    for (const z of shadowZones) {
      ldv.setInt32(off,      z.x1, true);
      ldv.setInt32(off + 4,  z.y1, true);
      ldv.setInt32(off + 8,  z.x2, true);
      ldv.setInt32(off + 12, z.y2, true);
      off += 16;
    }

    if (lightMasks.length > 0) {
      ldv.setUint32(off, lightMasks.length, true); off += 4;
      ldv.setUint32(off, 0, true); off += 4;
      const levelBytes = new Uint8Array(levelBuf);
      for (const m of lightMasks) {
        ldv.setInt32(off,  m.x,    true); off += 4;
        ldv.setInt32(off,  m.y,    true); off += 4;
        ldv.setUint32(off, m.diam, true); off += 4;
        levelBytes.set(m.data, off);
        off += m.data.length;
      }
    }

    const levelCompressed = pako.deflate(new Uint8Array(levelBuf));
    const descBytes = new TextEncoder().encode(header.description);
    const descBuf = new Uint8Array(128);
    descBuf.set(descBytes.slice(0, 127));
    const totalSize = 149 + rawMinimap.length + 4 + levelCompressed.length;
    if (totalSize > 65535) return { ok: false, error: `Map too large: ${totalSize} bytes (max 65535)` };

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

    const name = fileName || 'map.SIL';
    const headers: Record<string, string> = {
      'Content-Type': 'application/octet-stream',
      'X-Filename': name,
      'X-Author': author || 'anonymous',
    };
    if (apiKey) headers['X-Api-Key'] = apiKey;

    try {
      const resp = await fetch(`${apiUrl}/api/maps`, {
        method: 'POST',
        headers,
        body: fBytes,
      });
      if (!resp.ok) {
        const txt = await resp.text();
        return { ok: false, error: `Server error ${resp.status}: ${txt.trim()}` };
      }
      const data = await resp.json() as Record<string, unknown>;
      return { ok: true, meta: data };
    } catch (e) {
      return { ok: false, error: (e as Error).message };
    }
  }, [mapData]);

  return {
    map: mapData, openMap, saveMap, publishMap, createMap,
    updateTile, patchTile, applyTileBatch, applyAllLayersBatch, beginPaint, commitPaint,
    addPlatform, removePlatform, updatePlatform,
    addActor, removeActor, updateActor, moveActor,
    addShadowZone, removeShadowZone,
    updateHeader,
    undo, redo, canUndo, canRedo,
    resizeMap,
  };
}
