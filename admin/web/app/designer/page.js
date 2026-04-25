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

export default function DesignerPage() {
  useAuth();
  const wsConnected = useSocket({});

  const { loaded, error, tileImages, tileBankCounts, progress, loadFiles } = useGameData();
  const { map, openMap, saveMap, updateTile, beginPaint, commitPaint,
          addPlatform, removePlatform, addActor, removeActor,
          undo, redo, canUndo, canRedo } = useSilMap();

  const [activeTool, setActiveTool]     = useState('TILE_BG');
  const [activeLayer, setActiveLayer]   = useState(0);
  const [selectedTileId, setSelectedTile] = useState(0);
  const [selectedActorId, setSelectedActor] = useState(36); // player start default
  const [zoom, setZoom]   = useState(0.5);
  const [pan, setPan]     = useState({ x: 32, y: 32 });
  const [cursor, setCursor] = useState({ tx: 0, ty: 0, wx: 0, wy: 0 });
  const [dragPlatform, setDragPlatform] = useState(null);
  const [lumMode, setLumMode] = useState(false);

  const silInputRef  = useRef(null);
  const dataDirRef   = useRef(null);
  const dataFilesRef = useRef(null);

  // Global undo/redo keyboard shortcuts
  useEffect(() => {
    const handler = (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'z' && !e.shiftKey) { e.preventDefault(); undo(); }
      if ((e.ctrlKey || e.metaKey) && (e.key === 'y' || (e.key === 'z' && e.shiftKey))) { e.preventDefault(); redo(); }
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
          <button
            onClick={() => silInputRef.current?.click()}
            className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-text transition-colors"
          >
            OPEN .SIL
          </button>
          <input ref={silInputRef} type="file" accept=".SIL,.sil"
            className="hidden" onChange={handleOpenSil} />

          {/* Save */}
          {map && (
            <button
              onClick={saveMap}
              className="px-3 py-1 text-xs font-mono border border-game-primary text-game-primary rounded hover:bg-game-dark transition-colors"
            >
              SAVE .SIL
            </button>
          )}

          {/* Fit */}
          {map && (
            <button
              onClick={fitToScreen}
              className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-text transition-colors"
            >
              FIT
            </button>
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
        />

        {/* Main area: canvas + right panel */}
        <div className="flex flex-1 min-h-0">
          {/* Canvas area */}
          <div id="canvas-container" className="flex-1 relative min-w-0">
            <MapCanvas
              map={map}
              tileImages={tileImages}
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
              selectedActorId={selectedActorId}
              dragPlatform={dragPlatform}
              onDragPlatformChange={handleDragPlatform}
              onCursorChange={setCursor}
            />
          </div>

          {/* Right panel: tile picker */}
          <div className="w-72 flex-shrink-0 border-l border-game-border bg-game-bgCard overflow-hidden flex flex-col">
            <TilePicker
              tileImages={tileImages}
              tileBankCounts={tileBankCounts}
              selectedTileId={selectedTileId}
              onSelectTile={setSelectedTile}
            />
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
    </div>
  );
}
