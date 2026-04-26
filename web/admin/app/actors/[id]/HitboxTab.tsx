'use client';
/**
 * C5: Per-frame hitbox editor
 * Click+drag on the preview canvas to draw/edit x1,y1,x2,y2 AABB boxes per frame.
 * Reads/writes into def.sequences[name].frames[i].hurtbox
 */
import { useEffect, useRef, useState, useCallback } from 'react';
import { type ActorDef } from '../../../lib/api';

// Hurtbox stored as [x1, y1, x2, y2] array (matching C++ loader format)
type Hurtbox = [number, number, number, number];

interface FrameDef {
  bank: number;
  index: number;
  duration: number;
  hurtbox?: Hurtbox;
}
interface AnimSequence {
  loop: boolean;
  frames: FrameDef[];
}
type Sequences = Record<string, AnimSequence>;

function hbToObj(hb: Hurtbox) { return { x1: hb[0], y1: hb[1], x2: hb[2], y2: hb[3] }; }
function objToHb(o: { x1: number; y1: number; x2: number; y2: number }): Hurtbox {
  return [o.x1, o.y1, o.x2, o.y2];
}

function getSequences(def: ActorDef): Sequences {
  return (def.sequences as Sequences) ?? {};
}

const SCALE = 4;
const CANVAS_W = 256;
const CANVAS_H = 256;
const ORIGIN_X = CANVAS_W / 2;
const ORIGIN_Y = CANVAS_H * 0.75; // foot anchor

function drawFrame(
  ctx: CanvasRenderingContext2D,
  img: HTMLImageElement | null,
  hurtbox: Hurtbox | undefined,
  dragging: { x1: number; y1: number; x2: number; y2: number } | null
) {
  ctx.clearRect(0, 0, CANVAS_W, CANVAS_H);

  // Dark grid background
  ctx.fillStyle = '#111';
  ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);
  ctx.strokeStyle = '#222';
  for (let x = 0; x < CANVAS_W; x += 16) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, CANVAS_H); ctx.stroke(); }
  for (let y = 0; y < CANVAS_H; y += 16) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(CANVAS_W, y); ctx.stroke(); }

  // Origin crosshair
  ctx.strokeStyle = '#334';
  ctx.beginPath(); ctx.moveTo(ORIGIN_X, 0); ctx.lineTo(ORIGIN_X, CANVAS_H); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(0, ORIGIN_Y); ctx.lineTo(CANVAS_W, ORIGIN_Y); ctx.stroke();

  if (img) {
    (ctx as CanvasRenderingContext2D & { imageSmoothingEnabled: boolean }).imageSmoothingEnabled = false;
    const dx = ORIGIN_X - img.width * SCALE / 2;
    const dy = ORIGIN_Y - img.height * SCALE;
    ctx.drawImage(img, dx, dy, img.width * SCALE, img.height * SCALE);
  }

  // Draw dragging box or saved hurtbox
  const boxObj = dragging ?? (hurtbox ? hbToObj(hurtbox) : null);
  if (boxObj) {
    const cx1 = ORIGIN_X + boxObj.x1 * SCALE;
    const cy1 = ORIGIN_Y + boxObj.y1 * SCALE;
    const cx2 = ORIGIN_X + boxObj.x2 * SCALE;
    const cy2 = ORIGIN_Y + boxObj.y2 * SCALE;
    ctx.strokeStyle = dragging ? '#ff0' : '#0f0';
    ctx.lineWidth = 1;
    ctx.strokeRect(Math.min(cx1, cx2), Math.min(cy1, cy2), Math.abs(cx2 - cx1), Math.abs(cy2 - cy1));
    ctx.fillStyle = dragging ? 'rgba(255,255,0,0.08)' : 'rgba(0,255,0,0.08)';
    ctx.fillRect(Math.min(cx1, cx2), Math.min(cy1, cy2), Math.abs(cx2 - cx1), Math.abs(cy2 - cy1));
  }
}

