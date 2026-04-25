'use client';
import { useRef, useEffect, useCallback } from 'react';

// Platform overlay colors
const PLATFORM_COLORS = {
  RECTANGLE:   'rgba(80,130,255,0.3)',
  LADDER:      'rgba(255,220,50,0.4)',
  STAIRSUP:    'rgba(50,220,100,0.4)',
  STAIRSDOWN:  'rgba(255,140,50,0.4)',
  TRACK:       'rgba(180,100,255,0.4)',
  OUTSIDEROOM: 'rgba(255,80,80,0.3)',
  SPECIFICROOM:'rgba(255,80,200,0.3)',
};

function platformTypeName(type1, type2) {
  if (type1 === 1 && type2 === 0) return 'LADDER';
  if (type1 === 0 && type2 === 1) return 'STAIRSUP';
  if (type1 === 0 && type2 === 2) return 'STAIRSDOWN';
  if (type1 === 2 && type2 === 0) return 'TRACK';
  if (type1 === 3 && type2 === 0) return 'OUTSIDEROOM';
  if (type1 === 3 && type2 === 1) return 'SPECIFICROOM';
  return 'RECTANGLE';
}

import { ACTOR_DEFS } from './Toolbar.js';

function getActorDef(id) {
  return ACTOR_DEFS.find(a => a.id === id) ?? { icon: '??', color: '#6b7280', label: 'Unknown' };
}

