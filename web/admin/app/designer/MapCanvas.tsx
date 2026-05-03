'use client';
import { useRef, useEffect, useCallback, useState } from 'react';
import { ACTOR_DEFS, PLATFORM_TOOL_TYPES } from './Toolbar';
import { bakeSingleLight, buildOccluders, LIGHT_RADII } from './lightBaker';
import type { SilMapData, MapPlatform, NavLink, SpriteEntry, TileCell } from '../../lib/types';

// Platform overlay colors
const PLATFORM_COLORS: Record<string, string> = {
  RECTANGLE:   'rgba(80,130,255,0.3)',
  LADDER:      'rgba(255,220,50,0.4)',
  STAIRSUP:    'rgba(50,220,100,0.4)',
  STAIRSDOWN:  'rgba(255,140,50,0.4)',
  TRACK:       'rgba(180,100,255,0.4)',
  OUTSIDEROOM: 'rgba(255,80,80,0.3)',
  SPECIFICROOM:'rgba(255,80,200,0.3)',
};

function platformTypeName(type1: number, type2: number): string {
  if (type1 === 1 && type2 === 0) return 'LADDER';
  if (type1 === 0 && type2 === 1) return 'STAIRSUP';
  if (type1 === 0 && type2 === 2) return 'STAIRSDOWN';
  if (type1 === 2 && type2 === 0) return 'TRACK';
  if (type1 === 3 && type2 === 0) return 'OUTSIDEROOM';
  if (type1 === 3 && type2 === 1) return 'SPECIFICROOM';
  return 'RECTANGLE';
}

function getActorDef(id: number) {
  return ACTOR_DEFS.find(a => a.id === id) ?? { icon: '??', color: '#6b7280', label: 'Unknown', bank: null as number | null, frame: 0 };
}

export interface DragPlatform {
  wx1: number;
  wy1: number;
  wx2: number;
  wy2: number;
  typeName?: string;
  tool?: string;
}

interface PlatformDragState {
  mode: 'handle' | 'body';
  handle: string | null;
  idx: number;
  origPlatform: MapPlatform;
  startWx: number;
  startWy: number;
}

interface PlatformPreview {
  wx1: number;
  wy1: number;
  wx2: number;
  wy2: number;
}

interface DraggingActorState {
  idx: number;
  startWx: number;
  startWy: number;
  origX: number;
  origY: number;
  moved: boolean;
}

interface DragActorPreview {
  idx: number;
  wx: number;
  wy: number;
}

interface VisState {
  bg: boolean[];
  fg: boolean[];
  platforms: boolean;
  actors: boolean;
  grid: boolean;
  lighting: boolean;
  parallax: boolean;
}

interface TileRightClickInfo {
  tx: number;
  ty: number;
  layerType: 'bg' | 'fg';
  layerIdx: number;
  cell: TileCell | null;
  x: number;
  y: number;
}

interface Props {
  map: SilMapData | null;
  tileImages?: Map<number, ImageBitmap[]> | null;
  spriteImages?: Map<number, (SpriteEntry | null)[]> | null;
  vis?: VisState;
  activeTool: string;
  activeLayer: number;
  selectedTileId: number;
  zoom: number;
  pan: { x: number; y: number };
  onZoomChange: (zoom: number) => void;
  onPanChange: (val: { x: number; y: number } | ((prev: { x: number; y: number }) => { x: number; y: number })) => void;
  onTilePaint: (layerType: 'bg' | 'fg', layerIdx: number, tx: number, ty: number, tileId: number) => void;
  onFloodFill?: (layerType: 'bg' | 'fg', layerIdx: number, updates: Array<{ x: number; y: number; tile_id: number; flip: number; lum: number }>) => void;
  onPlatformDraw: (platform: MapPlatform) => void;
  onPlatformRemove: (idx: number) => void;
  onActorPlace: (pos: { wx: number; wy: number }) => void;
  onActorRemove: (idx: number) => void;
  onActorRightClick: (idx: number, screenX: number, screenY: number) => void;
  onTileRightClick: (info: TileRightClickInfo) => void;
  onBeginPaint?: () => void;
  onCommitPaint?: () => void;
  selectedActorId: number;
  dragPlatform: DragPlatform | null;
  onDragPlatformChange: (val: DragPlatform | null | ((prev: DragPlatform | null) => DragPlatform | null)) => void;
  onCursorChange: (cursor: { tx: number; ty: number; wx: number; wy: number }) => void;
  onActorMove?: (idx: number, x: number, y: number) => void;
  eraseLayerType: string;
  highlightActorIdx: number | null;
  selectedPlatformIdx: number | null;
  onPlatformSelect: (idx: number | null) => void;
  onPlatformUpdate: (idx: number, x1: number, y1: number, x2: number, y2: number) => void;
  onActorSelect?: (idx: number | null) => void;
  onActorFlip?: (idx: number) => void;
  onShadowZoneDraw?: (zone: { x1: number; y1: number; x2: number; y2: number }) => void;
  onShadowZoneRemove?: (idx: number) => void;
  onActorTypeChange?: (idx: number, type: number) => void;
  gridSize: number;
  tileSelection?: { tx1: number; ty1: number; tx2: number; ty2: number; layerType: 'bg' | 'fg'; layerIdx: number } | null;
  onTileSelection?: (sel: { tx1: number; ty1: number; tx2: number; ty2: number; layerType: 'bg' | 'fg'; layerIdx: number } | null) => void;
  tileCopyBuffer?: {
    w: number; h: number;
    bg: [Array<{ tile_id: number; flip: number; lum: number }>, Array<{ tile_id: number; flip: number; lum: number }>, Array<{ tile_id: number; flip: number; lum: number }>, Array<{ tile_id: number; flip: number; lum: number }>];
    fg: [Array<{ tile_id: number; flip: number; lum: number }>, Array<{ tile_id: number; flip: number; lum: number }>, Array<{ tile_id: number; flip: number; lum: number }>, Array<{ tile_id: number; flip: number; lum: number }>];
  } | null;
  pastePending?: boolean;
  onTilePaste?: (tx: number, ty: number) => void;
  navLinkType?: 0 | 1 | 2;
  linkFromIdx?: number | null;
  selectedNavLinkIdx?: number | null;
  onNavLinkAdd?: (fromIdx: number, toIdx: number, type: 0 | 1 | 2) => void;
  onNavLinkSelect?: (idx: number | null) => void;
  onLinkFromIdxChange?: (idx: number | null) => void;
}

