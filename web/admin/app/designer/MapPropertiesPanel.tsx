'use client';
import { useState, useEffect, useRef } from 'react';
import type { MapHeader, SpriteEntry } from '../../lib/types';

function BgThumb({ idx, spriteImages, selected, onSelect }: {
  idx: number;
  spriteImages: Map<number, (SpriteEntry | null)[]> | null | undefined;
  selected: boolean;
  onSelect: () => void;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d')!;
    const W = canvas.width, H = canvas.height;
    ctx.fillStyle = '#0a0a0f';
    ctx.fillRect(0, 0, W, H);
    const bank = spriteImages?.get(idx);
    if (!bank) {
      ctx.fillStyle = '#1a1a2e';
      ctx.fillRect(0, 0, W, H);
      return;
    }
    const BG_COLS = 20, BG_ROWS = 12;
    const tw = W / BG_COLS, th = H / BG_ROWS;
    for (let r = 0; r < BG_ROWS; r++) {
      for (let c = 0; c < BG_COLS; c++) {
        const spr = bank[r * BG_COLS + c];
        if (spr) ctx.drawImage(spr.bitmap, c * tw, r * th, tw, th);
      }
    }
  }, [idx, spriteImages]);

  return (
    <button
      onClick={onSelect}
      title={`Background ${idx}`}
      className={`flex flex-col items-center gap-1 rounded border-2 p-1 transition-colors flex-shrink-0 ${
        selected
          ? 'border-game-primary bg-[#0d1a0d]'
          : 'border-game-border/50 hover:border-game-border bg-transparent'
      }`}
    >
      <canvas ref={canvasRef} width={100} height={60} className="rounded-sm block" />
      <span className={`text-[10px] font-mono ${selected ? 'text-game-primary' : 'text-game-textDim'}`}>
        BG {idx}
      </span>
    </button>
  );
}

interface Props {
  header: MapHeader | null;
  onUpdate: (patch: Partial<MapHeader>) => void;
  onClose: () => void;
  spriteImages?: Map<number, (SpriteEntry | null)[]> | null;
}

export default function MapPropertiesPanel({ header, onUpdate, onClose, spriteImages }: Props) {
  const [fields, setFields] = useState({
    description: header?.description ?? '',
    ambience:    String(header?.ambience   ?? 0),
    maxplayers:  String(header?.maxplayers ?? 8),
    maxteams:    String(header?.maxteams   ?? 2),
  });

  useEffect(() => {
    setFields({
      description: header?.description ?? '',
      ambience:    String(header?.ambience   ?? 0),
      maxplayers:  String(header?.maxplayers ?? 8),
      maxteams:    String(header?.maxteams   ?? 2),
    });
  }, [header]);

  const apply = () => {
    onUpdate({
      description: fields.description.slice(0, 127),
      ambience:    Math.max(-128, Math.min(127, parseInt(fields.ambience,   10) || 0)),
      maxplayers:  Math.max(1,    Math.min(8,   parseInt(fields.maxplayers, 10) || 8)),
      maxteams:    Math.max(1,    Math.min(4,   parseInt(fields.maxteams,   10) || 2)),
    });
  };

  const currentParallax = header?.parallax ?? 0;

  const inp = 'w-full bg-game-dark border border-game-border text-game-text text-xs px-2 py-1 rounded font-mono focus:outline-none focus:border-game-primary';
  const lbl = 'text-game-textDim text-xs font-mono mb-0.5';

  return (
    <div className="bg-game-bgCard border-b border-game-border px-4 py-3 flex flex-col gap-3">
      {/* Background picker */}
      <div>
        <div className={`${lbl} mb-1`}>Background</div>
        <div className="flex gap-2 overflow-x-auto pb-1">
          {[0, 1, 2, 3, 4].map(i => (
            <BgThumb
              key={i}
              idx={i}
              spriteImages={spriteImages}
              selected={currentParallax === i}
              onSelect={() => onUpdate({ parallax: i })}
            />
          ))}
        </div>
      </div>

      {/* Other map properties */}
      <div className="flex flex-wrap gap-4 items-end">
        <div className="flex-1 min-w-[180px]">
          <div className={lbl}>Description (max 127)</div>
          <input type="text" maxLength={127} value={fields.description}
            onChange={e => setFields(f => ({ ...f, description: e.target.value }))} className={inp} />
        </div>
        <div className="w-28">
          <div className={lbl}>Ambience (-128..127)</div>
          <input type="number" min={-128} max={127} value={fields.ambience}
            onChange={e => setFields(f => ({ ...f, ambience: e.target.value }))} className={inp} />
        </div>
        <div className="w-24">
          <div className={lbl}>Max Players</div>
          <input type="number" min={1} max={8} value={fields.maxplayers}
            onChange={e => setFields(f => ({ ...f, maxplayers: e.target.value }))} className={inp} />
        </div>
        <div className="w-24">
          <div className={lbl}>Max Teams</div>
          <input type="number" min={1} max={4} value={fields.maxteams}
            onChange={e => setFields(f => ({ ...f, maxteams: e.target.value }))} className={inp} />
        </div>
        <div className="flex gap-2 flex-shrink-0">
          <button onClick={apply}
            className="px-3 py-1 text-xs font-mono border border-game-primary text-game-primary rounded hover:bg-game-dark transition-colors">
            APPLY
          </button>
          <button onClick={onClose}
            className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary transition-colors">
            ✕
          </button>
        </div>
      </div>
    </div>
  );
}