export default function HitboxTab({
  actorId, def, onChange,
}: {
  actorId: string;
  def: ActorDef;
  onChange: (patch: Partial<ActorDef>) => void;
}) {
  const sequences = getSequences(def);
  const seqNames = Object.keys(sequences);
  const [selectedSeq, setSelectedSeq] = useState<string>(seqNames[0] ?? '');
  const [frameIdx, setFrameIdx]       = useState(0);

  const canvasRef = useRef<HTMLCanvasElement>(null);
  const imgRef    = useRef<HTMLImageElement | null>(null);
  const dragRef   = useRef<{ startX: number; startY: number; cur: { x1: number; y1: number; x2: number; y2: number } | null }>({ startX: 0, startY: 0, cur: null });

  const seq = sequences[selectedSeq];
  const frame: FrameDef | undefined = seq?.frames[frameIdx];

  // Load sprite image when frame changes
  useEffect(() => {
    imgRef.current = null;
    if (!frame) return;
    const img = new Image();
    const token = localStorage.getItem('zs_token') ?? '';
    img.src = `/api/sprites/${frame.bank}/${frame.index}?_t=${token.slice(-8)}`;
    img.onload = () => {
      imgRef.current = img;
      redraw(null);
    };
  }, [frame?.bank, frame?.index]);

  function redraw(dragging: { x1: number; y1: number; x2: number; y2: number } | null) {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d')!;
    drawFrame(ctx, imgRef.current, frame?.hurtbox, dragging);
  }

  useEffect(() => { redraw(null); }, [frame?.hurtbox, selectedSeq, frameIdx]);

  function canvasToGame(cx: number, cy: number) {
    return {
      x: Math.round((cx - ORIGIN_X) / SCALE),
      y: Math.round((cy - ORIGIN_Y) / SCALE),
    };
  }

  function onMouseDown(e: React.MouseEvent<HTMLCanvasElement>) {
    const rect = canvasRef.current!.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    dragRef.current = { startX: sx, startY: sy, cur: null };
  }

  function onMouseMove(e: React.MouseEvent<HTMLCanvasElement>) {
    if (!dragRef.current.cur && e.buttons === 0) return;
    if (e.buttons !== 1) return;
    const rect = canvasRef.current!.getBoundingClientRect();
    const ex = e.clientX - rect.left;
    const ey = e.clientY - rect.top;
    const p1 = canvasToGame(dragRef.current.startX, dragRef.current.startY);
    const p2 = canvasToGame(ex, ey);
    const box = { x1: p1.x, y1: p1.y, x2: p2.x, y2: p2.y };
    dragRef.current.cur = box;
    redraw(box);
  }

  function onMouseUp(_e: React.MouseEvent<HTMLCanvasElement>) {
    const box = dragRef.current.cur;
    if (!box || !seq || !frame) return;
    const finalHb: Hurtbox = [
      Math.min(box.x1, box.x2),
      Math.min(box.y1, box.y2),
      Math.max(box.x1, box.x2),
      Math.max(box.y1, box.y2),
    ];
    const nextFrames = seq.frames.map((f, i) =>
      i === frameIdx ? { ...f, hurtbox: finalHb } : f
    );
    onChange({
      sequences: {
        ...sequences,
        [selectedSeq]: { ...seq, frames: nextFrames },
      },
    });
    dragRef.current.cur = null;
  }

  function clearHurtbox() {
    if (!seq || !frame) return;
    const nextFrames = seq.frames.map((f, i) =>
      i === frameIdx ? { ...f, hurtbox: undefined } : f
    );
    onChange({ sequences: { ...sequences, [selectedSeq]: { ...seq, frames: nextFrames } } });
  }

  return (
    <div className="flex h-full">
      {/* Sequence + frame selector */}
      <div className="w-52 border-r border-game-border flex flex-col">
        <div className="px-4 py-3 text-xs text-game-textDim tracking-widest border-b border-game-border">SEQUENCE</div>
        <div className="overflow-y-auto max-h-40">
          {seqNames.map(name => (
            <button
              key={name}
              onClick={() => { setSelectedSeq(name); setFrameIdx(0); }}
              className={`w-full text-left px-4 py-2 text-xs font-mono ${name === selectedSeq ? 'bg-game-primary/10 text-game-primary' : 'text-game-text hover:bg-game-bgCard'}`}
            >
              {name}
            </button>
          ))}
        </div>
        <div className="px-4 py-3 text-xs text-game-textDim tracking-widest border-t border-b border-game-border">FRAMES</div>
        <div className="flex-1 overflow-y-auto">
          {(seq?.frames ?? []).map((f, i) => (
            <button
              key={i}
              onClick={() => setFrameIdx(i)}
              className={`w-full text-left px-4 py-1.5 text-xs font-mono flex items-center justify-between ${i === frameIdx ? 'bg-game-primary/10 text-game-primary' : 'text-game-text hover:bg-game-bgCard'}`}
            >
              <span>#{i} b{f.bank}:{f.index}</span>
              {f.hurtbox && <span className="text-game-primary text-[10px]">✓</span>}
            </button>
          ))}
        </div>
      </div>

      {/* Canvas */}
      <div className="flex-1 flex gap-6 p-6">
        <div className="flex flex-col gap-2">
          <div className="text-xs text-game-textDim tracking-widest mb-1">
            DRAW HURTBOX — click+drag on canvas
          </div>
          <canvas
            ref={canvasRef}
            width={CANVAS_W}
            height={CANVAS_H}
            className="border border-game-border cursor-crosshair"
            style={{ imageRendering: 'pixelated', width: CANVAS_W, height: CANVAS_H }}
            onMouseDown={onMouseDown}
            onMouseMove={onMouseMove}
            onMouseUp={onMouseUp}
          />
          <button
            onClick={clearHurtbox}
            className="text-xs text-game-danger border border-game-danger px-3 py-1 hover:bg-game-danger/10 self-start"
          >
            CLEAR HURTBOX
          </button>
        </div>

        {/* Hurtbox values */}
        <div className="flex flex-col gap-4">
          <div className="text-xs text-game-textDim tracking-widest">HURTBOX VALUES</div>
          {frame?.hurtbox ? (
            <div className="bg-game-bgCard border border-game-border p-4 font-mono text-sm space-y-2">
              {(['x1','y1','x2','y2'] as const).map((k, ki) => (
                <div key={k} className="flex items-center gap-3">
                  <span className="text-game-textDim w-4">{k}</span>
                  <input
                    type="number"
                    className="w-20 bg-game-bg border border-game-border px-2 py-1 text-xs text-center"
                    value={frame.hurtbox![ki]}
                    onChange={e => {
                      if (!seq) return;
                      const nextFrames = seq.frames.map((f, i) => {
                        if (i !== frameIdx) return f;
                        const hb: Hurtbox = [...(f.hurtbox ?? [0,0,0,0])] as Hurtbox;
                        hb[ki] = +e.target.value;
                        return { ...f, hurtbox: hb };
                      });
                      onChange({ sequences: { ...sequences, [selectedSeq]: { ...seq, frames: nextFrames } } });
                    }}
                  />
                </div>
              ))}
              <div className="text-xs text-game-textDim pt-2 border-t border-game-border">
                Relative to anchor (x=center, y=foot)
              </div>
            </div>
          ) : (
            <div className="text-game-textDim text-xs">No hurtbox on this frame</div>
          )}

          <div className="mt-4 text-xs text-game-textDim space-y-1">
            <p>• Draw by clicking and dragging</p>
            <p>• Coordinates relative to player anchor</p>
            <p>• x=0 is center, y=0 is foot (y1 negative = above foot)</p>
            <p>• Copied from JSON player.json when importing</p>
          </div>
        </div>
      </div>
    </div>
  );
}