export default function MapCanvas({
  map, tileImages, spriteImages, vis, activeTool, activeLayer, selectedTileId,
  zoom, pan, onZoomChange, onPanChange,
  onTilePaint, onFloodFill, onPlatformDraw, onPlatformRemove, onActorPlace, onActorRemove, onActorRightClick,
  onTileRightClick,
  onBeginPaint, onCommitPaint,
  selectedActorId, dragPlatform, onDragPlatformChange,
  onCursorChange,
  onActorMove,
  eraseLayerType,
  highlightActorIdx,
  selectedPlatformIdx, onPlatformSelect, onPlatformUpdate,
  onActorSelect,
  onActorFlip,
  onShadowZoneDraw,
  onShadowZoneRemove,
  onActorTypeChange,
  gridSize,
  tileSelection,
  onTileSelection,
  tileCopyBuffer,
  pastePending,
  onTilePaste,
  navLinkType,
  linkFromIdx,
  selectedNavLinkIdx,
  onNavLinkAdd,
  onNavLinkSelect,
  onLinkFromIdxChange,
}: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const overlayCanvasRef = useRef<HTMLCanvasElement>(null);
  const rafRef = useRef<number | null>(null);
  const isPainting = useRef(false);
  const isSpacePanning = useRef(false);
  const isCtrlPanning = useRef(false);
  const isPanning = useRef(false);
  const lastPan = useRef({ x: 0, y: 0 });
  const draggingActorRef = useRef<DraggingActorState | null>(null); // { idx, startWx, startWy, origX, origY, moved }
  const [dragActorPreview, setDragActorPreview] = useState<DragActorPreview | null>(null); // { idx, wx, wy } | null
  // Platform drag ref: { mode, handle, idx, origPlatform, startWx, startWy }
  const platformDragRef = useRef<PlatformDragState | null>(null);
  // Current preview bounds during platform drag { wx1, wy1, wx2, wy2 }
  const platformPreviewRef = useRef<PlatformPreview | null>(null);
  // Shadow zone drag: start world pos while drawing
  const shadowZoneDragRef = useRef<{ startWx: number; startWy: number; curWx: number; curWy: number } | null>(null);
  const [shadowZonePreview, setShadowZonePreview] = useState<{ x1: number; y1: number; x2: number; y2: number } | null>(null);
  // Tile selection drag
  const isSelectingTile = useRef(false);
  const tileSelStartRef = useRef<{ tx: number; ty: number } | null>(null);
  // Current hover tile (for paste preview — updated every mousemove, no re-render)
  const hoverTileRef = useRef<{ tx: number; ty: number }>({ tx: 0, ty: 0 });
  // Light radius ring drag: tracks active drag state
  const lightRadiusDragRef = useRef<{ actorIdx: number; origType: number } | null>(null);
  const lastEmittedSizeRef = useRef<number | null>(null);
  const [isLightRadiusDragging, setIsLightRadiusDragging] = useState(false);

  // Baked shadow preview canvases for static lights (dynShadows=false)
  const bakedLightPreviewsRef = useRef<Array<{ canvas: OffscreenCanvas; wx: number; wy: number; diam: number }>>([]);
  useEffect(() => {
    if (!map) { bakedLightPreviewsRef.current = []; return; }
    const occluders = buildOccluders(map.platforms, map.shadowZones);
    const previews: typeof bakedLightPreviewsRef.current = [];
    const HALF_ANGLE = Math.PI / 4;
    const cosHalf = Math.cos(HALF_ANGLE);
    const DIR_ANGLES = [0, -Math.PI/4, -Math.PI/2, -3*Math.PI/4, Math.PI, 3*Math.PI/4, Math.PI/2, Math.PI/4];
    for (const actor of map.actors) {
      if (actor.id !== 71) continue;
      const u = (actor.type ?? 0) >>> 0;
      const dynShadows = (u >>> 7) & 1;
      if (dynShadows) continue; // only static (baked) lights
      const size = u & 3;
      const shape = (u >>> 2) & 1;
      const radius = LIGHT_RADII[size] ?? 80;
      const diam = radius * 2;
      const data = bakeSingleLight(actor.x, actor.y, radius, occluders, shape, (actor.direction ?? 0) & 7);
      const cr = (u >>> 8) & 0xFF, cg = (u >>> 16) & 0xFF, cb = (u >>> 24) & 0xFF;
      const lr = cr || 255, lg = cg || 220, lb = cb || 100;
      const dirAngle = DIR_ANGLES[(actor.direction ?? 0) & 7];
      const cosDirX = Math.cos(dirAngle), sinDirY = Math.sin(dirAngle);
      const oc = new OffscreenCanvas(diam, diam);
      const octx = oc.getContext('2d')!;
      const imgData = octx.createImageData(diam, diam);
      for (let my = 0; my < diam; my++) {
        for (let mx = 0; mx < diam; mx++) {
          const idx = my * diam + mx;
          const dx = mx - radius, dy = my - radius;
          if (dx * dx + dy * dy >= radius * radius) continue;
          if (shape === 1) {
            const dist = Math.hypot(dx, dy);
            if (dist > 0.5 && (dx/dist)*cosDirX + (dy/dist)*sinDirY < cosHalf) continue;
          }
          const px = idx * 4;
          if (data[idx] === 1) {
            imgData.data[px] = lr; imgData.data[px+1] = lg; imgData.data[px+2] = lb;
            imgData.data[px+3] = 55;
          } else {
            imgData.data[px] = 0; imgData.data[px+1] = 0; imgData.data[px+2] = 20;
            imgData.data[px+3] = 110;
          }
        }
      }
      octx.putImageData(imgData, 0, 0);
      previews.push({ canvas: oc, wx: actor.x, wy: actor.y, diam });
    }
    bakedLightPreviewsRef.current = previews;
  }, [map]);

  // World → canvas coords
  const worldToCanvas = useCallback((wx: number, wy: number) => ({
    cx: wx * zoom + pan.x,
    cy: wy * zoom + pan.y,
  }), [zoom, pan]);

  // Canvas → world coords (in tiles)
  const canvasToTile = useCallback((cx: number, cy: number) => ({
    tx: Math.floor((cx - pan.x) / (64 * zoom)),
    ty: Math.floor((cy - pan.y) / (64 * zoom)),
  }), [zoom, pan]);

  // Canvas → world pixel coords
  const canvasToWorld = useCallback((cx: number, cy: number) => ({
    wx: (cx - pan.x) / zoom,
    wy: (cy - pan.y) / zoom,
  }), [zoom, pan]);

  // Snap a world coordinate to the grid (no-op when grid is off or gridSize is 0)
  const snap = useCallback((v: number) => {
    if (!vis?.grid || gridSize <= 0) return v;
    return Math.round(v / gridSize) * gridSize;
  }, [vis?.grid, gridSize]);

  // Draw the map
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d')!;
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
    const { width, height, layers, actors, platforms, shadowZones } = map;

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
    function blitTile(tile_id: number, flip: number, col: number, row: number) {
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

    // Draw parallax background stretched to cover the full map extent
    if (vis?.parallax !== false) {
      const parallaxIdx = map.header?.parallax ?? 0;
      const bgBank = spriteImages?.get(parallaxIdx);
      if (bgBank) {
        const BG_COLS = 20, BG_ROWS = 12;
        const bw = (width  * tileSize) / BG_COLS;
        const bh = (height * tileSize) / BG_ROWS;
        for (let row = 0; row < BG_ROWS; row++) {
          for (let col = 0; col < BG_COLS; col++) {
            const spr = bgBank[row * BG_COLS + col];
            if (!spr) continue;
            ctx.drawImage(spr.bitmap, col * bw + pan.x, row * bh + pan.y, bw, bh);
          }
        }
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
    function buildPlatformPath(cx1: number, cy1: number, cx2: number, cy2: number, typeName: string) {
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

    function drawPlatform(cx1: number, cy1: number, cx2: number, cy2: number, typeName: string, dashed: boolean) {
      const color = PLATFORM_COLORS[typeName] ?? 'rgba(80,130,255,0.3)';
      const strokeColor = color.replace(/[\d.]+\)$/, '0.9)');

      // Base fill
      ctx.lineWidth = 1;
      buildPlatformPath(cx1, cy1, cx2, cy2, typeName);
      ctx.fillStyle = color;
      ctx.fill();

      // Diagonal cross-stitch hatch — only for solid collision platforms
      if (typeName === 'RECTANGLE' || typeName === 'STAIRSUP' || typeName === 'STAIRSDOWN') {
        ctx.save();
        buildPlatformPath(cx1, cy1, cx2, cy2, typeName);
        ctx.clip();
        ctx.strokeStyle = strokeColor;
        ctx.lineWidth = 0.5;
        ctx.setLineDash([]);
        const step = 8;
        const px = Math.min(cx1, cx2), py = Math.min(cy1, cy2);
        const pw = Math.abs(cx2 - cx1), ph = Math.abs(cy2 - cy1);
        ctx.beginPath();
        // \ diagonals
        for (let d = -ph; d <= pw; d += step) {
          ctx.moveTo(px + d,      py);
          ctx.lineTo(px + d + ph, py + ph);
        }
        // / diagonals
        for (let d = -ph; d <= pw; d += step) {
          ctx.moveTo(px + d,      py + ph);
          ctx.lineTo(px + d + ph, py);
        }
        ctx.stroke();
        ctx.restore();
      }

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
      drawPlatform(cx1, cy1, cx2, cy2, typeName ?? 'RECTANGLE', true);
    }

    // Shadow zones (rendered after platforms, before actors)
    if (shadowZones && shadowZones.length > 0) {
      for (const z of shadowZones) {
        const zx1 = Math.min(z.x1, z.x2) * zoom + pan.x;
        const zy1 = Math.min(z.y1, z.y2) * zoom + pan.y;
        const zw  = Math.abs(z.x2 - z.x1) * zoom;
        const zh  = Math.abs(z.y2 - z.y1) * zoom;
        ctx.fillStyle = 'rgba(160,20,20,0.25)';
        ctx.fillRect(zx1, zy1, zw, zh);
        ctx.strokeStyle = 'rgba(220,60,60,0.8)';
        ctx.lineWidth = 1.5;
        ctx.setLineDash([4, 3]);
        ctx.strokeRect(zx1, zy1, zw, zh);
        ctx.setLineDash([]);
        if (zoom > 0.25) {
          ctx.fillStyle = 'rgba(220,100,100,0.9)';
          ctx.font = `${Math.max(9, 10 * zoom)}px monospace`;
          ctx.textAlign = 'left';
          ctx.textBaseline = 'top';
          ctx.fillText('SZ', zx1 + 3, zy1 + 2);
        }
      }
    }

    // Shadow zone preview while drawing
    if (shadowZonePreview) {
      const { x1, y1, x2, y2 } = shadowZonePreview;
      const px1 = Math.min(x1, x2) * zoom + pan.x;
      const py1 = Math.min(y1, y2) * zoom + pan.y;
      const pw  = Math.abs(x2 - x1) * zoom;
      const ph  = Math.abs(y2 - y1) * zoom;
      ctx.fillStyle = 'rgba(200,50,50,0.2)';
      ctx.fillRect(px1, py1, pw, ph);
      ctx.strokeStyle = 'rgba(255,80,80,0.9)';
      ctx.lineWidth = 1.5;
      ctx.setLineDash([4, 3]);
      ctx.strokeRect(px1, py1, pw, ph);
      ctx.setLineDash([]);
    }

    // Nav link arrows
    const NAV_LINK_COLORS: Record<number, string> = { 0: '#00ff88', 1: '#ffdd00', 2: '#ff6644' };
    const NAV_LINK_LABELS: Record<number, string> = { 0: 'JUMP', 1: 'FALL', 2: 'JETPACK' };
    const navLinks: NavLink[] = map.navLinks ?? [];
    if (navLinks.length > 0) {
      const ah = Math.max(8, 10 * zoom);
      for (let i = 0; i < navLinks.length; i++) {
        const link = navLinks[i];
        const from = platforms[link.fromIdx];
        const to = platforms[link.toIdx];
        if (!from || !to) continue;
        const fx = ((from.x1 + from.x2) / 2) * zoom + pan.x;
        const fy = Math.min(from.y1, from.y2) * zoom + pan.y;
        const tx = ((to.x1 + to.x2) / 2) * zoom + pan.x;
        const ty = Math.min(to.y1, to.y2) * zoom + pan.y;
        const isSelected = i === selectedNavLinkIdx;
        const color = isSelected ? '#ffffff' : (NAV_LINK_COLORS[link.type] ?? '#00ff88');
        ctx.strokeStyle = color;
        ctx.lineWidth = isSelected ? 3 : 2;
        ctx.setLineDash([]);
        ctx.beginPath();
        ctx.moveTo(fx, fy);
        ctx.lineTo(tx, ty);
        ctx.stroke();
        const angle = Math.atan2(ty - fy, tx - fx);
        ctx.fillStyle = color;
        ctx.beginPath();
        ctx.moveTo(tx, ty);
        ctx.lineTo(tx - ah * Math.cos(angle - Math.PI / 6), ty - ah * Math.sin(angle - Math.PI / 6));
        ctx.lineTo(tx - ah * Math.cos(angle + Math.PI / 6), ty - ah * Math.sin(angle + Math.PI / 6));
        ctx.closePath();
        ctx.fill();
        if (zoom > 0.2) {
          const mx = (fx + tx) / 2;
          const my = (fy + ty) / 2;
          ctx.font = `${Math.max(8, 9 * zoom)}px monospace`;
          ctx.fillStyle = color;
          ctx.textAlign = 'center';
          ctx.textBaseline = 'middle';
          ctx.fillText(NAV_LINK_LABELS[link.type] ?? '', mx, my - 8);
        }
      }
    }

    // Highlight linkFromIdx platform (pending NAV_LINK first-click)
    if (activeTool === 'NAV_LINK' && linkFromIdx != null && platforms[linkFromIdx]) {
      const p = platforms[linkFromIdx];
      const px1 = p.x1 * zoom + pan.x;
      const py1 = p.y1 * zoom + pan.y;
      const px2 = p.x2 * zoom + pan.x;
      const py2 = p.y2 * zoom + pan.y;
      ctx.strokeStyle = '#00ff88';
      ctx.lineWidth = 2;
      ctx.setLineDash([4, 3]);
      ctx.strokeRect(px1, py1, px2 - px1, py2 - py1);
      ctx.setLineDash([]);
    }

    // Actor icons
    if (vis?.actors !== false) {
      // Draw baked shadow previews for static lights before actor sprites
      if (vis?.lighting !== false && bakedLightPreviewsRef.current.length > 0) {
        ctx.save();
        ctx.globalCompositeOperation = 'source-over';
        for (const { canvas, wx, wy, diam } of bakedLightPreviewsRef.current) {
          const mx = wx * zoom + pan.x - (diam / 2) * zoom;
          const my = wy * zoom + pan.y - (diam / 2) * zoom;
          ctx.drawImage(canvas, mx, my, diam * zoom, diam * zoom);
        }
        ctx.restore();
      }
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
        if (a.id === 71) bankNum = 222; // light halo — frame from bits 0-1 of actor.type
        const lightFrame = a.id === 71 ? (a.type ?? 0) & 3 : null;

        // Try to draw actual sprite
        const sprBank = bankNum != null ? spriteImages?.get(bankNum) : null;
        const spr = sprBank?.[lightFrame != null ? lightFrame : (def.frame ?? 0)];
        if (spr) {
          // Light actors: draw sprite with additive-style glow overlay
          if (a.id === 71 && vis?.lighting !== false) {
            const gx = cx - (spr.width / 2) * zoom;
            const gy = cy - (spr.height / 2) * zoom;
            const gw = spr.width * zoom;
            const gh = spr.height * zoom;
            // Decode color from actortype bits 8-30
            const u = (a.type ?? 0) >>> 0;
            const cr = (u >>> 8) & 0xFF;
            const cg = (u >>> 16) & 0xFF;
            const cb = (u >>> 24) & 0xFF;
            const hasColor = cr || cg || cb;
            const inner = hasColor ? `rgba(${cr},${cg},${cb},0.5)` : 'rgba(255,220,100,0.35)';
            const outer = hasColor ? `rgba(${cr},${cg},${cb},0)` : 'rgba(255,180,50,0)';
            const grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, Math.max(gw, gh) / 2);
            grad.addColorStop(0, inner);
            grad.addColorStop(1, outer);
            ctx.fillStyle = grad;
            ctx.fillRect(gx, gy, gw, gh);
            ctx.drawImage(spr.bitmap, gx, gy, gw, gh);
            // Radius indicator: circle for halo, cone wedge for spot
            const lightSize = (u >>> 0) & 3;
            const lightRadius = lightSize === 2 ? 200 : lightSize === 1 ? 140 : 80;
            const lightShape = (u >>> 2) & 1; // 0=halo, 1=spot
            const ringColor = hasColor ? `rgba(${cr},${cg},${cb},0.45)` : 'rgba(255,200,80,0.45)';
            const fillColor = hasColor ? `rgba(${cr},${cg},${cb},0.08)` : 'rgba(255,200,80,0.08)';
            ctx.save();
            ctx.setLineDash([4, 4]);
            ctx.strokeStyle = ringColor;
            ctx.lineWidth = 1;
            if (lightShape === 1) {
              // Spot light: draw a 90° cone wedge in the actor's direction
              const DIR_ANGLES = [0, -Math.PI/4, -Math.PI/2, -3*Math.PI/4, Math.PI, 3*Math.PI/4, Math.PI/2, Math.PI/4];
              const dirAngle = DIR_ANGLES[(a.direction ?? 0) & 7];
              const half = Math.PI / 4;
              const r = lightRadius * zoom;
              ctx.beginPath();
              ctx.moveTo(cx, cy);
              ctx.arc(cx, cy, r, dirAngle - half, dirAngle + half);
              ctx.closePath();
              ctx.fillStyle = fillColor;
              ctx.fill();
              ctx.stroke();
            } else {
              ctx.beginPath();
              ctx.arc(cx, cy, lightRadius * zoom, 0, Math.PI * 2);
              ctx.stroke();
            }
            ctx.restore();
          } else {
          const mirrored = a.direction !== 0;
          const sx = cx - (mirrored ? spr.width - spr.offsetX : spr.offsetX) * zoom;
          const sy = cy - spr.offsetY * zoom;
          const sw = spr.width * zoom;
          const sh = spr.height * zoom;
          if (mirrored) {
            ctx.save();
            ctx.scale(-1, 1);
            ctx.drawImage(spr.bitmap, -sx - sw, sy, sw, sh);
            ctx.restore();
          } else {
            ctx.drawImage(spr.bitmap, sx, sy, sw, sh);
          }
          if (zoom > 0.3) {
            ctx.fillStyle = def.color + 'cc';
            ctx.fillRect(cx - 12, cy + 2, 24, 11);
            ctx.fillStyle = '#fff';
            ctx.fillText(def.icon, cx, cy + 7);
          }
          } // end else (non-light actor)
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
  }, [map, tileImages, spriteImages, vis, zoom, pan, dragPlatform, dragActorPreview, shadowZonePreview, selectedNavLinkIdx, linkFromIdx, activeTool]);

  // Resize canvas to fill container
  useEffect(() => {
    const canvas = canvasRef.current;
    const overlay = overlayCanvasRef.current;
    if (!canvas) return;
    const resizer = new ResizeObserver(() => {
      const rect = canvas.parentElement!.getBoundingClientRect();
      canvas.width  = rect.width;
      canvas.height = rect.height;
      if (overlay) { overlay.width = rect.width; overlay.height = rect.height; }
    });
    resizer.observe(canvas.parentElement!);
    return () => resizer.disconnect();
  }, []);

  // Animated marching-ants selection highlight + tile selection + paste preview on overlay canvas
  useEffect(() => {
    const overlay = overlayCanvasRef.current;
    if (!overlay) return;
    const ctx = overlay.getContext('2d')!;

    const hasActorHighlight = highlightActorIdx != null && map?.actors[highlightActorIdx];
    const hasPlatformHighlight = selectedPlatformIdx != null && map?.platforms[selectedPlatformIdx];
    const hasTileSelection = tileSelection != null;
    const hasPastePreview = !!(pastePending && tileCopyBuffer);

    if (!hasActorHighlight && !hasPlatformHighlight && !hasTileSelection && !hasPastePreview) {
      ctx.clearRect(0, 0, overlay.width, overlay.height);
      if (rafRef.current) { cancelAnimationFrame(rafRef.current); rafRef.current = null; }
      return;
    }

    const actor = hasActorHighlight ? map!.actors[highlightActorIdx!] : null;
    const def = actor ? getActorDef(actor.id) : null;

    function getSpriteRect(overrideCx?: number, overrideCy?: number) {
      if (!actor || !def) return null;
      let bankNum = def.bank;
      if (actor.id === 54) bankNum = actor.type === 0 ? 183 : 184;
      if (actor.id === 47) bankNum = 49 + Math.min(actor.type ?? 0, 9);
      if (actor.id === 63) bankNum = actor.type === 0 ? 200 : actor.type === 2 ? 201 : 205;
      if (actor.id === 71) bankNum = 222;
      const sprBank = bankNum != null ? spriteImages?.get(bankNum) : null;
      const lightFrame = actor.id === 71 ? ((actor.type ?? 0) >>> 0) & 3 : null;
      const spr = sprBank?.[lightFrame != null ? lightFrame : (def.frame ?? 0)];
      const cx = overrideCx ?? actor.x * zoom + pan.x;
      const cy = overrideCy ?? actor.y * zoom + pan.y;
      if (spr) {
        const pad = 3;
        return { x: cx - spr.offsetX * zoom - pad, y: cy - spr.offsetY * zoom - pad,
                 w: spr.width * zoom + pad * 2, h: spr.height * zoom + pad * 2,
                 circle: false, cx: 0, cy: 0, r: 0 };
      }
      const r = Math.max(8, 10 * zoom);
      return { circle: true, cx, cy, r: r + 3, x: 0, y: 0, w: 0, h: 0 };
    }

    // Draw X/Y axis gizmo at a canvas-space origin point
    function drawAxis(ocx: number, ocy: number) {
      const SHAFT = 56;
      const HEAD  = 9;
      const W     = overlay!.width;
      const H     = overlay!.height;
      ctx.save();
      ctx.setLineDash([]);

      // Faint full-canvas crosshair guides
      ctx.lineWidth = 0.75;
      ctx.strokeStyle = 'rgba(210,60,60,0.18)';
      ctx.beginPath(); ctx.moveTo(0, ocy); ctx.lineTo(W, ocy); ctx.stroke();
      ctx.strokeStyle = 'rgba(60,200,60,0.18)';
      ctx.beginPath(); ctx.moveTo(ocx, 0); ctx.lineTo(ocx, H); ctx.stroke();

      // X+ arrow (right, red)
      ctx.lineWidth = 2;
      ctx.strokeStyle = 'rgba(230,60,60,0.95)';
      ctx.fillStyle   = 'rgba(230,60,60,0.95)';
      ctx.beginPath(); ctx.moveTo(ocx, ocy); ctx.lineTo(ocx + SHAFT, ocy); ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(ocx + SHAFT, ocy);
      ctx.lineTo(ocx + SHAFT - HEAD, ocy - HEAD / 2);
      ctx.lineTo(ocx + SHAFT - HEAD, ocy + HEAD / 2);
      ctx.closePath(); ctx.fill();
      ctx.font = 'bold 10px monospace';
      ctx.fillText('X', ocx + SHAFT + 4, ocy + 4);

      // Y+ arrow (down — Y increases downward in game world coords, green)
      ctx.strokeStyle = 'rgba(60,200,60,0.95)';
      ctx.fillStyle   = 'rgba(60,200,60,0.95)';
      ctx.beginPath(); ctx.moveTo(ocx, ocy); ctx.lineTo(ocx, ocy + SHAFT); ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(ocx, ocy + SHAFT);
      ctx.lineTo(ocx - HEAD / 2, ocy + SHAFT - HEAD);
      ctx.lineTo(ocx + HEAD / 2, ocy + SHAFT - HEAD);
      ctx.closePath(); ctx.fill();
      ctx.fillText('Y', ocx + 4, ocy + SHAFT + 12);

      // Origin dot
      ctx.fillStyle = 'rgba(255,255,255,0.9)';
      ctx.beginPath(); ctx.arc(ocx, ocy, 3, 0, Math.PI * 2); ctx.fill();

      ctx.restore();
    }

    let dashOffset = 0;
    function draw() {
      ctx.clearRect(0, 0, overlay!.width, overlay!.height);

      // Actor marching-ants highlight + axis gizmo
      if (hasActorHighlight) {
        const isDragging = dragActorPreview?.idx === highlightActorIdx;
        const liveCx = isDragging ? dragActorPreview!.wx * zoom + pan.x : actor!.x * zoom + pan.x;
        const liveCy = isDragging ? dragActorPreview!.wy * zoom + pan.y : actor!.y * zoom + pan.y;

        // Shadow preview for selected light actors
        if (actor?.id === 71 && map) {
          const lx = liveCx, ly = liveCy;
          // Light radius in world pixels by size (bits 0-1 of actortype)
          const lightSize = ((actor.type ?? 0) >>> 0) & 3;
          const lightRange = lightSize === 2 ? 420 : lightSize === 1 ? 280 : 160;
          const REACH = lightRange * 2 * zoom;
          const RECTANGLE_TYPE = 1;

          const projectShadow = (r: { x1: number; y1: number; x2: number; y2: number }) => {
            const minX = Math.min(r.x1, r.x2), maxX = Math.max(r.x1, r.x2);
            const minY = Math.min(r.y1, r.y2), maxY = Math.max(r.y1, r.y2);
            // Skip if light is inside occluder
            if (actor.x >= minX && actor.x <= maxX && actor.y >= minY && actor.y <= maxY) return;
            // Cull if center is too far
            const cx = (minX + maxX) / 2, cy = (minY + maxY) / 2;
            if (Math.hypot(cx - actor.x, cy - actor.y) > lightRange * 1.5) return;
            // Convert corners to screen space
            const corners: [number, number][] = [
              [r.x1 * zoom + pan.x, r.y1 * zoom + pan.y],
              [r.x2 * zoom + pan.x, r.y1 * zoom + pan.y],
              [r.x2 * zoom + pan.x, r.y2 * zoom + pan.y],
              [r.x1 * zoom + pan.x, r.y2 * zoom + pan.y],
            ];
            // Sort corners by angle from light, find largest gap (= silhouette edges)
            const wa = corners.map(([cx, cy]) => ({ cx, cy, a: Math.atan2(cy - ly, cx - lx) }))
              .sort((a, b) => a.a - b.a);
            let maxGap = -Infinity, maxI = 0;
            for (let i = 0; i < 4; i++) {
              const gap = ((wa[(i + 1) % 4].a - wa[i].a + 3 * Math.PI) % (2 * Math.PI));
              if (gap > maxGap) { maxGap = gap; maxI = i; }
            }
            const c1 = wa[(maxI + 1) % 4], c2 = wa[maxI];
            const proj = (cx: number, cy: number): [number, number] => {
              const dx = cx - lx, dy = cy - ly;
              const len = Math.hypot(dx, dy) || 1;
              return [cx + (dx / len) * REACH, cy + (dy / len) * REACH];
            };
            ctx.beginPath();
            ctx.moveTo(c1.cx, c1.cy);
            const [p1x, p1y] = proj(c1.cx, c1.cy);
            ctx.lineTo(p1x, p1y);
            const [p2x, p2y] = proj(c2.cx, c2.cy);
            ctx.lineTo(p2x, p2y);
            ctx.lineTo(c2.cx, c2.cy);
            ctx.closePath();
            ctx.fill();
          };

          ctx.save();
          ctx.globalAlpha = 0.5;
          ctx.fillStyle = '#000033';
          for (const p of map.platforms) {
            if (p.type1 !== 0) continue; // only solid platforms cast shadows
            projectShadow(p);
          }
          for (const z of map.shadowZones) {
            projectShadow(z);
          }
          ctx.restore();
        }

        const rect = getSpriteRect(liveCx, liveCy);
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
        if (actor) drawAxis(liveCx, liveCy);
      }

      // Platform marching-ants highlight + resize handles
      if (hasPlatformHighlight) {
        const p = map!.platforms[selectedPlatformIdx!];
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

        drawAxis((cx1 + cx2) / 2, (cy1 + cy2) / 2);
      }

      // Tile selection rect
      if (hasTileSelection) {
        const { tx1, ty1, tx2, ty2 } = tileSelection!;
        const sx = tx1 * 64 * zoom + pan.x;
        const sy = ty1 * 64 * zoom + pan.y;
        const sw = (tx2 - tx1 + 1) * 64 * zoom;
        const sh = (ty2 - ty1 + 1) * 64 * zoom;
        ctx.save();
        ctx.fillStyle = 'rgba(80,150,255,0.12)';
        ctx.fillRect(sx, sy, sw, sh);
        ctx.lineWidth = 2;
        ctx.setLineDash([6, 4]);
        ctx.strokeStyle = 'rgba(130,190,255,0.9)';
        ctx.lineDashOffset = -dashOffset;
        ctx.strokeRect(sx, sy, sw, sh);
        ctx.strokeStyle = 'rgba(255,255,255,0.5)';
        ctx.lineDashOffset = -dashOffset + 5;
        ctx.strokeRect(sx, sy, sw, sh);
        ctx.restore();
      }

      // Paste preview at hover tile — render all 8 layers (bg[0..3] then fg[0..3]) composited
      if (hasPastePreview) {
        const { tx: hx, ty: hy } = hoverTileRef.current;
        const { w, h, bg, fg } = tileCopyBuffer!;
        ctx.save();
        ctx.globalAlpha = 0.55;
        const allLayers = [...bg, ...fg];
        const ts = 64 * zoom;
        for (const layerCells of allLayers) {
          for (let dy = 0; dy < h; dy++) {
            for (let dx = 0; dx < w; dx++) {
              const tile = layerCells[dy * w + dx];
              if (!tile || tile.tile_id === 0) continue;
              const bank = (tile.tile_id >> 8) & 0xFF;
              const slot = tile.tile_id & 0xFF;
              const bitmaps = tileImages?.get(bank);
              const img = bitmaps?.[slot];
              if (!img) continue;
              const px = (hx + dx) * ts + pan.x;
              const py = (hy + dy) * ts + pan.y;
              if (tile.flip) {
                ctx.save();
                ctx.translate(px + ts, py);
                ctx.scale(-1, 1);
                ctx.drawImage(img, 0, 0, ts, ts);
                ctx.restore();
              } else {
                ctx.drawImage(img, px, py, ts, ts);
              }
            }
          }
        }
        ctx.globalAlpha = 1;
        ctx.lineWidth = 2;
        ctx.setLineDash([5, 3]);
        ctx.strokeStyle = 'rgba(255,220,60,0.9)';
        ctx.lineDashOffset = -dashOffset;
        ctx.strokeRect(hx * 64 * zoom + pan.x, hy * 64 * zoom + pan.y, w * 64 * zoom, h * 64 * zoom);
        ctx.restore();
      }

      dashOffset = (dashOffset + 0.5) % 10;
      rafRef.current = requestAnimationFrame(draw);
    }
    rafRef.current = requestAnimationFrame(draw);
    return () => { if (rafRef.current) { cancelAnimationFrame(rafRef.current); rafRef.current = null; } };
  }, [highlightActorIdx, selectedPlatformIdx, map, spriteImages, zoom, pan, dragActorPreview, tileSelection, tileCopyBuffer, pastePending, tileImages]);

  // Mouse event handlers
  const getCanvasPos = (e: { clientX: number; clientY: number }) => {
    const rect = canvasRef.current!.getBoundingClientRect();
    return { cx: e.clientX - rect.left, cy: e.clientY - rect.top };
  };

  const handleWheel = useCallback((e: WheelEvent) => {
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

  const handleMouseDown = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
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

    // Paste intercept: left-click in bounds while paste is pending stamps the buffer
    if (pastePending && tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
      onTilePaste?.(tx, ty);
      return;
    }

    if (activeTool === 'FLOOD_FILL') {
      if (!onFloodFill || !selectedTileId || tx < 0 || tx >= map.width || ty < 0 || ty >= map.height) return;
      const lt = (eraseLayerType ?? 'bg') as 'bg' | 'fg';
      const layerArr = lt === 'fg' ? map.layers.fg : map.layers.bg;
      const targetId = layerArr[activeLayer][ty * map.width + tx]?.tile_id ?? 0;
      if (targetId === selectedTileId) return;
      // BFS 4-connected flood fill
      const visited = new Uint8Array(map.width * map.height);
      const queue: Array<[number, number]> = [[tx, ty]];
      const updates: Array<{ x: number; y: number; tile_id: number; flip: number; lum: number }> = [];
      visited[ty * map.width + tx] = 1;
      while (queue.length) {
        const [cx, cy] = queue.shift()!;
        updates.push({ x: cx, y: cy, tile_id: selectedTileId, flip: 0, lum: 0 });
        for (const [nx, ny] of [[cx-1,cy],[cx+1,cy],[cx,cy-1],[cx,cy+1]] as [number,number][]) {
          if (nx < 0 || nx >= map.width || ny < 0 || ny >= map.height) continue;
          if (visited[ny * map.width + nx]) continue;
          if ((layerArr[activeLayer][ny * map.width + nx]?.tile_id ?? 0) !== targetId) continue;
          visited[ny * map.width + nx] = 1;
          queue.push([nx, ny]);
        }
      }
      onFloodFill(lt, activeLayer, updates);
      return;
    }

    if (activeTool === 'TILE_SELECT') {
      if (tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
        isSelectingTile.current = true;
        tileSelStartRef.current = { tx, ty };
        onTileSelection?.({ tx1: tx, ty1: ty, tx2: tx, ty2: ty, layerType: 'bg', layerIdx: 0 });
      }
    } else if (activeTool === 'TILE_BG' || activeTool === 'TILE_FG') {
      isPainting.current = true;
      onBeginPaint?.();
      if (selectedTileId && tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
        onTilePaint(activeTool === 'TILE_FG' ? 'fg' : 'bg', activeLayer, tx, ty, selectedTileId);
      }
    } else if (activeTool === 'ERASE_TILE') {
      isPainting.current = true;
      onBeginPaint?.();
      if (tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
        onTilePaint((eraseLayerType ?? 'bg') as 'bg' | 'fg', activeLayer, tx, ty, 0);
      }
    } else if (['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK','OUTSIDEROOM','SPECIFICROOM'].includes(activeTool)) {
      isPainting.current = true;
      onDragPlatformChange({ wx1: snap(wx), wy1: snap(wy), wx2: snap(wx), wy2: snap(wy), tool: activeTool });
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
      onActorPlace({ wx: snap(wx), wy: snap(wy) });
    } else if (activeTool === 'SHADOW_ZONE') {
      isPainting.current = true;
      shadowZoneDragRef.current = { startWx: snap(wx), startWy: snap(wy), curWx: snap(wx), curWy: snap(wy) };
      setShadowZonePreview({ x1: snap(wx), y1: snap(wy), x2: snap(wx), y2: snap(wy) });
    } else if (activeTool === 'NAV_LINK') {
      let hitIdx: number | null = null;
      for (let i = map.platforms.length - 1; i >= 0; i--) {
        const p = map.platforms[i];
        if (wx >= p.x1 && wx <= p.x2 && wy >= p.y1 && wy <= p.y2) { hitIdx = i; break; }
      }
      if (hitIdx !== null) {
        if (linkFromIdx == null) {
          onLinkFromIdxChange?.(hitIdx);
        } else if (hitIdx !== linkFromIdx) {
          onNavLinkAdd?.(linkFromIdx, hitIdx, navLinkType ?? 0);
          onLinkFromIdxChange?.(null);
        }
      }
    } else if (activeTool === 'SELECT') {
      const handleSize = 8 / zoom;
      const hs = handleSize / 2;

      // 0. Check nav links for selection
      const navLinksArr: NavLink[] = map.navLinks ?? [];
      if (navLinksArr.length > 0) {
        const LINK_HIT = 12 / zoom;
        for (let i = navLinksArr.length - 1; i >= 0; i--) {
          const link = navLinksArr[i];
          const from = map.platforms[link.fromIdx];
          const to = map.platforms[link.toIdx];
          if (!from || !to) continue;
          const fx = (from.x1 + from.x2) / 2;
          const fy = Math.min(from.y1, from.y2);
          const tx = (to.x1 + to.x2) / 2;
          const ty = Math.min(to.y1, to.y2);
          const dx = tx - fx, dy = ty - fy;
          const len2 = dx * dx + dy * dy;
          const t = len2 === 0 ? 0 : Math.max(0, Math.min(1, ((wx - fx) * dx + (wy - fy) * dy) / len2));
          const dist = Math.hypot(wx - (fx + t * dx), wy - (fy + t * dy));
          if (dist < LINK_HIT) {
            onNavLinkSelect?.(i);
            onPlatformSelect(null);
            onActorSelect?.(null);
            return;
          }
        }
      }

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

      // 1.5. Light radius ring drag — if the selected actor is a light and the click is near its ring
      if (highlightActorIdx != null && map.actors[highlightActorIdx]?.id === 71) {
        const a = map.actors[highlightActorIdx];
        const lightSize = (a.type ?? 0) & 3;
        const lightRadius = lightSize === 2 ? 200 : lightSize === 1 ? 140 : 80;
        if (Math.abs(Math.hypot(wx - a.x, wy - a.y) - lightRadius) < 12) {
          lightRadiusDragRef.current = { actorIdx: highlightActorIdx, origType: a.type ?? 0 };
          lastEmittedSizeRef.current = lightSize;
          setIsLightRadiusDragging(true);
          return;
        }
      }

      // 2. Hit-test actors — priority over new platform selection; cycle through stack on repeated clicks
      const HIT = 48 / zoom;
      const hits: number[] = [];
      for (let i = map.actors.length - 1; i >= 0; i--) {
        if (Math.hypot(map.actors[i].x - wx, map.actors[i].y - wy) < HIT) hits.push(i);
      }
      if (hits.length > 0) {
        // If the currently selected actor is already in this stack, cycle to the next
        const stackPos = hits.indexOf(highlightActorIdx ?? -1);
        const nextIdx = stackPos >= 0 ? hits[(stackPos + 1) % hits.length] : hits[0];
        const a = map.actors[nextIdx];
        onActorSelect?.(nextIdx);
        onPlatformSelect(null);
        draggingActorRef.current = { idx: nextIdx, startWx: wx, startWy: wy, origX: a.x, origY: a.y, moved: false };
        setDragActorPreview({ idx: nextIdx, wx: a.x, wy: a.y });
        return;
      }

      // 3. Hit-test all platforms for selection
      for (let i = map.platforms.length - 1; i >= 0; i--) {
        const p = map.platforms[i];
        if (wx >= p.x1 && wx <= p.x2 && wy >= p.y1 && wy <= p.y2) {
          onPlatformSelect(i);
          onActorSelect?.(null);
          return;
        }
      }

      // 4. Click on empty → deselect all
      onPlatformSelect(null);
      onActorSelect?.(null);
    }
  }, [map, activeTool, activeLayer, selectedTileId, canvasToTile, canvasToWorld, zoom, eraseLayerType,
      onTilePaint, onPlatformRemove, onActorPlace, onDragPlatformChange, onBeginPaint,
      selectedPlatformIdx, onPlatformSelect, onActorSelect, highlightActorIdx, snap,
      pastePending, onTilePaste, onTileSelection, onFloodFill, onActorTypeChange,
      linkFromIdx, navLinkType, onNavLinkAdd, onNavLinkSelect, onLinkFromIdxChange]);

  // Right-click: actors take priority, fall through to tile property editor
  const handleContextMenu = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    e.preventDefault();
    if (!map) return;

    // NAV_LINK: right-click cancels pending from-idx
    if (activeTool === 'NAV_LINK') {
      onLinkFromIdxChange?.(null);
      return;
    }

    const { cx, cy } = getCanvasPos(e);
    const { wx, wy } = canvasToWorld(cx, cy);

    // Check actors first
    const HIT_RADIUS = 96 / zoom;
    let best: number | null = null, bestDist = HIT_RADIUS;
    for (let i = map.actors.length - 1; i >= 0; i--) {
      const a = map.actors[i];
      const dist = Math.hypot(a.x - wx, a.y - wy);
      if (dist < bestDist) { bestDist = dist; best = i; }
    }
    if (best !== null && onActorRightClick) {
      onActorRightClick(best, e.clientX, e.clientY);
      return;
    }

    // Right-click on a shadow zone removes it
    if (activeTool === 'SHADOW_ZONE' && map.shadowZones) {
      for (let i = map.shadowZones.length - 1; i >= 0; i--) {
        const z = map.shadowZones[i];
        const x1 = Math.min(z.x1, z.x2), x2 = Math.max(z.x1, z.x2);
        const y1 = Math.min(z.y1, z.y2), y2 = Math.max(z.y1, z.y2);
        if (wx >= x1 && wx <= x2 && wy >= y1 && wy <= y2) {
          onShadowZoneRemove?.(i);
          return;
        }
      }
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
  }, [map, canvasToWorld, canvasToTile, zoom, activeTool, activeLayer, onActorRightClick, onTileRightClick, onShadowZoneRemove, onLinkFromIdxChange]);

  const handleMouseMove = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
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

    // Always update hover tile for paste preview (no re-render)
    hoverTileRef.current = { tx, ty };

    // Live tile selection drag
    if (isSelectingTile.current && tileSelStartRef.current) {
      const { tx: stx, ty: sty } = tileSelStartRef.current;
      onTileSelection?.({
        tx1: Math.min(stx, tx), ty1: Math.min(sty, ty),
        tx2: Math.max(stx, tx), ty2: Math.max(sty, ty),
        layerType: 'bg', layerIdx: 0,
      });
      return;
    }

    // Platform drag (SELECT tool — handle or body move)
    if (platformDragRef.current) {
      const { mode, handle, origPlatform, startWx, startWy } = platformDragRef.current;
      const dx = wx - startWx;
      const dy = wy - startWy;
      const MIN_SIZE = 16;
      let { x1, y1, x2, y2 } = origPlatform;

      if (mode === 'body') {
        const rawX1 = origPlatform.x1 + dx;
        const rawY1 = origPlatform.y1 + dy;
        x1 = snap(rawX1);
        y1 = snap(rawY1);
        x2 = x1 + (origPlatform.x2 - origPlatform.x1);
        y2 = y1 + (origPlatform.y2 - origPlatform.y1);
      } else {
        switch (handle) {
          case 'TL': x1 = snap(origPlatform.x1 + dx); y1 = snap(origPlatform.y1 + dy); break;
          case 'TR': x2 = snap(origPlatform.x2 + dx); y1 = snap(origPlatform.y1 + dy); break;
          case 'BL': x1 = snap(origPlatform.x1 + dx); y2 = snap(origPlatform.y2 + dy); break;
          case 'BR': x2 = snap(origPlatform.x2 + dx); y2 = snap(origPlatform.y2 + dy); break;
          case 'T':  y1 = snap(origPlatform.y1 + dy); break;
          case 'B':  y2 = snap(origPlatform.y2 + dy); break;
          case 'L':  x1 = snap(origPlatform.x1 + dx); break;
          case 'R':  x2 = snap(origPlatform.x2 + dx); break;
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
      const newWx = snap(origX + (wx - startWx));
      const newWy = snap(origY + (wy - startWy));
      draggingActorRef.current.moved = Math.hypot(wx - startWx, wy - startWy) > 4;
      setDragActorPreview({ idx: draggingActorRef.current.idx, wx: newWx, wy: newWy });
      return;
    }

    // Light radius ring drag (SELECT tool)
    if (lightRadiusDragRef.current) {
      const { actorIdx, origType } = lightRadiusDragRef.current;
      const a = map.actors[actorIdx];
      if (a) {
        const dist = Math.hypot(wx - a.x, wy - a.y);
        const newSize = dist < 110 ? 0 : dist < 170 ? 1 : 2;
        if (newSize !== lastEmittedSizeRef.current) {
          lastEmittedSizeRef.current = newSize;
          onActorTypeChange?.(actorIdx, (origType & ~3) | newSize);
        }
      }
      return;
    }

    if (isPainting.current) {
      if (activeTool === 'TILE_BG' || activeTool === 'TILE_FG') {
        if (selectedTileId && tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
          onTilePaint(activeTool === 'TILE_FG' ? 'fg' : 'bg', activeLayer, tx, ty, selectedTileId);
        }
      } else if (activeTool === 'ERASE_TILE') {
        if (tx >= 0 && tx < map.width && ty >= 0 && ty < map.height) {
          onTilePaint((eraseLayerType ?? 'bg') as 'bg' | 'fg', activeLayer, tx, ty, 0);
        }
      } else if (['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK','OUTSIDEROOM','SPECIFICROOM'].includes(activeTool)) {
        onDragPlatformChange(prev => prev ? { ...prev, wx2: snap(wx), wy2: snap(wy) } : null);
      } else if (activeTool === 'SHADOW_ZONE' && shadowZoneDragRef.current) {
        const snWx = snap(wx), snWy = snap(wy);
        shadowZoneDragRef.current.curWx = snWx;
        shadowZoneDragRef.current.curWy = snWy;
        setShadowZonePreview({ x1: shadowZoneDragRef.current.startWx, y1: shadowZoneDragRef.current.startWy, x2: snWx, y2: snWy });
      }
    }
  }, [map, activeTool, activeLayer, selectedTileId, canvasToTile, canvasToWorld, eraseLayerType,
      onTilePaint, onPanChange, onCursorChange, onDragPlatformChange, snap,
      onTileSelection, onActorTypeChange]);

  const handleMouseUp = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    if (isPanning.current) {
      isPanning.current = false;
      return;
    }

    // Finish tile selection drag
    if (isSelectingTile.current) {
      isSelectingTile.current = false;
      tileSelStartRef.current = null;
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

    // Finish light radius ring drag
    if (lightRadiusDragRef.current) {
      const { actorIdx, origType } = lightRadiusDragRef.current;
      lightRadiusDragRef.current = null;
      lastEmittedSizeRef.current = null;
      setIsLightRadiusDragging(false);
      if (map?.actors[actorIdx]) {
        const a = map.actors[actorIdx];
        const { cx, cy } = getCanvasPos(e);
        const { wx, wy } = canvasToWorld(cx, cy);
        const dist = Math.hypot(wx - a.x, wy - a.y);
        const newSize = dist < 110 ? 0 : dist < 170 ? 1 : 2;
        onActorTypeChange?.(actorIdx, (origType & ~3) | newSize);
      }
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
        const snWx = snap(wx), snWy = snap(wy);
        const t = PLATFORM_TOOL_TYPES[activeTool];
        const x1 = Math.min(wx1, snWx), y1 = Math.min(wy1, snWy);
        const x2 = Math.max(wx1, snWx), y2 = Math.max(wy1, snWy);
        if (x2 - x1 > 2 && y2 - y1 > 2 && t) onPlatformDraw({ x1, y1, x2, y2, ...t });
        onDragPlatformChange(null);
      } else if (activeTool === 'SHADOW_ZONE' && shadowZoneDragRef.current) {
        const { startWx, startWy, curWx, curWy } = shadowZoneDragRef.current;
        const x1 = Math.min(startWx, curWx), y1 = Math.min(startWy, curWy);
        const x2 = Math.max(startWx, curWx), y2 = Math.max(startWy, curWy);
        if (x2 - x1 > 4 && y2 - y1 > 4) onShadowZoneDraw?.({ x1, y1, x2, y2 });
        shadowZoneDragRef.current = null;
        setShadowZonePreview(null);
      }
    }
    isPainting.current = false;
  }, [map, activeTool, dragPlatform, dragActorPreview, canvasToWorld, onPlatformDraw, onDragPlatformChange, onCommitPaint, onActorMove, onPlatformUpdate, onPlatformSelect, snap, onShadowZoneDraw, onActorTypeChange]);

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    if (e.code === 'Space') { isSpacePanning.current = true; e.preventDefault(); }
    if (e.code === 'ControlLeft' || e.code === 'ControlRight') isCtrlPanning.current = true;
    if (e.code === 'KeyF' && highlightActorIdx != null) {
      onActorFlip?.(highlightActorIdx);
      e.preventDefault();
    }
  }, [highlightActorIdx, onActorFlip]);
  const handleKeyUp = useCallback((e: KeyboardEvent) => {
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

  const cursorStyle = isLightRadiusDragging
    ? 'ew-resize'
    : dragActorPreview
    ? 'grabbing'
    : (isPanning.current || isSpacePanning.current || isCtrlPanning.current)
      ? 'grab'
      : pastePending ? 'copy'
      : activeTool === 'FLOOD_FILL' ? 'cell'
      : activeTool === 'SELECT' ? 'pointer'
      : activeTool === 'TILE_SELECT' ? 'crosshair'
      : activeTool === 'ERASE_TILE' ? 'crosshair'
      : activeTool === 'SHADOW_ZONE' ? 'crosshair'
      : activeTool === 'NAV_LINK' ? 'crosshair'
      : isPainting.current ? 'crosshair'
      : 'default';

  // suppress unused import warning — PLATFORM_TOOL_TYPES is used in handleMouseUp via the identifier
  void worldToCanvas;

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
