'use client';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import { useState, useCallback, useRef, useEffect } from 'react';
import { useGameData } from './useGameData.js';
import { useSilMap } from './useSilMap.js';
import Toolbar, { ACTOR_DEFS, PLATFORM_TOOL_TYPES } from './Toolbar.js';
import MapCanvas from './MapCanvas.js';
import TilePicker from './TilePicker.js';
import ActorContextMenu from './ActorContextMenu.js';
import TileContextMenu from './TileContextMenu.js';
import MapPropertiesPanel from './MapPropertiesPanel.js';
import ActorListPanel from './ActorListPanel.js';
import Minimap from './Minimap.js';

export default function DesignerPage() {
  useAuth();
  const wsConnected = useSocket({});

  const { loaded, error, tileImages, spriteImages, tileBankCounts, progress, loadFiles } = useGameData();
  const { map, openMap, saveMap, publishMap, createMap, updateTile, patchTile, beginPaint, commitPaint,
          addPlatform, removePlatform, addActor, removeActor, updateActor, moveActor,
          updateHeader, updatePlatform,
          undo, redo, canUndo, canRedo, resizeMap } = useSilMap();

  const [activeTool, setActiveTool]     = useState('TILE_BG');
  const [activeLayer, setActiveLayer]   = useState(0);
  const [eraseLayerType, setEraseLayerType] = useState('bg');
  const [selectedTileId, setSelectedTile] = useState(0);
  const [selectedActorId, setSelectedActor] = useState(36); // player start default
  const [selectedPlatformIdx, setSelectedPlatformIdx] = useState(null);
  const [zoom, setZoom]   = useState(0.5);
  const [pan, setPan]     = useState({ x: 32, y: 32 });
  const [cursor, setCursor] = useState({ tx: 0, ty: 0, wx: 0, wy: 0 });
  const [dragPlatform, setDragPlatform] = useState(null);
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
  const [actorMenu, setActorMenu] = useState(null);
  const [tileMenu, setTileMenu] = useState(null);
  const [highlightActorIdx, setHighlightActorIdx] = useState(null);
  const [vis, setVis] = useState({
    bg: [true, true, true, true],
    fg: [true, true, true, true],
    platforms: true,
    actors: true,
    grid: true,
    lighting: true,
  });
  const toggleVis = (key, idx = null) => setVis(v => {
    if (idx !== null) {
      const arr = [...v[key]]; arr[idx] = !arr[idx]; return { ...v, [key]: arr };
    }
    return { ...v, [key]: !v[key] };
  });

  const defaultMapApiUrl = process.env.NEXT_PUBLIC_MAP_API_URL || '';
  const [showPublish, setShowPublish] = useState(false);
  const [pubApiUrl, setPubApiUrl]     = useState(defaultMapApiUrl);
  const [pubApiKey, setPubApiKey]     = useState('');
  const [pubAuthor, setPubAuthor]     = useState('');
  const [pubStatus, setPubStatus]     = useState(null); // { ok, msg }

  const handlePublish = useCallback(async () => {
    setPubStatus({ ok: null, msg: 'Publishing…' });
    const result = await publishMap({ author: pubAuthor, apiUrl: pubApiUrl, apiKey: pubApiKey });
    if (result.ok) {
      setPubStatus({ ok: true, msg: `✓ Published  sha1: ${result.meta.sha1.slice(0, 8)}…` });
    } else {
      setPubStatus({ ok: false, msg: result.error });
    }
  }, [publishMap, pubAuthor, pubApiUrl, pubApiKey]);

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

  const silInputRef  = useRef(null);
  const dataDirRef   = useRef(null);
  const dataFilesRef = useRef(null);

  // Global keyboard shortcuts
  useEffect(() => {
    const handler = (e) => {
      // Undo/Redo always takes priority
      if ((e.ctrlKey || e.metaKey) && e.key === 'z' && !e.shiftKey) { e.preventDefault(); undo(); return; }
      if ((e.ctrlKey || e.metaKey) && (e.key === 'y' || (e.key === 'z' && e.shiftKey))) { e.preventDefault(); redo(); return; }

      // Skip tool shortcuts when typing in an input
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT' || e.target.tagName === 'TEXTAREA') return;
      if (e.ctrlKey || e.metaKey || e.altKey) return;

      switch (e.key) {
        case 'b': setActiveTool('TILE_BG');    break;
        case 'f': setActiveTool('TILE_FG');    break;
        case 'e': setActiveTool('ERASE_TILE'); break;
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
  }, [undo, redo]);

  const handleDataDir = (e) => {
    if (e.target.files?.length) loadFiles(e.target.files);
  };
  const handleDataFiles = (e) => {
    if (e.target.files?.length) loadFiles(e.target.files);
  };

  const handleOpenSil = (e) => {
    if (e.target.files?.[0]) openMap(e.target.files[0]);
  };

  const handleTilePaint = useCallback((layerType, layerIdx, tx, ty, tileId) => {
    updateTile(layerType, layerIdx, tx, ty, tileId, 0, lumMode ? 1 : 0);
  }, [updateTile, lumMode]);

  const handlePlatformDraw = useCallback((platform) => {
    addPlatform(platform);
  }, [addPlatform]);

  const handleActorPlace = useCallback(({ wx, wy }) => {
    const def = ACTOR_DEFS.find(a => a.id === selectedActorId);
    addActor({
      id: selectedActorId,
      x: Math.round(wx),
      y: Math.round(wy),
      direction: 0,
      type: 0,
      matchid: 0,
      subplane: 0,
      unknown: 0,
      securityid: 0,
    });
  }, [selectedActorId, addActor]);

  const handleActorMove = useCallback((idx, x, y) => {
    moveActor(idx, x, y);
  }, [moveActor]);

  const canvasContainerRef = useRef(null);

  const handleCenterOnActor = useCallback((actor) => {
    const container = canvasContainerRef.current;
    if (!container) return;
    const { width: cw, height: ch } = container.getBoundingClientRect();
    setPan({ x: cw / 2 - actor.x * zoom, y: ch / 2 - actor.y * zoom });
  }, [zoom]);

  const handleMinimapPan = useCallback((wx, wy) => {
    const container = canvasContainerRef.current;
    if (!container) return;
    const { width: cw, height: ch } = container.getBoundingClientRect();
    setPan({ x: cw / 2 - wx * zoom, y: ch / 2 - wy * zoom });
  }, [zoom]);

  const handleDragPlatform = useCallback((valOrFn) => {
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
            title="Pick your game data/ folder"
          >
            LOAD DATA DIR
          </button>
          <input ref={dataDirRef} type="file" webkitdirectory=""
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
            <div className="relative">
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
                        placeholder="http://host:15172"
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
                {map.width}×{map.height} — "{map.header.description || 'untitled'}"
              </span>
            )}
          </div>
        </div>

        {/* Toolbar */}
        <Toolbar
          activeTool={activeTool}
          onToolChange={setActiveTool}
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
          />
        )}

        {/* Main area: canvas + right panel */}
        <div className="flex flex-1 min-h-0">
          {/* Canvas area */}
          <div id="canvas-container" ref={canvasContainerRef} className="flex-1 relative min-w-0 flex flex-col">

            {/* Visibility toggles */}
            <div className="flex items-center gap-1 px-2 py-1 bg-[#080c08] border-b border-[#1a2e1a] flex-shrink-0 flex-wrap">
              <span className="text-[#3a5a3a] text-xs font-mono mr-1">👁</span>
              {['BG0','BG1','BG2','BG3'].map((lbl, i) => (
                <button key={lbl} onClick={() => toggleVis('bg', i)}
                  className={`px-1.5 py-0.5 text-xs font-mono rounded border ${vis.bg[i] ? 'border-[#2a4a2a] text-[#80c080] bg-[#0d1a0d]' : 'border-[#1a1a1a] text-[#3a3a3a] bg-transparent line-through'}`}>
                  {lbl}
                </button>
              ))}
              <span className="text-[#1a3a1a] text-xs mx-0.5">|</span>
              {['FG0','FG1','FG2','FG3'].map((lbl, i) => (
                <button key={lbl} onClick={() => toggleVis('fg', i)}
                  className={`px-1.5 py-0.5 text-xs font-mono rounded border ${vis.fg[i] ? 'border-[#2a4a4a] text-[#80c0c0] bg-[#0d1a1a]' : 'border-[#1a1a1a] text-[#3a3a3a] bg-transparent line-through'}`}>
                  {lbl}
                </button>
              ))}
              <span className="text-[#1a3a1a] text-xs mx-0.5">|</span>
              {[['PLT','platforms'],['ACT','actors'],['GRID','grid'],['LIGHT','lighting']].map(([lbl, key]) => (
                <button key={key} onClick={() => toggleVis(key)}
                  className={`px-1.5 py-0.5 text-xs font-mono rounded border ${vis[key] ? 'border-[#3a3a2a] text-[#c0c080] bg-[#1a1a0d]' : 'border-[#1a1a1a] text-[#3a3a3a] bg-transparent line-through'}`}>
                  {lbl}
                </button>
              ))}
            </div>

            <div className="flex-1 relative min-h-0">
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
              onBeginPaint={beginPaint}
              onCommitPaint={commitPaint}
              onPlatformDraw={handlePlatformDraw}
              onPlatformRemove={removePlatform}
              onActorPlace={handleActorPlace}
              onActorRemove={removeActor}
              onActorMove={handleActorMove}
              onActorRightClick={(idx, sx, sy) => { setActorMenu({ idx, screenX: sx, screenY: sy }); setHighlightActorIdx(idx); }}
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

          {/* Right panel: tile picker + actor list */}
          <div className="w-72 flex-shrink-0 border-l border-game-border bg-game-bgCard overflow-hidden flex flex-col">
            <div className="flex-1 min-h-0 overflow-hidden flex flex-col">
              <TilePicker
                tileImages={tileImages}
                tileBankCounts={tileBankCounts}
                selectedTileId={selectedTileId}
                onSelectTile={setSelectedTile}
              />
            </div>
            {map && (
              <ActorListPanel
                actors={map.actors}
                highlightIdx={highlightActorIdx}
                onCenter={(actor) => { handleCenterOnActor(actor); setHighlightActorIdx(map.actors.indexOf(actor)); }}
                onActorRightClick={(idx, sx, sy) => { setActorMenu({ idx, screenX: sx, screenY: sy }); setHighlightActorIdx(idx); }}
              />
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
          <span className="text-xs font-mono text-game-muted ml-auto">
            CTRL/SPACE+DRAG or MMB to pan · SCROLL to zoom · CTRL+Z/Y to undo/redo
          </span>
        </div>
      </div>

      {/* Tile context menu (right-click on tile) */}
      {tileMenu && (
        <TileContextMenu
          menu={tileMenu}
          onPatch={(patch) => patchTile(tileMenu.layerType, tileMenu.layerIdx, tileMenu.tx, tileMenu.ty, patch)}
          onClear={() => updateTile(tileMenu.layerType, tileMenu.layerIdx, tileMenu.tx, tileMenu.ty, 0, 0, 0)}
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
              <span className="text-game-primary">G</span><span>Toggle grid</span>
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
