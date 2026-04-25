'use client';
import { useRef, useEffect, useCallback, useState } from 'react';

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
  map, tileImages, spriteImages, vis, activeTool, activeLayer, selectedTileId,
  zoom, pan, onZoomChange, onPanChange,
  onTilePaint, onPlatformDraw, onPlatformRemove, onActorPlace, onActorRemove, onActorRightClick,
  onTileRightClick,
  onBeginPaint, onCommitPaint,
  selectedActorId, dragPlatform, onDragPlatformChange,
  onCursorChange,
  onActorMove,
  eraseLayerType,
  highlightActorIdx,
  selectedPlatformIdx, onPlatformSelect, onPlatformUpdate,
})  {
  const canvasRef = useRef(null);
  const overlayCanvasRef = useRef(null);
  const rafRef = useRef(null);
  const isPainting = useRef(false);
  const isSpacePanning = useRef(false);
  const isCtrlPanning = useRef(false);
  const isPanning = useRef(false);
  const lastPan = useRef({ x: 0, y: 0 });
  const draggingActorRef = useRef(null); // { idx, startWx, startWy, origX, origY, moved }
  const [dragActorPreview, setDragActorPreview] = useState(null); // { idx, wx, wy } | null
  // Platform drag ref: { mode, handle, idx, origPlatform, startWx, startWy }
  const platformDragRef = useRef(null);
  // Current preview bounds during platform drag { wx1, wy1, wx2, wy2 }
  const platformPreviewRef = useRef(null);

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
    const darkAlpha = vis?.lighting !== false ? Math.max(0, 1 - ambiencelevel / 128) : 0;

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
      if (vis?.bg?.[l] === false) continue;
      for (let row = 0; row < height; row++) {
        for (let col = 0; col < width; col++) {
          const cell = layers.bg[l][row * width + col];
          if (cell) blitTile(cell.tile_id, cell.flip, col, row);
        }
      }
    }
    for (let l = 0; l < 4; l++) {
      if (vis?.fg?.[l] === false) continue;
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
      if (vis?.bg?.[l] === false) continue;
      for (let row = 0; row < height; row++) {
        for (let col = 0; col < width; col++) {
          const cell = layers.bg[l][row * width + col];
          if (cell?.lum) blitTile(cell.tile_id, cell.flip, col, row);
        }
      }
    }
    for (let l = 0; l < 4; l++) {
      if (vis?.fg?.[l] === false) continue;
      for (let row = 0; row < height; row++) {
        for (let col = 0; col < width; col++) {
          const cell = layers.fg[l][row * width + col];
          if (cell?.lum) blitTile(cell.tile_id, cell.flip, col, row);
        }
      }
    }

    // Grid overlay when zoomed in enough
    if (vis?.grid !== false && zoom >= 0.25) {
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
    function buildPlatformPath(cx1, cy1, cx2, cy2, typeName) {
      ctx.beginPath();
      if (typeName === 'STAIRSUP') {
        ctx.moveTo(cx1, cy2);
        ctx.lineTo(cx2, cy1);
        ctx.lineTo(cx2, cy2);
      } else if (typeName === 'STAIRSDOWN') {
        ctx.moveTo(cx1, cy1);
        ctx.lineTo(cx2, cy2);
        ctx.lineTo(cx1, cy2);
      } else {
        ctx.rect(cx1, cy1, cx2 - cx1, cy2 - cy1);
      }
      ctx.closePath();
    }

    function drawPlatform(cx1, cy1, cx2, cy2, typeName, dashed) {
      const color = PLATFORM_COLORS[typeName] ?? 'rgba(80,130,255,0.3)';
      const strokeColor = color.replace(/[\d.]+\)$/, '0.9)');

      // Base fill
      ctx.lineWidth = 1;
      buildPlatformPath(cx1, cy1, cx2, cy2, typeName);
      ctx.fillStyle = color;
      ctx.fill();

      // Cross-grid hatch clipped to the platform shape
      ctx.save();
      buildPlatformPath(cx1, cy1, cx2, cy2, typeName);
      ctx.clip();
      ctx.strokeStyle = strokeColor;
      ctx.lineWidth = 0.5;
      ctx.setLineDash([]);
      const step = 8;
      const xMin = Math.floor(Math.min(cx1, cx2) / step) * step;
      const xMax = Math.ceil(Math.max(cx1, cx2) / step) * step;
      const yMin = Math.floor(Math.min(cy1, cy2) / step) * step;
      const yMax = Math.ceil(Math.max(cy1, cy2) / step) * step;
      ctx.beginPath();
      for (let y = yMin; y <= yMax; y += step) {
        ctx.moveTo(xMin, y); ctx.lineTo(xMax, y);
      }
      for (let x = xMin; x <= xMax; x += step) {
        ctx.moveTo(x, yMin); ctx.lineTo(x, yMax);
      }
      ctx.stroke();
      ctx.restore();

      // Outline
      ctx.lineWidth = 1;
      if (dashed) ctx.setLineDash([4, 4]);
      buildPlatformPath(cx1, cy1, cx2, cy2, typeName);
      ctx.strokeStyle = strokeColor;
      ctx.stroke();

      if (dashed) ctx.setLineDash([]);
    }

    if (vis?.platforms !== false) {
      for (const p of platforms) {
        const typeName = platformTypeName(p.type1, p.type2);
        const cx1 = p.x1 * zoom + pan.x;
        const cy1 = p.y1 * zoom + pan.y;
        const cx2 = p.x2 * zoom + pan.x;
        const cy2 = p.y2 * zoom + pan.y;
        drawPlatform(cx1, cy1, cx2, cy2, typeName, false);
      }
    }

    // Drag platform preview (always show)
    if (dragPlatform) {
      const { wx1, wy1, wx2, wy2, typeName } = dragPlatform;
      const cx1 = wx1 * zoom + pan.x;
      const cy1 = wy1 * zoom + pan.y;
      const cx2 = wx2 * zoom + pan.x;
      const cy2 = wy2 * zoom + pan.y;
      drawPlatform(cx1, cy1, cx2, cy2, typeName, true);
    }

    // Actor icons
    if (vis?.actors !== false) {
      ctx.font = 'bold 9px monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      for (const a of actors) {
        const def = getActorDef(a.id);
        const cx = a.x * zoom + pan.x;
        const cy = a.y * zoom + pan.y;

        // Dynamic bank for actors where it depends on actor.type
        let bankNum = def.bank;
        if (a.id === 54) bankNum = a.type === 0 ? 183 : 184;
        if (a.id === 47) bankNum = 49 + Math.min(a.type ?? 0, 9); // doodad type 0-9 → banks 49-58
        if (a.id === 63) {
          // Powerup: type 0=SuperShield→200, type 2=JetPack→201, rest→205
          if (a.type === 0) bankNum = 200;
          else if (a.type === 2) bankNum = 201;
          else bankNum = 205;
        }

        // Try to draw actual sprite
        const sprBank = bankNum != null ? spriteImages?.get(bankNum) : null;
        const spr = sprBank?.[def.frame ?? 0];
        if (spr) {
          const sx = cx - spr.offsetX * zoom;
          const sy = cy - spr.offsetY * zoom;
          const sw = spr.width * zoom;
          const sh = spr.height * zoom;
          ctx.drawImage(spr.bitmap, sx, sy, sw, sh);
          if (zoom > 0.3) {
            ctx.fillStyle = def.color + 'cc';
            ctx.fillRect(cx - 12, cy + 2, 24, 11);
            ctx.fillStyle = '#fff';
            ctx.fillText(def.icon, cx, cy + 7);
          }
        } else {
          // Fallback: colored circle with icon
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
      }
    }

    // Ghost actor while dragging (SELECT tool)
    if (dragActorPreview && actors[dragActorPreview.idx]) {
      const a = actors[dragActorPreview.idx];
      const def = getActorDef(a.id);
      const cx = dragActorPreview.wx * zoom + pan.x;
      const cy = dragActorPreview.wy * zoom + pan.y;
      const r = Math.max(6, 10 * zoom);
      ctx.save();
      ctx.globalAlpha = 0.55;
      ctx.fillStyle = def.color + '88';
      ctx.beginPath();
      ctx.arc(cx, cy, r, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = def.color;
      ctx.lineWidth = 2;
      ctx.setLineDash([4, 4]);
      ctx.stroke();
      ctx.setLineDash([]);
      ctx.restore();
    }
  }, [map, tileImages, spriteImages, vis, zoom, pan, dragPlatform, dragActorPreview]);

  // Resize canvas to fill container
  useEffect(() => {
    const canvas = canvasRef.current;
    const overlay = overlayCanvasRef.current;
    if (!canvas) return;
    const resizer = new ResizeObserver(() => {
      const rect = canvas.parentElement.getBoundingClientRect();
      canvas.width  = rect.width;
      canvas.height = rect.height;
      if (overlay) { overlay.width = rect.width; overlay.height = rect.height; }
    });
    resizer.observe(canvas.parentElement);
    return () => resizer.disconnect();
  }, []);

  // Animated marching-ants selection highlight on overlay canvas
  useEffect(() => {
    const overlay = overlayCanvasRef.current;
    if (!overlay) return;
    const ctx = overlay.getContext('2d');

    const hasActorHighlight = highlightActorIdx != null && map?.actors[highlightActorIdx];
    const hasPlatformHighlight = selectedPlatformIdx != null && map?.platforms[selectedPlatformIdx];

    if (!hasActorHighlight && !hasPlatformHighlight) {
      ctx.clearRect(0, 0, overlay.width, overlay.height);
      if (rafRef.current) { cancelAnimationFrame(rafRef.current); rafRef.current = null; }
      return;
    }

    const actor = hasActorHighlight ? map.actors[highlightActorIdx] : null;
    const def = actor ? getActorDef(actor.id) : null;

    function getSpriteRect() {
      if (!actor || !def) return null;
      let bankNum = def.bank;
      if (actor.id === 54) bankNum = actor.type === 0 ? 183 : 184;
      if (actor.id === 47) bankNum = 49 + Math.min(actor.type ?? 0, 9);
      if (actor.id === 63) bankNum = actor.type === 0 ? 200 : actor.type === 2 ? 201 : 205;
      const sprBank = bankNum != null ? spriteImages?.get(bankNum) : null;
      const spr = sprBank?.[def.frame ?? 0];
      const cx = actor.x * zoom + pan.x;
      const cy = actor.y * zoom + pan.y;
      if (spr) {
        const pad = 3;
        return { x: cx - spr.offsetX * zoom - pad, y: cy - spr.offsetY * zoom - pad,
                 w: spr.width * zoom + pad * 2, h: spr.height * zoom + pad * 2 };
      }
      const r = Math.max(8, 10 * zoom);
      return { circle: true, cx, cy, r: r + 3 };
    }

    let dashOffset = 0;
    function draw() {
      ctx.clearRect(0, 0, overlay.width, overlay.height);

      // Actor marching-ants highlight
      if (hasActorHighlight) {
        const rect = getSpriteRect();
        if (rect) {
          ctx.save();
          ctx.lineWidth = 2;
          ctx.setLineDash([6, 4]);
          ctx.strokeStyle = 'rgba(255,255,255,0.9)';
          ctx.lineDashOffset = -dashOffset;
          if (rect.circle) {
            ctx.beginPath(); ctx.arc(rect.cx, rect.cy, rect.r, 0, Math.PI * 2); ctx.stroke();
          } else {
            ctx.strokeRect(rect.x, rect.y, rect.w, rect.h);
          }
          ctx.strokeStyle = 'rgba(74,200,74,0.7)';
          ctx.lineDashOffset = -dashOffset + 5;
          if (rect.circle) {
            ctx.beginPath(); ctx.arc(rect.cx, rect.cy, rect.r, 0, Math.PI * 2); ctx.stroke();
          } else {
            ctx.strokeRect(rect.x, rect.y, rect.w, rect.h);
          }
          ctx.restore();
        }
      }

      // Platform marching-ants highlight + resize handles
      if (hasPlatformHighlight) {
        const p = map.platforms[selectedPlatformIdx];
        const preview = platformDragRef.current ? platformPreviewRef.current : null;
        const { x1, y1, x2, y2 } = preview
          ? { x1: preview.wx1, y1: preview.wy1, x2: preview.wx2, y2: preview.wy2 }
          : { x1: p.x1, y1: p.y1, x2: p.x2, y2: p.y2 };

        const cx1 = x1 * zoom + pan.x;
        const cy1 = y1 * zoom + pan.y;
        const cx2 = x2 * zoom + pan.x;
        const cy2 = y2 * zoom + pan.y;

        ctx.save();
        ctx.lineWidth = 2;
        ctx.setLineDash([6, 4]);
        ctx.strokeStyle = 'rgba(255,255,255,0.9)';
        ctx.lineDashOffset = -dashOffset;
        ctx.strokeRect(cx1, cy1, cx2 - cx1, cy2 - cy1);
        ctx.strokeStyle = 'rgba(74,200,74,0.7)';
        ctx.lineDashOffset = -dashOffset + 5;
        ctx.strokeRect(cx1, cy1, cx2 - cx1, cy2 - cy1);
        ctx.setLineDash([]);
        ctx.restore();

        // 8 resize handles (8×8 px, white fill, dark stroke)
        const HS = 8;
        const hs = HS / 2;
        const hmx = (cx1 + cx2) / 2;
        const hmy = (cy1 + cy2) / 2;
        const handlePoints = [
          { hx: cx1, hy: cy1 }, { hx: hmx, hy: cy1 }, { hx: cx2, hy: cy1 },
          { hx: cx1, hy: hmy },                         { hx: cx2, hy: hmy },
          { hx: cx1, hy: cy2 }, { hx: hmx, hy: cy2 }, { hx: cx2, hy: cy2 },
        ];
        ctx.fillStyle = 'rgba(255,255,255,0.95)';
        ctx.strokeStyle = 'rgba(30,30,30,0.9)';
        ctx.lineWidth = 1;
        for (const { hx, hy } of handlePoints) {
          ctx.fillRect(hx - hs, hy - hs, HS, HS);
          ctx.strokeRect(hx - hs, hy - hs, HS, HS);
        }
      }

      dashOffset = (dashOffset + 0.5) % 10;
      rafRef.current = requestAnimationFrame(draw);
    }
    rafRef.current = requestAnimationFrame(draw);
    return () => { if (rafRef.current) { cancelAnimationFrame(rafRef.current); rafRef.current = null; } };
  }, [highlightActorIdx, selectedPlatformIdx, map, spriteImages, zoom, pan]);

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
    } else if (activeTool === 'ERASE_TILE') {
      isPainting.current = true;
      onBeginPaint?.();
      if (tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
        onTilePaint(eraseLayerType ?? 'bg', activeLayer, tx, ty, 0);
      }
    } else if (['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK','OUTSIDEROOM','SPECIFICROOM'].includes(activeTool)) {
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
      const handleSize = 8 / zoom;
      const hs = handleSize / 2;

      // 1. If a platform is already selected, check its handles then body first
      if (selectedPlatformIdx != null && map.platforms[selectedPlatformIdx]) {
        const p = map.platforms[selectedPlatformIdx];
        const { x1, y1, x2, y2 } = p;
        const mx = (x1 + x2) / 2;
        const my = (y1 + y2) / 2;
        const handles = [
          { name: 'TL', hx: x1, hy: y1 }, { name: 'T',  hx: mx, hy: y1 }, { name: 'TR', hx: x2, hy: y1 },
          { name: 'L',  hx: x1, hy: my },                                   { name: 'R',  hx: x2, hy: my },
          { name: 'BL', hx: x1, hy: y2 }, { name: 'B',  hx: mx, hy: y2 }, { name: 'BR', hx: x2, hy: y2 },
        ];
        for (const { name, hx, hy } of handles) {
          if (Math.abs(wx - hx) <= hs && Math.abs(wy - hy) <= hs) {
            platformDragRef.current = { mode: 'handle', handle: name, idx: selectedPlatformIdx, origPlatform: { ...p }, startWx: wx, startWy: wy };
            return;
          }
        }
        if (wx >= x1 && wx <= x2 && wy >= y1 && wy <= y2) {
          platformDragRef.current = { mode: 'body', handle: null, idx: selectedPlatformIdx, origPlatform: { ...p }, startWx: wx, startWy: wy };
          return;
        }
      }

      // 2. Hit-test all platforms for selection
      for (let i = map.platforms.length - 1; i >= 0; i--) {
        const p = map.platforms[i];
        if (wx >= p.x1 && wx <= p.x2 && wy >= p.y1 && wy <= p.y2) {
          onPlatformSelect(i);
          return;
        }
      }

      // 3. Hit-test actors (existing logic)
      const HIT = 48 / zoom;
      for (let i = map.actors.length - 1; i >= 0; i--) {
        const a = map.actors[i];
        const dist = Math.hypot(a.x - wx, a.y - wy);
        if (dist < HIT) {
          draggingActorRef.current = { idx: i, startWx: wx, startWy: wy, origX: a.x, origY: a.y, moved: false };
          setDragActorPreview({ idx: i, wx: a.x, wy: a.y });
          return;
        }
      }

      // 4. Click on empty → deselect
      onPlatformSelect(null);
    }
  }, [map, activeTool, activeLayer, selectedTileId, canvasToTile, canvasToWorld, zoom, eraseLayerType,
      onTilePaint, onPlatformRemove, onActorPlace, onDragPlatformChange, onBeginPaint,
      selectedPlatformIdx, onPlatformSelect]);

  // Right-click: actors take priority, fall through to tile property editor
  const handleContextMenu = useCallback((e) => {
    e.preventDefault();
    if (!map) return;
    const { cx, cy } = getCanvasPos(e);
    const { wx, wy } = canvasToWorld(cx, cy);

    // Check actors first
    const HIT_RADIUS = 96 / zoom;
    let best = null, bestDist = HIT_RADIUS;
    for (let i = map.actors.length - 1; i >= 0; i--) {
      const a = map.actors[i];
      const dist = Math.hypot(a.x - wx, a.y - wy);
      if (dist < bestDist) { bestDist = dist; best = i; }
    }
    if (best !== null && onActorRightClick) {
      onActorRightClick(best, e.clientX, e.clientY);
      return;
    }

    // Fall through to tile
    if (onTileRightClick) {
      const { tx, ty } = canvasToTile(cx, cy);
      if (tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
        const layerType = activeTool === 'TILE_FG' ? 'fg' : 'bg';
        const layerIdx = activeLayer;
        const layerArr = map.layers[layerType][layerIdx];
        const cell = layerArr[ty * map.width + tx] ?? null;
        onTileRightClick({ tx, ty, layerType, layerIdx, cell, x: e.clientX, y: e.clientY });
      }
    }
  }, [map, canvasToWorld, canvasToTile, zoom, activeTool, activeLayer, onActorRightClick, onTileRightClick]);

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

    // Platform drag (SELECT tool — handle or body move)
    if (platformDragRef.current) {
      const { mode, handle, origPlatform, startWx, startWy } = platformDragRef.current;
      const dx = wx - startWx;
      const dy = wy - startWy;
      const MIN_SIZE = 16;
      let { x1, y1, x2, y2 } = origPlatform;

      if (mode === 'body') {
        x1 = origPlatform.x1 + dx;
        y1 = origPlatform.y1 + dy;
        x2 = origPlatform.x2 + dx;
        y2 = origPlatform.y2 + dy;
      } else {
        switch (handle) {
          case 'TL': x1 = origPlatform.x1 + dx; y1 = origPlatform.y1 + dy; break;
          case 'TR': x2 = origPlatform.x2 + dx; y1 = origPlatform.y1 + dy; break;
          case 'BL': x1 = origPlatform.x1 + dx; y2 = origPlatform.y2 + dy; break;
          case 'BR': x2 = origPlatform.x2 + dx; y2 = origPlatform.y2 + dy; break;
          case 'T':  y1 = origPlatform.y1 + dy; break;
          case 'B':  y2 = origPlatform.y2 + dy; break;
          case 'L':  x1 = origPlatform.x1 + dx; break;
          case 'R':  x2 = origPlatform.x2 + dx; break;
        }
        if (x2 - x1 < MIN_SIZE) {
          if (handle === 'TL' || handle === 'BL' || handle === 'L') x1 = x2 - MIN_SIZE;
          else x2 = x1 + MIN_SIZE;
        }
        if (y2 - y1 < MIN_SIZE) {
          if (handle === 'TL' || handle === 'TR' || handle === 'T') y1 = y2 - MIN_SIZE;
          else y2 = y1 + MIN_SIZE;
        }
      }

      platformPreviewRef.current = { wx1: x1, wy1: y1, wx2: x2, wy2: y2 };
      onDragPlatformChange({ wx1: x1, wy1: y1, wx2: x2, wy2: y2, typeName: origPlatform.typeName });
      return;
    }

    // Actor drag (SELECT tool)
    if (draggingActorRef.current) {
      const { startWx, startWy, origX, origY } = draggingActorRef.current;
      const newWx = origX + (wx - startWx);
      const newWy = origY + (wy - startWy);
      draggingActorRef.current.moved = Math.hypot(wx - startWx, wy - startWy) > 4;
      setDragActorPreview({ idx: draggingActorRef.current.idx, wx: newWx, wy: newWy });
      return;
    }

    if (isPainting.current) {
      if (activeTool === 'TILE_BG' || activeTool === 'TILE_FG') {
        if (selectedTileId && tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
          onTilePaint(activeTool === 'TILE_FG' ? 'fg' : 'bg', activeLayer, tx, ty, selectedTileId);
        }
      } else if (activeTool === 'ERASE_TILE') {
        if (tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
          onTilePaint(eraseLayerType ?? 'bg', activeLayer, tx, ty, 0);
        }
      } else if (['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK','OUTSIDEROOM','SPECIFICROOM'].includes(activeTool)) {
        onDragPlatformChange(prev => prev ? { ...prev, wx2: wx, wy2: wy } : null);
      }
    }
  }, [map, activeTool, activeLayer, selectedTileId, canvasToTile, canvasToWorld, eraseLayerType,
      onTilePaint, onPanChange, onCursorChange, onDragPlatformChange]);

  const handleMouseUp = useCallback((e) => {
    if (isPanning.current) {
      isPanning.current = false;
      return;
    }

    // Finish actor drag
    if (draggingActorRef.current) {
      const { idx, moved } = draggingActorRef.current;
      if (moved && dragActorPreview) {
        onActorMove?.(idx, Math.round(dragActorPreview.wx), Math.round(dragActorPreview.wy));
      }
      draggingActorRef.current = null;
      setDragActorPreview(null);
      return;
    }

    // Finish platform drag (SELECT tool)
    if (platformDragRef.current) {
      const { idx, origPlatform } = platformDragRef.current;
      const preview = platformPreviewRef.current;
      if (preview) {
        const { wx1, wy1, wx2, wy2 } = preview;
        if (wx1 !== origPlatform.x1 || wy1 !== origPlatform.y1 || wx2 !== origPlatform.x2 || wy2 !== origPlatform.y2) {
          onPlatformUpdate(idx, Math.round(wx1), Math.round(wy1), Math.round(wx2), Math.round(wy2));
        }
      }
      onDragPlatformChange(null);
      onPlatformSelect(idx);
      platformDragRef.current = null;
      platformPreviewRef.current = null;
      return;
    }

    if (!map) { isPainting.current = false; return; }
    const { cx, cy } = getCanvasPos(e);
    const { wx, wy } = canvasToWorld(cx, cy);

    if (isPainting.current) {
      if (['TILE_BG', 'TILE_FG', 'ERASE_TILE'].includes(activeTool)) {
        onCommitPaint?.();
      } else if (['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK','OUTSIDEROOM','SPECIFICROOM'].includes(activeTool) && dragPlatform) {
        const { wx1, wy1 } = dragPlatform;
        const TOOL_TYPE_MAP = {
          RECT:         { type1: 0, type2: 0, typeName: 'RECTANGLE' },
          STAIRSUP:     { type1: 0, type2: 1, typeName: 'STAIRSUP' },
          STAIRSDOWN:   { type1: 0, type2: 2, typeName: 'STAIRSDOWN' },
          LADDER:       { type1: 1, type2: 0, typeName: 'LADDER' },
          TRACK:        { type1: 2, type2: 0, typeName: 'TRACK' },
          OUTSIDEROOM:  { type1: 3, type2: 0, typeName: 'OUTSIDEROOM' },
          SPECIFICROOM: { type1: 3, type2: 1, typeName: 'SPECIFICROOM' },
        };
        const t = TOOL_TYPE_MAP[activeTool];
        const x1 = Math.min(wx1, wx), y1 = Math.min(wy1, wy);
        const x2 = Math.max(wx1, wx), y2 = Math.max(wy1, wy);
        if (x2 - x1 > 2 && y2 - y1 > 2) onPlatformDraw({ x1, y1, x2, y2, ...t });
        onDragPlatformChange(null);
      }
    }
    isPainting.current = false;
  }, [map, activeTool, dragPlatform, dragActorPreview, canvasToWorld, onPlatformDraw, onDragPlatformChange, onCommitPaint, onActorMove, onPlatformUpdate, onPlatformSelect]);

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

  const cursorStyle = dragActorPreview
    ? 'grabbing'
    : (isPanning.current || isSpacePanning.current || isCtrlPanning.current)
      ? 'grab'
      : activeTool === 'SELECT' ? 'pointer'
      : activeTool === 'ERASE_TILE' ? 'crosshair'
      : isPainting.current ? 'crosshair'
      : 'default';

  return (
    <div className="relative w-full h-full overflow-hidden bg-[#050a05]">
      <canvas
        ref={canvasRef}
        style={{ cursor: cursorStyle, display: 'block', width: '100%', height: '100%' }}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
        onContextMenu={handleContextMenu}
      />
      <canvas
        ref={overlayCanvasRef}
        style={{ position: 'absolute', inset: 0, pointerEvents: 'none', width: '100%', height: '100%' }}
      />
    </div>
  );
}
