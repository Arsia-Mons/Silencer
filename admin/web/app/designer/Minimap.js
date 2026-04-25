'use client';
import { useRef, useEffect, useState } from 'react';

const MM_MAX_W = 160;
const MM_MAX_H = 120;

export default function Minimap({ map, tileImages, zoom, pan, containerRef, onPanTo }) {
  const canvasRef = useRef(null);
  const [hovered, setHovered] = useState(false);

  const mapW = map?.width  ?? 40;
  const mapH = map?.height ?? 30;
  const scale = Math.min(MM_MAX_W / mapW, MM_MAX_H / mapH, 4);
  const mmW = Math.max(1, Math.round(mapW * scale));
  const mmH = Math.max(1, Math.round(mapH * scale));

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !map) return;
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, mmW, mmH);
    ctx.fillStyle = '#080c08';
    ctx.fillRect(0, 0, mmW, mmH);

    const { width, height, layers, actors } = map;
    const tS = scale;

    // Draw bg layer 0
    for (let row = 0; row < height; row++) {
      for (let col = 0; col < width; col++) {
        const cell = layers.bg[0][row * width + col];
        if (!cell || !cell.tile_id) continue;
        const bank = (cell.tile_id >> 8) & 0xFF;
        const idx  = cell.tile_id & 0xFF;
        const bmp  = tileImages?.get(bank)?.[idx];
        if (bmp) {
          ctx.drawImage(bmp, col * tS, row * tS, tS, tS);
        } else {
          ctx.fillStyle = '#1a3a1a';
          ctx.fillRect(col * tS, row * tS, tS, tS);
        }
      }
    }

    // Draw actors as colored dots
    for (const a of actors) {
      const ax = (a.x / 64) * tS;
      const ay = (a.y / 64) * tS;
      ctx.fillStyle = '#22d3ee';
      ctx.fillRect(ax - 1, ay - 1, 3, 3);
    }

    // Viewport rect
    const container = containerRef?.current;
    if (container && zoom && pan) {
      const { width: cw, height: ch } = container.getBoundingClientRect();
      const vpX1 = (-pan.x / zoom / 64) * tS;
      const vpY1 = (-pan.y / zoom / 64) * tS;
      const vpW  = (cw / zoom / 64) * tS;
      const vpH  = (ch / zoom / 64) * tS;
      ctx.strokeStyle = '#4a8a4a';
      ctx.lineWidth = 1;
      ctx.strokeRect(vpX1, vpY1, vpW, vpH);
    }
  }, [map, tileImages, zoom, pan, mmW, mmH, scale, containerRef]);

  const handleClick = (e) => {
    if (!map || !onPanTo) return;
    const rect = canvasRef.current.getBoundingClientRect();
    const cx = e.clientX - rect.left;
    const cy = e.clientY - rect.top;
    onPanTo((cx / scale) * 64, (cy / scale) * 64);
  };

  if (!map) return null;

  return (
    <div
      className="absolute bottom-2 right-2 z-10 border border-game-border rounded overflow-hidden shadow-xl"
      style={{ opacity: hovered ? 1 : 0.7, transition: 'opacity 0.15s' }}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
    >
      <canvas
        ref={canvasRef}
        width={mmW}
        height={mmH}
        onClick={handleClick}
        style={{ display: 'block', cursor: 'crosshair' }}
      />
    </div>
  );
}
