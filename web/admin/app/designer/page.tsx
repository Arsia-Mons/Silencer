'use client';
import { useAuth } from '../../lib/auth';
import { useSocket } from '../../lib/socket';
import Sidebar from '../../components/Sidebar';
import { useState, useCallback, useRef, useEffect } from 'react';
import { useGameData } from './useGameData';
import { useSilMap } from './useSilMap';
import Toolbar, { ACTOR_DEFS, PLATFORM_TOOL_TYPES } from './Toolbar';
import MapCanvas from './MapCanvas';
import type { DragPlatform } from './MapCanvas';
import TilePicker from './TilePicker';
import ActorContextMenu from './ActorContextMenu';
import TileContextMenu from './TileContextMenu';
import type { TileMenuInfo } from './TileContextMenu';
import MapPropertiesPanel from './MapPropertiesPanel';
import ActorListPanel from './ActorListPanel';
import Minimap from './Minimap';
import type { MapActor } from '../../lib/types';
import { API } from '../../lib/api';
import { useLightsStore } from '../../lib/lights-store';

interface VisState {
  bg: boolean[];
  fg: boolean[];
  platforms: boolean;
  actors: boolean;
  grid: boolean;
  lighting: boolean;
  parallax: boolean;
}

interface ActorMenu {
  idx: number;
  screenX: number;
  screenY: number;
}

interface PubStatus {
  ok: boolean | null;
  msg: string;
}