export default function MapCanvas({
  map, tileImages, activeTool, activeLayer, selectedTileId,
  zoom, pan, onZoomChange, onPanChange,
  onTilePaint, onPlatformDraw, onPlatformRemove, onActorPlace, onActorRemove,
  onBeginPaint, onCommitPaint,
  selectedActorId, dragPlatform, onDragPlatformChange,
  onCursorChange,
}) {
  const canvasRef = useRef(null);
  const isPainting = useRef(false);
  const isSpacePanning = useRef(false);
  const isCtrlPanning = useRef(false);
  const isPanning = useRef(false);
  const lastPan = useRef({ x: 0, y: 0 });

  // World → canvas coords
  const worldToCanvas = useCallback((wx, wy) => ({
    cx: wx * zoom + pan.x,
    cy: wy * zoom + pan.y,
  }), [zoom, pan]);

  // Canvas → world coords (in tiles)
  const canvasToTile = useCallback((cx, cy) => ({
    tx: Math.floor((cx - pan.x) / (64 * zoom)),
    ty: Math.floor((cy - pan.y) / (64 * zoom)),
  }), [zoom, pan]);

  // Canvas → world pixel coords
  const canvasToWorld = useCallback((cx, cy) => ({
    wx: (cx - pan.x) / zoom,
    wy: (cy - pan.y) / zoom,
  }), [zoom, pan]);

  // Draw the map
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const W = canvas.width;
    const H = canvas.height;
    ctx.clearRect(0, 0, W, H);

    if (!map) {
      ctx.fillStyle = '#0a0a0f';
      ctx.fillRect(0, 0, W, H);
      ctx.fillStyle = '#1a2e1a';
      ctx.font = '14px monospace';
      ctx.textAlign = 'center';
      ctx.fillText('Open a .SIL file to begin', W / 2, H / 2);
      return;
    }

    const tileSize = 64 * zoom;
    const { width, height, layers, actors, platforms } = map;

    // Draw checkerboard background
    ctx.fillStyle = '#0a0a0f';
    ctx.fillRect(0, 0, W, H);
    const checkSize = Math.max(8, tileSize / 2);
    ctx.fillStyle = '#0f0f18';
    for (let cy = 0; cy < H; cy += checkSize * 2) {
      for (let cx = 0; cx < W; cx += checkSize * 2) {
        ctx.fillRect(cx, cy, checkSize, checkSize);
        ctx.fillRect(cx + checkSize, cy + checkSize, checkSize, checkSize);
      }
    }

    // Ambient darkness from map header: ambiencelevel = 128 + ambience*4.5
    const ambience = map.header?.ambience ?? 0;
    const ambiencelevel = Math.max(0, Math.min(255, 128 + ambience * 4.5));
    const darkAlpha = Math.max(0, 1 - ambiencelevel / 128); // e.g. ~0.42-0.55

    // Draw a single tile (no filter — fast)
    function blitTile(tile_id, flip, col, row) {
      if (!tile_id) return;
      const bank = (tile_id >> 8) & 0xFF;
      const idx  = tile_id & 0xFF;
      const bitmaps = tileImages?.get(bank);
      if (!bitmaps || !bitmaps[idx]) return;
      const bmp = bitmaps[idx];
      const dx = col * tileSize + pan.x;
      const dy = row * tileSize + pan.y;
      if (dx + tileSize < 0 || dx > W || dy + tileSize < 0 || dy > H) return;
      if (flip) {
        ctx.save();
        ctx.translate(dx + tileSize, dy);
        ctx.scale(-1, 1);
        ctx.drawImage(bmp, 0, 0, tileSize, tileSize);
        ctx.restore();
      } else {
        ctx.drawImage(bmp, dx, dy, tileSize, tileSize);
      }
    }

    // Pass 1: draw ALL tiles at full brightness (no filter per tile)
    for (let l = 0; l < 4; l++) {
      for (let row = 0; row < height; row++) {
        for (let col = 0; col < width; col++) {
          const cell = layers.bg[l][row * width + col];
          if (cell) blitTile(cell.tile_id, cell.flip, col, row);
        }
      }
    }
    for (let l = 0; l < 4; l++) {
      for (let row = 0; row < height; row++) {
        for (let col = 0; col < width; col++) {
          const cell = layers.fg[l][row * width + col];
          if (cell) blitTile(cell.tile_id, cell.flip, col, row);
        }
      }
    }

    // Pass 2: one dark overlay over the entire map area (darkens non-LUM tiles)
    if (darkAlpha > 0) {
      ctx.fillStyle = `rgba(0,0,0,${darkAlpha.toFixed(3)})`;
      ctx.fillRect(pan.x, pan.y, width * tileSize, height * tileSize);
    }

    // Pass 3: redraw LUM tiles on top of the overlay (they appear bright)
    for (let l = 0; l < 4; l++) {
      for (let row = 0; row < height; row++) {
        for (let col = 0; col < width; col++) {
          const cell = layers.bg[l][row * width + col];
          if (cell?.lum) blitTile(cell.tile_id, cell.flip, col, row);
        }
      }
    }
    for (let l = 0; l < 4; l++) {
      for (let row = 0; row < height; row++) {
        for (let col = 0; col < width; col++) {
          const cell = layers.fg[l][row * width + col];
          if (cell?.lum) blitTile(cell.tile_id, cell.flip, col, row);
        }
      }
    }

    // Grid overlay when zoomed in enough
    if (zoom >= 0.25) {
      ctx.strokeStyle = 'rgba(26,46,26,0.4)';
      ctx.lineWidth = 0.5;
      const startCol = Math.max(0, Math.floor(-pan.x / tileSize));
      const endCol   = Math.min(width,  Math.ceil((W - pan.x) / tileSize));
      const startRow = Math.max(0, Math.floor(-pan.y / tileSize));
      const endRow   = Math.min(height, Math.ceil((H - pan.y) / tileSize));
      for (let col = startCol; col <= endCol; col++) {
        ctx.beginPath();
        ctx.moveTo(col * tileSize + pan.x, pan.y);
        ctx.lineTo(col * tileSize + pan.x, height * tileSize + pan.y);
        ctx.stroke();
      }
      for (let row = startRow; row <= endRow; row++) {
        ctx.beginPath();
        ctx.moveTo(pan.x, row * tileSize + pan.y);
        ctx.lineTo(width * tileSize + pan.x, row * tileSize + pan.y);
        ctx.stroke();
      }
    }

    // Map border
    ctx.strokeStyle = '#1a2e1a';
    ctx.lineWidth = 2;
    ctx.strokeRect(pan.x, pan.y, width * tileSize, height * tileSize);

    // Platform overlays
    for (const p of platforms) {
      const typeName = platformTypeName(p.type1, p.type2);
      const color = PLATFORM_COLORS[typeName] ?? 'rgba(80,130,255,0.3)';
      ctx.fillStyle = color;
      ctx.strokeStyle = color.replace(/[\d.]+\)$/, '0.9)');
      ctx.lineWidth = 1;
      const cx1 = p.x1 * zoom + pan.x;
      const cy1 = p.y1 * zoom + pan.y;
      const cx2 = p.x2 * zoom + pan.x;
      const cy2 = p.y2 * zoom + pan.y;
      ctx.fillRect(cx1, cy1, cx2 - cx1, cy2 - cy1);
      ctx.strokeRect(cx1, cy1, cx2 - cx1, cy2 - cy1);
    }

    // Drag platform preview
    if (dragPlatform) {
      const { wx1, wy1, wx2, wy2, typeName } = dragPlatform;
      const color = PLATFORM_COLORS[typeName] ?? 'rgba(80,130,255,0.3)';
      ctx.fillStyle = color;
      ctx.strokeStyle = color.replace(/[\d.]+\)$/, '0.9)');
      ctx.setLineDash([4, 4]);
      ctx.lineWidth = 1;
      const cx1 = wx1 * zoom + pan.x;
      const cy1 = wy1 * zoom + pan.y;
      const cx2 = wx2 * zoom + pan.x;
      const cy2 = wy2 * zoom + pan.y;
      ctx.fillRect(cx1, cy1, cx2 - cx1, cy2 - cy1);
      ctx.strokeRect(cx1, cy1, cx2 - cx1, cy2 - cy1);
      ctx.setLineDash([]);
    }

    // Actor icons
    ctx.font = 'bold 9px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    for (const a of actors) {
      const def = getActorDef(a.id);
      const cx = a.x * zoom + pan.x;
      const cy = a.y * zoom + pan.y;
      const r = Math.max(6, 8 * zoom);
      ctx.fillStyle = def.color + 'cc';
      ctx.beginPath();
      ctx.arc(cx, cy, r, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = def.color;
      ctx.lineWidth = 1;
      ctx.stroke();
      if (zoom > 0.15) {
        ctx.fillStyle = '#fff';
        ctx.fillText(def.icon, cx, cy);
      }
    }
  }, [map, tileImages, zoom, pan, dragPlatform]);

  // Resize canvas to fill container
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const resizer = new ResizeObserver(() => {
      const rect = canvas.parentElement.getBoundingClientRect();
      canvas.width  = rect.width;
      canvas.height = rect.height;
    });
    resizer.observe(canvas.parentElement);
    return () => resizer.disconnect();
  }, []);

  // Mouse event handlers
  const getCanvasPos = (e) => {
    const rect = canvasRef.current.getBoundingClientRect();
    return { cx: e.clientX - rect.left, cy: e.clientY - rect.top };
  };

  const handleWheel = useCallback((e) => {
    e.preventDefault();
    const { cx, cy } = getCanvasPos(e);
    const factor = e.deltaY < 0 ? 1.15 : 1 / 1.15;
    const newZoom = Math.min(4, Math.max(0.05, zoom * factor));
    // Zoom centered on cursor
    const newPanX = cx - (cx - pan.x) * (newZoom / zoom);
    const newPanY = cy - (cy - pan.y) * (newZoom / zoom);
    onZoomChange(newZoom);
    onPanChange({ x: newPanX, y: newPanY });
  }, [zoom, pan, onZoomChange, onPanChange]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    canvas.addEventListener('wheel', handleWheel, { passive: false });
    return () => canvas.removeEventListener('wheel', handleWheel);
  }, [handleWheel]);

  const handleMouseDown = useCallback((e) => {
    const { cx, cy } = getCanvasPos(e);

    // Middle mouse or Space+left or Ctrl+left = pan
    if (e.button === 1 || (e.button === 0 && isSpacePanning.current) || (e.button === 0 && isCtrlPanning.current)) {
      isPanning.current = true;
      lastPan.current = { x: cx, y: cy };
      return;
    }

    if (!map || e.button !== 0) return;

    const { tx, ty } = canvasToTile(cx, cy);
    const { wx, wy } = canvasToWorld(cx, cy);

    if (activeTool === 'TILE_BG' || activeTool === 'TILE_FG') {
      isPainting.current = true;
      onBeginPaint?.();
      if (selectedTileId && tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
        onTilePaint(activeTool === 'TILE_FG' ? 'fg' : 'bg', activeLayer, tx, ty, selectedTileId);
      }
    } else if (['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK'].includes(activeTool)) {
      isPainting.current = true;
      onDragPlatformChange({ wx1: wx, wy1: wy, wx2: wx, wy2: wy, tool: activeTool });
    } else if (activeTool === 'ERASE_PLATFORM') {
      // Find platform under cursor and remove
      for (let i = map.platforms.length - 1; i >= 0; i--) {
        const p = map.platforms[i];
        if (wx >= p.x1 && wx <= p.x2 && wy >= p.y1 && wy <= p.y2) {
          onPlatformRemove(i);
          break;
        }
      }
    } else if (activeTool === 'ACTOR') {
      onActorPlace({ wx, wy });
    } else if (activeTool === 'SELECT') {
      // Remove actor on click
      for (let i = map.actors.length - 1; i >= 0; i--) {
        const a = map.actors[i];
        const dist = Math.hypot(a.x - wx, a.y - wy);
        if (dist < 32 / zoom) {
          onActorRemove(i);
          return;
        }
      }
      // Remove platform on click
      for (let i = map.platforms.length - 1; i >= 0; i--) {
        const p = map.platforms[i];
        if (wx >= p.x1 && wx <= p.x2 && wy >= p.y1 && wy <= p.y2) {
          onPlatformRemove(i);
          return;
        }
      }
    }
  }, [map, activeTool, activeLayer, selectedTileId, canvasToTile, canvasToWorld, zoom,
      onTilePaint, onPlatformRemove, onActorPlace, onActorRemove, onDragPlatformChange, onBeginPaint]);

  const handleMouseMove = useCallback((e) => {
    const { cx, cy } = getCanvasPos(e);

    if (isPanning.current) {
      const dx = cx - lastPan.current.x;
      const dy = cy - lastPan.current.y;
      lastPan.current = { x: cx, y: cy };
      onPanChange(prev => ({ x: prev.x + dx, y: prev.y + dy }));
      return;
    }

    if (!map) return;
    const { tx, ty } = canvasToTile(cx, cy);
    const { wx, wy } = canvasToWorld(cx, cy);
    onCursorChange({ tx, ty, wx, wy });

    if (isPainting.current) {
      if (activeTool === 'TILE_BG' || activeTool === 'TILE_FG') {
        if (selectedTileId && tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
          onTilePaint(activeTool === 'TILE_FG' ? 'fg' : 'bg', activeLayer, tx, ty, selectedTileId);
        }
      } else if (['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK'].includes(activeTool)) {
        onDragPlatformChange(prev => prev ? { ...prev, wx2: wx, wy2: wy } : null);
      }
    }
  }, [map, isPanning, activeTool, activeLayer, selectedTileId, canvasToTile, canvasToWorld,
      onTilePaint, onPanChange, onCursorChange, onDragPlatformChange]);

  const handleMouseUp = useCallback((e) => {
    if (isPanning.current) {
      isPanning.current = false;
      return;
    }

    if (!map) { isPainting.current = false; return; }
    const { cx, cy } = getCanvasPos(e);
    const { wx, wy } = canvasToWorld(cx, cy);

    if (isPainting.current) {
      if (['TILE_BG', 'TILE_FG'].includes(activeTool)) {
        onCommitPaint?.();
      } else if (['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK'].includes(activeTool) && dragPlatform) {
        const { wx1, wy1 } = dragPlatform;
        const TOOL_TYPE_MAP = {
          RECT:       { type1: 0, type2: 0, typeName: 'RECTANGLE' },
          STAIRSUP:   { type1: 0, type2: 1, typeName: 'STAIRSUP' },
          STAIRSDOWN: { type1: 0, type2: 2, typeName: 'STAIRSDOWN' },
          LADDER:     { type1: 1, type2: 0, typeName: 'LADDER' },
          TRACK:      { type1: 2, type2: 0, typeName: 'TRACK' },
        };
        const t = TOOL_TYPE_MAP[activeTool];
        const x1 = Math.min(wx1, wx), y1 = Math.min(wy1, wy);
        const x2 = Math.max(wx1, wx), y2 = Math.max(wy1, wy);
        if (x2 - x1 > 2 && y2 - y1 > 2) onPlatformDraw({ x1, y1, x2, y2, ...t });
        onDragPlatformChange(null);
      }
    }
    isPainting.current = false;
  }, [map, activeTool, dragPlatform, canvasToWorld, onPlatformDraw, onDragPlatformChange, onCommitPaint]);

  const handleKeyDown = useCallback((e) => {
    if (e.code === 'Space') { isSpacePanning.current = true; e.preventDefault(); }
    if (e.code === 'ControlLeft' || e.code === 'ControlRight') isCtrlPanning.current = true;
  }, []);
  const handleKeyUp = useCallback((e) => {
    if (e.code === 'Space') isSpacePanning.current = false;
    if (e.code === 'ControlLeft' || e.code === 'ControlRight') isCtrlPanning.current = false;
  }, []);

  useEffect(() => {
    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
    };
  }, [handleKeyDown, handleKeyUp]);

  const cursorStyle = (isPanning.current || isSpacePanning.current || isCtrlPanning.current) ? 'grab' : (isPainting.current ? 'crosshair' : 'default');

  return (
    <div className="relative w-full h-full overflow-hidden bg-[#050a05]">
      <canvas
        ref={canvasRef}
        style={{ cursor: cursorStyle, display: 'block', width: '100%', height: '100%' }}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
        onContextMenu={e => e.preventDefault()}
      />
    </div>
  );
}