export default function DesignerPage() {
  useAuth();
  const wsConnected = useSocket({});

  const { loaded, error, tileImages, spriteImages, tileBankCounts, progress, loadFiles } = useGameData();
  const { lights: lightDefs, load: loadLights } = useLightsStore();
  useEffect(() => { loadLights(); }, [loadLights]);
  const { map, openMap, saveMap, publishMap, createMap, updateTile, patchTile, applyTileBatch, applyAllLayersBatch, beginPaint, commitPaint,
          addPlatform, removePlatform, addActor, removeActor, updateActor, moveActor,
          updateHeader, updatePlatform, addShadowZone, removeShadowZone,
          undo, redo, canUndo, canRedo, resizeMap } = useSilMap();

  type TileSel = { tx1: number; ty1: number; tx2: number; ty2: number; layerType: 'bg' | 'fg'; layerIdx: number };
  type TileCell = { tile_id: number; flip: number; lum: number };
  // All 8 layers: bg[0..3] and fg[0..3], each a flat w*h array
  type TileCopyBuf = {
    w: number; h: number;
    bg: [TileCell[], TileCell[], TileCell[], TileCell[]];
    fg: [TileCell[], TileCell[], TileCell[], TileCell[]];
  };

  const [activeTool, setActiveTool]     = useState('TILE_BG');
  const [activeLayer, setActiveLayer]   = useState(0);
  const [eraseLayerType, setEraseLayerType] = useState('bg');
  const [selectedTileId, setSelectedTile] = useState(0);
  const [selectedActorId, setSelectedActor] = useState(36); // player start default
  const [selectedPlatformIdx, setSelectedPlatformIdx] = useState<number | null>(null);
  const [tileLayerType, setTileLayerType] = useState<'bg' | 'fg'>('bg');
  const [tileSelection, setTileSelection] = useState<TileSel | null>(null);
  const [tileCopyBuffer, setTileCopyBuffer] = useState<TileCopyBuf | null>(null);
  const [pastePending, setPastePending] = useState(false);
  const [zoom, setZoom]   = useState(0.5);
  const [pan, setPan]     = useState({ x: 32, y: 32 });
  const fitPendingRef = useRef(!!map); // true on return-navigation so we auto-fit the restored map
  const [cursor, setCursor] = useState<{ tx: number; ty: number; wx: number; wy: number }>({ tx: 0, ty: 0, wx: 0, wy: 0 });
  const [dragPlatform, setDragPlatform] = useState<DragPlatform | null>(null);
  const [lumMode, setLumMode] = useState(false);
  const [resizeW, setResizeW] = useState('');
  const [resizeH, setResizeH] = useState('');
  const [showResize, setShowResize] = useState(false);
  const [showNewMap, setShowNewMap] = useState(false);
  const [newMapW, setNewMapW] = useState('40');
  const [newMapH, setNewMapH] = useState('30');
  const [newMapDesc, setNewMapDesc] = useState('New Map');
  const [showProps, setShowProps] = useState(false);
  const [showHotkeys, setShowHotkeys] = useState(false);
  const [actorMenu, setActorMenu] = useState<ActorMenu | null>(null);
  const [tileMenu, setTileMenu] = useState<TileMenuInfo | null>(null);
  const [highlightActorIdx, setHighlightActorIdx] = useState<number | null>(null);
  const [rightTab, setRightTab] = useState<'tiles' | 'actors'>('tiles');
  const [vis, setVis] = useState<VisState>({
    bg: [true, true, true, true],
    fg: [true, true, true, true],
    platforms: true,
    actors: true,
    grid: true,
    lighting: true,
    parallax: true,
  });
  const [gridSize, setGridSize] = useState(16);
  const toggleVis = (key: keyof VisState, idx: number | null = null) => setVis(v => {
    if (idx !== null) {
      const arr = [...(v[key] as boolean[])]; arr[idx] = !arr[idx]; return { ...v, [key]: arr };
    }
    return { ...v, [key]: !(v[key] as boolean) };
  });

  const defaultMapApiUrl = process.env.NEXT_PUBLIC_MAP_API_URL || '';
  const [showPublish, setShowPublish] = useState(false);
  const [pubApiUrl, setPubApiUrl]     = useState(defaultMapApiUrl);
  const [pubApiKey, setPubApiKey]     = useState('');
  const [pubAuthor, setPubAuthor]     = useState('');
  const [pubStatus, setPubStatus]     = useState<PubStatus | null>(null);

  // Published maps panel
  const [showMaps, setShowMaps]             = useState(false);
  const [mapList, setMapList]               = useState<Array<{ sha1: string; name: string; size: number; author: string; uploaded_at: string }>>([]);
  const [mapListLoading, setMapListLoading] = useState(false);
  const [mapListError, setMapListError]     = useState<string | null>(null);
  const [deleteStatus, setDeleteStatus]     = useState<Record<string, string>>({});
  const [lastPublishedSha1, setLastPublishedSha1] = useState<string | null>(null);

  const fetchMapList = useCallback(async () => {
    setMapListLoading(true);
    setMapListError(null);
    try {
      const r = await fetch(`${API}/maps`);
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      const data = await r.json();
      const sorted = [...data].sort((a: { uploaded_at: string }, b: { uploaded_at: string }) =>
        new Date(b.uploaded_at).getTime() - new Date(a.uploaded_at).getTime()
      );
      setMapList(sorted);
    } catch (e) {
      setMapListError(e instanceof Error ? e.message : 'Failed to load');
    } finally {
      setMapListLoading(false);
    }
  }, []);

  useEffect(() => {
    if (!showMaps) return;
    fetchMapList();
    const id = setInterval(fetchMapList, 4000);
    return () => clearInterval(id);
  }, [showMaps, fetchMapList]);

  const handleDeleteMap = useCallback(async (name: string) => {
    if (!confirm(`Delete "${name}" from the server?`)) return;
    setDeleteStatus(s => ({ ...s, [name]: 'deleting…' }));
    try {
      const headers: Record<string, string> = {};
      if (pubApiKey) headers['X-Api-Key'] = pubApiKey;
      const r = await fetch(`${API}/maps/${encodeURIComponent(name)}`, { method: 'DELETE', headers });
      if (r.ok) {
        setDeleteStatus(s => ({ ...s, [name]: '✓ deleted' }));
        fetchMapList();
      } else {
        const body = await r.json().catch(() => ({ error: r.statusText }));
        setDeleteStatus(s => ({ ...s, [name]: `✗ ${body.error ?? r.statusText}` }));
      }
    } catch {
      setDeleteStatus(s => ({ ...s, [name]: '✗ network error' }));
    }
  }, [pubApiKey, fetchMapList]);

  const handlePublish = useCallback(async () => {
    setPubStatus({ ok: null, msg: 'Publishing…' });
    const result = await publishMap({ author: pubAuthor, apiUrl: pubApiUrl, apiKey: pubApiKey });
    if (result.ok) {
      const sha1 = String((result.meta as Record<string, unknown>)?.sha1 ?? '');
      setPubStatus({ ok: true, msg: `✓ Published  sha1: ${sha1.slice(0, 8)}…` });
      setLastPublishedSha1(sha1);
      fetchMapList();
    } else {
      setPubStatus({ ok: false, msg: result.error ?? 'Unknown error' });
    }
  }, [publishMap, pubAuthor, pubApiUrl, pubApiKey, fetchMapList]);

  // Sync resize inputs when map changes
  useEffect(() => {
    if (map) { setResizeW(String(map.width)); setResizeH(String(map.height)); }
  }, [map?.width, map?.height]); // eslint-disable-line react-hooks/exhaustive-deps

  // Clear platform selection when switching away from SELECT tool
  useEffect(() => {
    if (activeTool !== 'SELECT') setSelectedPlatformIdx(null);
  }, [activeTool]);

  const applyResize = () => {
    const w = parseInt(resizeW, 10);
    const h = parseInt(resizeH, 10);
    if (!w || !h || w < 1 || h < 1 || w > 512 || h > 512) return;
    resizeMap(w, h);
    setShowResize(false);
  };

  const silInputRef  = useRef<HTMLInputElement>(null);
  const dataDirRef   = useRef<HTMLInputElement>(null);
  const dataFilesRef = useRef<HTMLInputElement>(null);


  // Tool change: track tile layer type and clear selection/paste on tool switch
  const handleToolChange = useCallback((tool: string) => {
    if (tool === 'TILE_BG') setTileLayerType('bg');
    else if (tool === 'TILE_FG') setTileLayerType('fg');
    if (tool !== 'TILE_SELECT') { setTileSelection(null); setPastePending(false); }
    setActiveTool(tool);
  }, []);

  // Ref to access latest copy/paste context from stable keyboard handler
  const clipCtxRef = useRef({ tileSelection, tileCopyBuffer, map, activeLayer, tileLayerType, pastePending });
  useEffect(() => { clipCtxRef.current = { tileSelection, tileCopyBuffer, map, activeLayer, tileLayerType, pastePending }; });

  // Copy/paste keyboard shortcuts (Ctrl/Cmd+C, Ctrl/Cmd+V, Escape)
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'c') {
        const { tileSelection: sel, map: m } = clipCtxRef.current;
        if (!sel || !m) return;
        e.preventDefault();
        const { tx1, ty1, tx2, ty2 } = sel;
        const w = tx2 - tx1 + 1; const h = ty2 - ty1 + 1;
        const extractLayer = (arr: Array<{ tile_id: number; flip: number; lum: number } | null>) => {
          const out: TileCell[] = [];
          for (let dy = 0; dy < h; dy++)
            for (let dx = 0; dx < w; dx++)
              out.push(arr[(ty1 + dy) * m.width + (tx1 + dx)] ?? { tile_id: 0, flip: 0, lum: 0 });
          return out;
        };
        setTileCopyBuffer({
          w, h,
          bg: [
            extractLayer(m.layers.bg[0]),
            extractLayer(m.layers.bg[1]),
            extractLayer(m.layers.bg[2]),
            extractLayer(m.layers.bg[3]),
          ],
          fg: [
            extractLayer(m.layers.fg[0]),
            extractLayer(m.layers.fg[1]),
            extractLayer(m.layers.fg[2]),
            extractLayer(m.layers.fg[3]),
          ],
        });
        setPastePending(false);
      } else if ((e.ctrlKey || e.metaKey) && e.key === 'v') {
        if (!clipCtxRef.current.tileCopyBuffer) return;
        e.preventDefault();
        setPastePending(p => !p);
      } else if (e.key === 'Escape') {
        setPastePending(false);
      }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, []); // stable — reads via ref

  // Global keyboard shortcuts
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      // Undo/Redo always takes priority
      if ((e.ctrlKey || e.metaKey) && e.key === 'z' && !e.shiftKey) { e.preventDefault(); undo(); return; }
      if ((e.ctrlKey || e.metaKey) && (e.key === 'y' || (e.key === 'z' && e.shiftKey))) { e.preventDefault(); redo(); return; }

      // Skip tool shortcuts when typing in an input
      if ((e.target as HTMLElement).tagName === 'INPUT' || (e.target as HTMLElement).tagName === 'SELECT' || (e.target as HTMLElement).tagName === 'TEXTAREA') return;
      if (e.ctrlKey || e.metaKey || e.altKey) return;

      switch (e.key) {
        case 'b': handleToolChange('TILE_BG');    break;
        case 'f': handleToolChange('TILE_FG');    break;
        case 'e': handleToolChange('ERASE_TILE'); break;
        case 'i': handleToolChange('FLOOD_FILL'); break;
        case 'p': setActiveTool('RECT');       break;
        case 'a': setActiveTool('ACTOR');      break;
        case 's': setActiveTool('SELECT');     break;
        case '1': setActiveLayer(0); break;
        case '2': setActiveLayer(1); break;
        case '3': setActiveLayer(2); break;
        case '4': setActiveLayer(3); break;
        case 'g': setVis(v => ({ ...v, grid: !v.grid })); break;
        case 'l': setVis(v => ({ ...v, lighting: !v.lighting })); break;
        case '?': setShowHotkeys(h => !h); break;
        default: break;
      }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [undo, redo, handleToolChange]);

  const handleDataDir = (e: React.ChangeEvent<HTMLInputElement>) => {
    if (e.target.files?.length) loadFiles(e.target.files);
  };
  const handleDataFiles = (e: React.ChangeEvent<HTMLInputElement>) => {
    if (e.target.files?.length) loadFiles(e.target.files);
  };

  const handleOpenSil = async (e: React.ChangeEvent<HTMLInputElement>) => {
    if (!e.target.files?.[0]) return;
    fitPendingRef.current = true;
    await openMap(e.target.files[0]);
  };

  const handleTilePaint = useCallback((layerType: 'bg' | 'fg', layerIdx: number, tx: number, ty: number, tileId: number) => {
    updateTile(layerType, layerIdx, tx, ty, tileId, 0, lumMode ? 1 : 0);
  }, [updateTile, lumMode]);

  const handlePlatformDraw = useCallback((platform: Parameters<typeof addPlatform>[0]) => {
    addPlatform(platform);
  }, [addPlatform]);

  const handleActorPlace = useCallback(({ wx, wy }: { wx: number; wy: number }) => {
    let type = 0;
    if (selectedActorId === 71) {
      // Light actor: seed actortype from matching LightDef (size from frame, color from defaultColor)
      const lightDef = lightDefs.find(l => l.frame === type);
      if (lightDef?.defaultColor) {
        const m = /^#?([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(lightDef.defaultColor);
        if (m) {
          const r = parseInt(m[1], 16), g = parseInt(m[2], 16), b = parseInt(m[3], 16);
          type = (type & 3) | ((r & 0xFF) << 8) | ((g & 0xFF) << 16) | ((b & 0xFF) << 24);
        }
      }
    }
    addActor({
      id: selectedActorId,
      x: Math.round(wx),
      y: Math.round(wy),
      direction: 0,
      type,
      matchid: 0,
      subplane: 0,
      unknown: 0,
      securityid: 0,
    });
  }, [selectedActorId, addActor, lightDefs]);

  const handleActorMove = useCallback((idx: number, x: number, y: number) => {
    moveActor(idx, x, y);
  }, [moveActor]);

  const handleActorFlip = useCallback((idx: number) => {
    const actor = map?.actors[idx];
    if (!actor) return;
    updateActor(idx, { direction: actor.direction ? 0 : 1 });
  }, [map, updateActor]);

  const handleShadowZoneDraw = useCallback((zone: Parameters<typeof addShadowZone>[0]) => {
    addShadowZone(zone);
  }, [addShadowZone]);

  const handleShadowZoneRemove = useCallback((idx: number) => {
    removeShadowZone(idx);
  }, [removeShadowZone]);

  const handleTilePaste = useCallback((tx: number, ty: number) => {
    if (!tileCopyBuffer || !map) return;
    const { w, bg, fg } = tileCopyBuffer;
    const patches = [
      ...bg.map((cells, li) => ({ layerType: 'bg' as const, layerIdx: li, updates: cells.map((t, i) => ({ x: tx + (i % w), y: ty + Math.floor(i / w), ...t })) })),
      ...fg.map((cells, li) => ({ layerType: 'fg' as const, layerIdx: li, updates: cells.map((t, i) => ({ x: tx + (i % w), y: ty + Math.floor(i / w), ...t })) })),
    ];
    applyAllLayersBatch(patches);
  }, [tileCopyBuffer, map, applyAllLayersBatch]);

  const canvasContainerRef = useRef<HTMLDivElement>(null);

  const handleCenterOnActor = useCallback((actor: MapActor) => {
    const container = canvasContainerRef.current;
    if (!container) return;
    const { width: cw, height: ch } = container.getBoundingClientRect();
    setPan({ x: cw / 2 - actor.x * zoom, y: ch / 2 - actor.y * zoom });
  }, [zoom]);

  const handleMinimapPan = useCallback((wx: number, wy: number) => {
    const container = canvasContainerRef.current;
    if (!container) return;
    const { width: cw, height: ch } = container.getBoundingClientRect();
    setPan({ x: cw / 2 - wx * zoom, y: ch / 2 - wy * zoom });
  }, [zoom]);

  const handleDragPlatform = useCallback((valOrFn: DragPlatform | null | ((prev: DragPlatform | null) => DragPlatform | null)) => {
    if (typeof valOrFn === 'function') {
      setDragPlatform(prev => {
        const next = valOrFn(prev);
        // Annotate with typeName for rendering
        if (next && next.tool) {
          const t = PLATFORM_TOOL_TYPES[next.tool] ?? PLATFORM_TOOL_TYPES.RECT;
          return { ...next, typeName: t.typeName };
        }
        return next;
      });
    } else {
      if (valOrFn && valOrFn.tool) {
        const t = PLATFORM_TOOL_TYPES[valOrFn.tool] ?? PLATFORM_TOOL_TYPES.RECT;
        setDragPlatform({ ...valOrFn, typeName: t.typeName });
      } else {
        setDragPlatform(valOrFn);
      }
    }
  }, []);

  const fitToScreen = () => {
    if (!map) return;
    const container = document.getElementById('canvas-container');
    if (!container) return;
    const { width: cw, height: ch } = container.getBoundingClientRect();
    const zx = cw / (map.width * 64);
    const zy = ch / (map.height * 64);
    const newZoom = Math.min(zx, zy) * 0.95;
    setZoom(newZoom);
    setPan({ x: (cw - map.width * 64 * newZoom) / 2, y: (ch - map.height * 64 * newZoom) / 2 });
  };

  // Run fitToScreen after React commits a new map — rAF lets the browser paint first
  useEffect(() => {
    if (!fitPendingRef.current || !map) return;
    fitPendingRef.current = false;
    const { width, height } = map;
    requestAnimationFrame(() => {
      const container = document.getElementById('canvas-container');
      if (!container) return;
      const { width: cw, height: ch } = container.getBoundingClientRect();
      const newZoom = Math.min(cw / (width * 64), ch / (height * 64)) * 0.95;
      setZoom(newZoom);
      setPan({ x: (cw - width * 64 * newZoom) / 2, y: (ch - height * 64 * newZoom) / 2 });
    });
  }, [map]);

  const isLoading = progress.total > 0 && progress.done < progress.total;

  return (
    <div className="flex h-screen overflow-hidden">
      <Sidebar wsConnected={wsConnected} />

      <div className="flex-1 flex flex-col min-w-0 overflow-hidden">
        {/* Top bar */}
        <div className="flex items-center gap-3 px-4 py-2 bg-game-bgCard border-b border-game-border flex-shrink-0">
          <h1 className="text-game-primary font-mono text-sm tracking-widest">◫ MAP DESIGNER</h1>
          <div className="w-px h-4 bg-game-border" />

          {/* Load game data — directory input (works on HTTP) or individual files fallback */}
          <button
            onClick={() => dataDirRef.current?.click()}
            className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-text transition-colors"
            title="Pick your game assets/ folder"
          >
            LOAD DATA DIR
          </button>
          <input ref={dataDirRef} type="file"
            {...({ webkitdirectory: '' } as unknown as React.InputHTMLAttributes<HTMLInputElement>)}
            className="hidden" onChange={handleDataDir} />
          <button
            onClick={() => dataFilesRef.current?.click()}
            className="px-3 py-1 text-xs font-mono border border-game-border text-game-muted rounded hover:border-game-border hover:text-game-textDim transition-colors"
            title="Fallback: manually select PALETTE.BIN + BIN_TIL.DAT + TIL_XXX.BIN files"
          >
            FILES…
          </button>
          <input ref={dataFilesRef} type="file" multiple accept=".bin,.dat,.BIN,.DAT"
            className="hidden" onChange={handleDataFiles} />

          {/* Undo / Redo */}
          <button onClick={undo} disabled={!canUndo}
            className="px-3 py-1 text-xs font-mono border border-game-border rounded transition-colors disabled:opacity-30 disabled:cursor-not-allowed text-game-textDim hover:border-game-primary hover:text-game-text"
            title="Undo (Ctrl+Z)">
            ↩ UNDO
          </button>
          <button onClick={redo} disabled={!canRedo}
            className="px-3 py-1 text-xs font-mono border border-game-border rounded transition-colors disabled:opacity-30 disabled:cursor-not-allowed text-game-textDim hover:border-game-primary hover:text-game-text"
            title="Redo (Ctrl+Y)">
            ↪ REDO
          </button>
          <div className="w-px h-4 bg-game-border" />
          {/* Open */}
          <button
            onClick={() => silInputRef.current?.click()}
            className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-text transition-colors"
          >
            OPEN .SIL
          </button>
          <input ref={silInputRef} type="file" accept=".SIL,.sil"
            className="hidden" onChange={handleOpenSil} />

          {/* New Map */}
          <div className="relative">
            <button
              onClick={() => setShowNewMap(n => !n)}
              className={`px-3 py-1 text-xs font-mono border rounded transition-colors ${showNewMap ? 'border-game-primary text-game-primary' : 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-text'}`}
            >
              + NEW
            </button>
            {showNewMap && (
              <div className="absolute top-full left-0 mt-1 z-50 bg-game-bgCard border border-game-border rounded p-3 flex flex-col gap-2 shadow-xl min-w-[200px]">
                <div className="text-xs text-game-textDim font-mono tracking-wider">NEW MAP</div>
                <div className="flex items-center gap-2">
                  <span className="text-xs text-game-textDim font-mono w-8">W</span>
                  <input type="number" min="1" max="512" value={newMapW}
                    onChange={e => setNewMapW(e.target.value)}
                    className="w-20 px-2 py-1 text-xs font-mono bg-game-dark border border-game-border text-game-text rounded focus:border-game-primary outline-none" />
                </div>
                <div className="flex items-center gap-2">
                  <span className="text-xs text-game-textDim font-mono w-8">H</span>
                  <input type="number" min="1" max="512" value={newMapH}
                    onChange={e => setNewMapH(e.target.value)}
                    className="w-20 px-2 py-1 text-xs font-mono bg-game-dark border border-game-border text-game-text rounded focus:border-game-primary outline-none" />
                </div>
                <div className="flex items-center gap-2">
                  <span className="text-xs text-game-textDim font-mono w-8 text-[10px]">DESC</span>
                  <input type="text" value={newMapDesc}
                    onChange={e => setNewMapDesc(e.target.value)}
                    className="flex-1 px-2 py-1 text-xs font-mono bg-game-dark border border-game-border text-game-text rounded focus:border-game-primary outline-none" />
                </div>
                <div className="flex gap-2">
                  <button onClick={() => {
                    const w = parseInt(newMapW, 10);
                    const h = parseInt(newMapH, 10);
                    if (!w || !h || w < 1 || h < 1 || w > 512 || h > 512) return;
                    createMap(w, h, newMapDesc);
                    setShowNewMap(false);
                    fitPendingRef.current = true;
                  }}
                    className="flex-1 py-1 text-xs font-mono border border-game-primary text-game-primary rounded hover:bg-game-dark transition-colors">
                    CREATE
                  </button>
                  <button onClick={() => setShowNewMap(false)}
                    className="px-2 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary transition-colors">
                    ✕
                  </button>
                </div>
              </div>
            )}
          </div>

          {/* Save */}
          {map && (
            <button
              onClick={saveMap}
              className="px-3 py-1 text-xs font-mono border border-game-primary text-game-primary rounded hover:bg-game-dark transition-colors"
            >
              SAVE .SIL
            </button>
          )}

          {/* Publish */}
          {map && (
            <div className="relative flex items-center">
              <button
                onClick={() => { setShowPublish(p => !p); setPubStatus(null); }}
                className={`px-3 py-1 text-xs font-mono border rounded transition-colors ${showPublish ? 'border-game-primary text-game-primary bg-game-dark' : 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-text'}`}
              >
                ⬆ PUBLISH
              </button>
              {showPublish && (
                <div className="absolute top-8 left-0 z-50 bg-game-bgCard border border-game-border rounded p-3 w-72 shadow-lg">
                  <div className="text-xs font-mono text-game-primary mb-2">Publish to Map Server</div>
                  <div className="flex flex-col gap-2">
                    <div>
                      <label className="text-xs text-game-textDim">Server URL</label>
                      <input
                        value={pubApiUrl}
                        onChange={e => setPubApiUrl(e.target.value)}
                        placeholder="(leave blank to use this server's /api/maps)"
                        className="w-full mt-1 px-2 py-1 text-xs font-mono bg-game-bg border border-game-border rounded text-game-text focus:outline-none focus:border-game-primary"
                      />
                    </div>
                    <div>
                      <label className="text-xs text-game-textDim">API Key <span className="text-game-textDim opacity-60">(leave blank if none)</span></label>
                      <input
                        type="password"
                        value={pubApiKey}
                        onChange={e => setPubApiKey(e.target.value)}
                        className="w-full mt-1 px-2 py-1 text-xs font-mono bg-game-bg border border-game-border rounded text-game-text focus:outline-none focus:border-game-primary"
                      />
                    </div>
                    <div>
                      <label className="text-xs text-game-textDim">Author</label>
                      <input
                        value={pubAuthor}
                        onChange={e => setPubAuthor(e.target.value)}
                        placeholder="anonymous"
                        className="w-full mt-1 px-2 py-1 text-xs font-mono bg-game-bg border border-game-border rounded text-game-text focus:outline-none focus:border-game-primary"
                      />
                    </div>
                    {pubStatus && (
                      <div className={`text-xs font-mono ${pubStatus.ok === true ? 'text-game-primary' : pubStatus.ok === false ? 'text-red-400' : 'text-game-textDim'}`}>
                        {pubStatus.msg}
                      </div>
                    )}
                    <div className="flex gap-2 mt-1">
                      <button
                        onClick={handlePublish}
                        className="flex-1 px-2 py-1 text-xs font-mono border border-game-primary text-game-primary rounded hover:bg-game-dark transition-colors"
                      >
                        Upload
                      </button>
                      <button
                        onClick={() => setShowPublish(false)}
                        className="px-2 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary transition-colors"
                      >
                        Close
                      </button>
                    </div>
                  </div>
                </div>
              )}
            </div>
          )}

          {/* Manage published maps */}
          <div className="relative flex items-center">
            <button
              onClick={() => setShowMaps(p => !p)}
              className={`px-3 py-1 text-xs font-mono border rounded transition-colors ${showMaps ? 'border-game-primary text-game-primary bg-game-dark' : 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-text'}`}
            >
              📋 MAPS
            </button>
            {showMaps && (
              <div className="absolute top-8 left-0 z-50 bg-game-bgCard border border-game-border rounded p-3 w-96 shadow-lg">
                <div className="flex items-center justify-between mb-2">
                  <div className="text-xs font-mono text-game-primary">Published Maps</div>
                  <button
                    onClick={fetchMapList}
                    className="text-xs font-mono text-game-textDim hover:text-game-text border border-game-border rounded px-2 py-0.5 transition-colors"
                    title="Refresh"
                  >↻ refresh</button>
                </div>
                {pubApiKey === '' && (
                  <div className="mb-2">
                    <input
                      type="password"
                      placeholder="API key for delete (optional)"
                      onChange={e => setPubApiKey(e.target.value)}
                      className="w-full px-2 py-1 text-xs font-mono bg-game-bg border border-game-border rounded text-game-text focus:outline-none focus:border-game-primary"
                    />
                  </div>
                )}
                {mapListLoading && <div className="text-xs font-mono text-game-textDim">Loading…</div>}
                {mapListError  && <div className="text-xs font-mono text-red-400">{mapListError}</div>}
                {!mapListLoading && !mapListError && mapList.length === 0 && (
                  <div className="text-xs font-mono text-game-textDim">No maps published yet.</div>
                )}
                {!mapListLoading && mapList.length > 0 && (
                  <div className="flex flex-col gap-1 max-h-72 overflow-y-auto">
                    {mapList.map(m => (
                      <div
                        key={m.sha1}
                        className={`flex items-center gap-2 px-2 py-1 rounded text-xs font-mono ${lastPublishedSha1 === m.sha1 ? 'bg-game-primary bg-opacity-10 border border-game-primary' : 'bg-game-bg'}`}
                      >
                        <div className="flex-1 min-w-0">
                          <div className="text-game-text truncate">{m.name}</div>
                          <div className="text-game-textDim text-[10px]">{m.author} · {(m.size / 1024).toFixed(1)}KB · {new Date(m.uploaded_at).toLocaleDateString()}</div>
                        </div>
                        {deleteStatus[m.name] ? (
                          <span className={`text-[10px] ${deleteStatus[m.name].startsWith('✓') ? 'text-game-primary' : 'text-red-400'}`}>
                            {deleteStatus[m.name]}
                          </span>
                        ) : (
                          <button
                            onClick={() => handleDeleteMap(m.name)}
                            className="text-[10px] text-red-400 hover:text-red-300 border border-red-800 hover:border-red-500 rounded px-1.5 py-0.5 transition-colors flex-shrink-0"
                          >
                            ✕
                          </button>
                        )}
                      </div>
                    ))}
                  </div>
                )}
                <button
                  onClick={() => setShowMaps(false)}
                  className="mt-2 w-full px-2 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary transition-colors"
                >
                  Close
                </button>
              </div>
            )}
          </div>

          {/* Props */}
          {map && (
            <button
              onClick={() => setShowProps(p => !p)}
              className={`px-3 py-1 text-xs font-mono border rounded transition-colors ${showProps ? 'border-game-primary text-game-primary' : 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-text'}`}
            >
              PROPS
            </button>
          )}

          {/* Hotkeys help */}
          <button
            onClick={() => setShowHotkeys(h => !h)}
            className="px-2 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-text transition-colors"
            title="Keyboard shortcuts"
          >
            ?
          </button>

          {/* Fit */}
          {map && (
            <button
              onClick={fitToScreen}
              className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-text transition-colors"
            >
              FIT
            </button>
          )}

          {/* Resize */}
          {map && (
            <div className="relative">
              <button
                onClick={() => setShowResize(r => !r)}
                className={`px-3 py-1 text-xs font-mono border rounded transition-colors ${showResize ? 'border-game-primary text-game-primary' : 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-text'}`}
              >
                ⊞ RESIZE
              </button>
              {showResize && (
                <div className="absolute top-full left-0 mt-1 z-50 bg-game-bgCard border border-game-border rounded p-3 flex flex-col gap-2 shadow-xl min-w-[160px]">
                  <div className="flex items-center gap-2">
                    <span className="text-xs text-game-textDim font-mono w-8">W</span>
                    <input
                      type="number" min="1" max="512" value={resizeW}
                      onChange={e => setResizeW(e.target.value)}
                      onKeyDown={e => e.key === 'Enter' && applyResize()}
                      className="w-20 px-2 py-1 text-xs font-mono bg-game-dark border border-game-border text-game-text rounded focus:border-game-primary outline-none"
                    />
                  </div>
                  <div className="flex items-center gap-2">
                    <span className="text-xs text-game-textDim font-mono w-8">H</span>
                    <input
                      type="number" min="1" max="512" value={resizeH}
                      onChange={e => setResizeH(e.target.value)}
                      onKeyDown={e => e.key === 'Enter' && applyResize()}
                      className="w-20 px-2 py-1 text-xs font-mono bg-game-dark border border-game-border text-game-text rounded focus:border-game-primary outline-none"
                    />
                  </div>
                  <div className="text-xs text-game-muted font-mono">Tiles outside bounds are removed</div>
                  <div className="flex gap-2">
                    <button onClick={applyResize}
                      className="flex-1 py-1 text-xs font-mono border border-game-primary text-game-primary rounded hover:bg-game-dark transition-colors">
                      APPLY
                    </button>
                    <button onClick={() => setShowResize(false)}
                      className="px-2 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary transition-colors">
                      ✕
                    </button>
                  </div>
                </div>
              )}
            </div>
          )}

          {/* Status badges */}
          <div className="flex items-center gap-2 ml-auto">
            {isLoading && (
              <span className="text-xs text-game-warning animate-pulse font-mono">
                DECODING {progress.done}/{progress.total} BANKS…
              </span>
            )}
            {loaded && !isLoading && (
              <span className="text-xs text-game-primary font-mono">
                ✓ {tileImages.size} BANKS LOADED
              </span>
            )}
            {error && (
              <span className="text-xs text-game-danger font-mono max-w-xs truncate" title={error}>
                ✕ {error}
              </span>
            )}
            {map && (
              <span className="text-xs text-game-textDim font-mono">
                {map.width}×{map.height} — &quot;{map.header.description || 'untitled'}&quot;
              </span>
            )}
          </div>
        </div>

        {/* Toolbar */}
        <Toolbar
          activeTool={activeTool}
          onToolChange={handleToolChange}
          activeLayer={activeLayer}
          onLayerChange={setActiveLayer}
          selectedActor={selectedActorId}
          onActorChange={setSelectedActor}
          lumMode={lumMode}
          onLumModeChange={setLumMode}
          eraseLayerType={eraseLayerType}
          onEraseLayerTypeChange={setEraseLayerType}
        />

        {/* Map Properties Panel */}
        {map && showProps && (
          <MapPropertiesPanel
            header={map.header}
            onUpdate={updateHeader}
            onClose={() => setShowProps(false)}
            spriteImages={spriteImages}
          />
        )}

        {/* Main area: canvas + right panel */}
        <div className="flex flex-1 min-h-0">
          {/* Canvas area */}
          <div id="canvas-container" ref={canvasContainerRef} className="flex-1 relative min-w-0 flex flex-col">

            {/* Visibility toggles */}
            <div className="flex items-center gap-1 px-2 py-1 bg-[#080c08] border-b border-[#1a2e1a] flex-shrink-0 flex-wrap">
              <span className="text-[#3a5a3a] text-xs font-mono mr-1">👁</span>
              {(['BG0','BG1','BG2','BG3'] as const).map((lbl, i) => (
                <button key={lbl} onClick={() => toggleVis('bg', i)}
                  className={`px-1.5 py-0.5 text-xs font-mono rounded border ${vis.bg[i] ? 'border-[#2a4a2a] text-[#80c080] bg-[#0d1a0d]' : 'border-[#1a1a1a] text-[#3a3a3a] bg-transparent line-through'}`}>
                  {lbl}
                </button>
              ))}
              <span className="text-[#1a3a1a] text-xs mx-0.5">|</span>
              {(['FG0','FG1','FG2','FG3'] as const).map((lbl, i) => (
                <button key={lbl} onClick={() => toggleVis('fg', i)}
                  className={`px-1.5 py-0.5 text-xs font-mono rounded border ${vis.fg[i] ? 'border-[#2a4a4a] text-[#80c0c0] bg-[#0d1a1a]' : 'border-[#1a1a1a] text-[#3a3a3a] bg-transparent line-through'}`}>
                  {lbl}
                </button>
              ))}
              <span className="text-[#1a3a1a] text-xs mx-0.5">|</span>
              {([['PLT','platforms'],['ACT','actors'],['PARA','parallax'],['GRID','grid'],['LIGHT','lighting']] as [string, keyof VisState][]).map(([lbl, key]) => (
                <button key={key} onClick={() => toggleVis(key)}
                  className={`px-1.5 py-0.5 text-xs font-mono rounded border ${vis[key] ? 'border-[#3a3a2a] text-[#c0c080] bg-[#1a1a0d]' : 'border-[#1a1a1a] text-[#3a3a3a] bg-transparent line-through'}`}>
                  {lbl}
                </button>
              ))}
              {vis.grid && (
                <>
                  <span className="text-[#1a3a1a] text-xs mx-0.5">⊞</span>
                  {([8, 16, 32, 64] as const).map(sz => (
                    <button key={sz} onClick={() => setGridSize(sz)}
                      className={`px-1.5 py-0.5 text-xs font-mono rounded border ${gridSize === sz ? 'border-[#3a3a2a] text-[#c0c080] bg-[#1a1a0d]' : 'border-[#1a1a1a] text-[#3a3a3a] bg-transparent'}`}>
                      {sz}
                    </button>
                  ))}
                </>
              )}
            </div>

            <div className="flex-1 relative min-h-0">
            {!loaded && !isLoading && (
              <div className="absolute inset-0 flex items-center justify-center z-10 bg-game-bg">
                <div className="flex flex-col items-center gap-4 p-12 border border-game-border rounded text-center">
                  <div className="text-4xl">🗺</div>
                  <div className="text-game-textDim text-xs font-mono leading-relaxed">
                    Select your game <code className="text-game-primary">shared/assets/</code> folder<br />
                    to load tile banks and sprite sheets.
                  </div>
                  <button
                    onClick={() => dataDirRef.current?.click()}
                    className="px-6 py-2 text-xs font-mono border border-game-primary text-game-primary rounded hover:bg-game-primary hover:text-game-bg transition-colors tracking-widest">
                    [ OPEN ASSETS FOLDER ]
                  </button>
                  {error && <div className="text-game-danger text-xs font-mono">{error}</div>}
                </div>
              </div>
            )}
            <MapCanvas
              map={map}
              tileImages={tileImages}
              spriteImages={spriteImages}
              vis={vis}
              activeTool={activeTool}
              activeLayer={activeLayer}
              selectedTileId={selectedTileId}
              zoom={zoom}
              pan={pan}
              onZoomChange={setZoom}
              onPanChange={setPan}
              onTilePaint={handleTilePaint}
              onFloodFill={applyTileBatch}
              onBeginPaint={beginPaint}
              onCommitPaint={commitPaint}
              onPlatformDraw={handlePlatformDraw}
              onPlatformRemove={removePlatform}
              onActorPlace={handleActorPlace}
              onActorRemove={removeActor}
              onActorMove={handleActorMove}
              onActorFlip={handleActorFlip}
              onActorRightClick={(idx, sx, sy) => { setActorMenu({ idx, screenX: sx, screenY: sy }); setHighlightActorIdx(idx); }}
              onActorSelect={setHighlightActorIdx}
              onTileRightClick={(info) => { setActorMenu(null); setTileMenu(info); }}
              selectedActorId={selectedActorId}
              dragPlatform={dragPlatform}
              onDragPlatformChange={handleDragPlatform}
              onCursorChange={setCursor}
              eraseLayerType={eraseLayerType}
              highlightActorIdx={highlightActorIdx}
              selectedPlatformIdx={selectedPlatformIdx}
              onPlatformSelect={setSelectedPlatformIdx}
              onPlatformUpdate={updatePlatform}
              onShadowZoneDraw={handleShadowZoneDraw}
              onShadowZoneRemove={handleShadowZoneRemove}
              gridSize={gridSize}
              tileSelection={tileSelection}
              onTileSelection={setTileSelection}
              tileCopyBuffer={tileCopyBuffer}
              pastePending={pastePending}
              onTilePaste={handleTilePaste}
            />
            <Minimap
              map={map}
              tileImages={tileImages}
              zoom={zoom}
              pan={pan}
              containerRef={canvasContainerRef}
              onPanTo={handleMinimapPan}
            />
            </div>
          </div>

          {/* Right panel: tiles / actors tabs */}
          <div className="w-72 flex-shrink-0 border-l border-game-border bg-game-bgCard overflow-hidden flex flex-col">
            {/* Tab bar */}
            <div className="flex border-b border-game-border shrink-0">
              {(['tiles', 'actors'] as const).map(tab => (
                <button key={tab} onClick={() => setRightTab(tab)}
                  className={`flex-1 py-1.5 text-[10px] font-mono tracking-widest transition-colors ${
                    rightTab === tab
                      ? 'text-game-primary border-b-2 border-game-primary bg-game-dark'
                      : 'text-game-textDim hover:text-game-text'
                  }`}>
                  {tab === 'tiles' ? 'TILES' : 'ACTORS'}
                </button>
              ))}
            </div>

            {rightTab === 'tiles' && (
              <div className="flex-1 min-h-0 overflow-hidden flex flex-col">
                <TilePicker
                  tileImages={tileImages}
                  tileBankCounts={tileBankCounts}
                  selectedTileId={selectedTileId}
                  onSelectTile={setSelectedTile}
                />
              </div>
            )}
            {rightTab === 'actors' && map && (
              <ActorListPanel
                actors={map.actors}
                highlightIdx={highlightActorIdx}
                onCenter={(actor) => { handleCenterOnActor(actor); setHighlightActorIdx(map.actors.indexOf(actor)); }}
                onActorRightClick={(idx, sx, sy) => { setActorMenu({ idx, screenX: sx, screenY: sy }); setHighlightActorIdx(idx); }}
              />
            )}
            {rightTab === 'actors' && !map && (
              <div className="flex-1 flex items-center justify-center p-4">
                <span className="text-[10px] font-mono text-game-textDim">No map loaded</span>
              </div>
            )}
          </div>
        </div>

        {/* Status bar */}
        <div className="flex items-center gap-4 px-4 py-1.5 bg-game-bgCard border-t border-game-border flex-shrink-0">
          <span className="text-xs font-mono text-game-textDim">
            TILE ({cursor.tx}, {cursor.ty})
          </span>
          <span className="text-xs font-mono text-game-muted">
            WORLD ({Math.round(cursor.wx)}, {Math.round(cursor.wy)})
          </span>
          <span className="text-xs font-mono text-game-muted">
            ZOOM {(zoom * 100).toFixed(0)}%
          </span>
          {map && (
            <span className="text-xs font-mono text-game-muted">
              MAP {map.width}×{map.height} | {map.actors.length} actors | {map.platforms.length} platforms
            </span>
          )}
          {tileCopyBuffer && (
            <span className="text-xs font-mono text-[#c0c040]">
              📋 {tileCopyBuffer.w}×{tileCopyBuffer.h} ALL LAYERS
              {pastePending ? ' — PASTE MODE (click to stamp · Esc to cancel)' : ' — Ctrl+V to paste'}
            </span>
          )}
          {tileSelection && !tileCopyBuffer && (
            <span className="text-xs font-mono text-[#80b0ff]">
              ⬚ {tileSelection.tx2 - tileSelection.tx1 + 1}×{tileSelection.ty2 - tileSelection.ty1 + 1} sel — Ctrl+C to copy
            </span>
          )}
        </div>
      </div>

      {/* Tile context menu (right-click on tile) */}
      {tileMenu && (
        <TileContextMenu
          menu={tileMenu}
          onPatch={(patch) => patchTile(tileMenu.layerType as 'bg' | 'fg', tileMenu.layerIdx, tileMenu.tx, tileMenu.ty, patch)}
          onClear={() => updateTile(tileMenu.layerType as 'bg' | 'fg', tileMenu.layerIdx, tileMenu.tx, tileMenu.ty, 0, 0, 0)}
          onClose={() => setTileMenu(null)}
        />
      )}

      {/* Actor context menu (right-click) */}
      {actorMenu && map?.actors[actorMenu.idx] && (
        <ActorContextMenu
          actor={map.actors[actorMenu.idx]}
          actorIdx={actorMenu.idx}
          screenX={actorMenu.screenX}
          screenY={actorMenu.screenY}
          onUpdate={updateActor}
          onDelete={removeActor}
          onClose={() => { setActorMenu(null); setHighlightActorIdx(null); }}
        />
      )}

      {/* Keyboard shortcut cheatsheet overlay */}
      {showHotkeys && (
        <div
          className="fixed inset-0 z-50 flex items-center justify-center bg-black/60"
          onClick={() => setShowHotkeys(false)}
        >
          <div
            className="bg-game-bgCard border border-game-border rounded p-5 font-mono text-xs shadow-2xl min-w-[320px]"
            onClick={e => e.stopPropagation()}
          >
            <div className="flex items-center justify-between mb-4">
              <span className="text-game-primary tracking-widest text-sm">KEYBOARD SHORTCUTS</span>
              <button onClick={() => setShowHotkeys(false)} className="text-game-textDim hover:text-game-text ml-4">✕</button>
            </div>
            <div className="grid grid-cols-2 gap-x-6 gap-y-1.5 text-game-textDim">
              <span className="text-game-primary">B</span><span>Tile BG tool</span>
              <span className="text-game-primary">F</span><span>Tile FG tool</span>
              <span className="text-game-primary">E</span><span>Erase tile</span>
              <span className="text-game-primary">P</span><span>Platform rect</span>
              <span className="text-game-primary">A</span><span>Actor tool</span>
              <span className="text-game-primary">S</span><span>Select / drag tool</span>
              <span className="text-game-primary">1–4</span><span>Layer 0–3</span>
              <span className="text-game-primary">G</span><span>Toggle grid snap</span>
              <span className="text-game-primary">L</span><span>Toggle lighting</span>
              <span className="text-game-primary">Ctrl+Z</span><span>Undo</span>
              <span className="text-game-primary">Ctrl+Y / Ctrl+Shift+Z</span><span>Redo</span>
              <span className="text-game-primary">Space + drag</span><span>Pan canvas</span>
              <span className="text-game-primary">Scroll</span><span>Zoom in/out</span>
              <span className="text-game-primary">?</span><span>Toggle this overlay</span>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
