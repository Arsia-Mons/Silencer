'use client';
/**
 * C3: Animation sequence builder + timeline
 * C4: Live preview canvas at game speed (60fps rAF loop)
 */
import { useEffect, useRef, useState, useCallback, useMemo } from 'react';
import { getSpriteFrames, type FrameMeta, type ActorDef } from '../../../lib/api';

function useSounds(): string[] {
  const [sounds, setSounds] = useState<string[]>([]);
  useEffect(() => {
    fetch('/api/sounds').then(r => r.json()).then(setSounds).catch(() => {});
  }, []);
  return sounds;
}

interface FrameDef {
  bank: number;
  index: number;
  duration: number; // ticks at 60fps
  sound?: string;
  soundVolume?: number;
}

interface AnimSequence {
  loop: boolean;
  frames: FrameDef[];
}

type Sequences = Record<string, AnimSequence>;

function getSequences(def: ActorDef): Sequences {
  return (def.sequences as Sequences) ?? {};
}

/** Draw a sprite frame onto a canvas via <img> proxy URL. */
function useImageCache() {
  const cache = useRef<Map<string, HTMLImageElement>>(new Map());
  return useCallback((bank: number, frame: number): Promise<HTMLImageElement> => {
    const key = `${bank}:${frame}`;
    if (cache.current.has(key)) return Promise.resolve(cache.current.get(key)!);
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => { cache.current.set(key, img); resolve(img); };
      img.onerror = reject;
      const token = localStorage.getItem('zs_token') ?? '';
      img.src = `/api/sprites/${bank}/${frame}?_t=${token.slice(-8)}`;
    });
  }, []);
}

const CANVAS_PAD = 24; // px padding around the scaled sprite
const CANVAS_MIN = 120;

/** Live preview canvas that plays a sequence at 60fps, auto-sized to the sprite. */
function PreviewCanvas({ sequence, scale }: { sequence: AnimSequence | null; scale: number }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const rafRef    = useRef<number>(0);
  const loadImg   = useImageCache();
  const stateRef  = useRef({ tick: 0, frameIdx: 0 });
  const [canvasSize, setCanvasSize] = useState({ w: CANVAS_MIN, h: CANVAS_MIN });

  // Measure max natural sprite size across all frames in the sequence
  useEffect(() => {
    if (!sequence || sequence.frames.length === 0) {
      setCanvasSize({ w: CANVAS_MIN, h: CANVAS_MIN });
      return;
    }
    // Deduplicate bank:index pairs
    const unique = [...new Map(sequence.frames.map(f => [`${f.bank}:${f.index}`, f])).values()];
    Promise.all(unique.map(f => loadImg(f.bank, f.index).catch(() => null)))
      .then(imgs => {
        let maxW = 0, maxH = 0;
        for (const img of imgs) {
          if (!img) continue;
          if (img.naturalWidth  > maxW) maxW = img.naturalWidth;
          if (img.naturalHeight > maxH) maxH = img.naturalHeight;
        }
        setCanvasSize({
          w: Math.max(maxW * scale + CANVAS_PAD * 2, CANVAS_MIN),
          h: Math.max(maxH * scale + CANVAS_PAD * 2, CANVAS_MIN),
        });
      });
  }, [sequence, loadImg, scale]);

  useEffect(() => {
    if (!sequence || sequence.frames.length === 0) {
      const ctx = canvasRef.current?.getContext('2d');
      if (ctx) ctx.clearRect(0, 0, canvasSize.w, canvasSize.h);
      return;
    }

    let lastTs = 0;
    const TICK_MS = 1000 / 60;

    async function loop(ts: number) {
      const dt = ts - lastTs;
      if (dt >= TICK_MS) {
        lastTs = ts - (dt % TICK_MS);
        const s = stateRef.current;
        const f = sequence!.frames[s.frameIdx];
        if (!f) { s.frameIdx = 0; s.tick = 0; }
        else {
          s.tick++;
          if (s.tick >= f.duration) {
            s.tick = 0;
            s.frameIdx++;
            if (s.frameIdx >= sequence!.frames.length) {
              s.frameIdx = sequence!.loop ? 0 : sequence!.frames.length - 1;
            }
          }
          try {
            const img = await loadImg(f.bank, f.index);
            const canvas = canvasRef.current;
            if (canvas) {
              const ctx = canvas.getContext('2d')!;
              // Grid background (matches hitbox tab)
              ctx.fillStyle = '#111';
              ctx.fillRect(0, 0, canvas.width, canvas.height);
              ctx.strokeStyle = '#222';
              ctx.lineWidth = 1;
              for (let gx = 0; gx < canvas.width; gx += 16) { ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, canvas.height); ctx.stroke(); }
              for (let gy = 0; gy < canvas.height; gy += 16) { ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(canvas.width, gy); ctx.stroke(); }
              const x = Math.floor((canvas.width  - img.width  * scale) / 2);
              const y = Math.floor((canvas.height - img.height * scale) / 2);
              (ctx as CanvasRenderingContext2D & { imageSmoothingEnabled: boolean }).imageSmoothingEnabled = false;
              ctx.drawImage(img, x, y, img.width * scale, img.height * scale);
            }
          } catch { /* skip frame */ }
        }
      }
      rafRef.current = requestAnimationFrame(loop);
    }

    stateRef.current = { tick: 0, frameIdx: 0 };
    rafRef.current = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(rafRef.current);
  }, [sequence, loadImg, canvasSize, scale]);

  return (
    <canvas
      ref={canvasRef}
      width={canvasSize.w}
      height={canvasSize.h}
      className="border border-game-border"
      style={{ imageRendering: 'pixelated' }}
    />
  );
}

/** Inline sound picker — shows current value; click to open a searchable dropdown. */
function SoundPicker({ value, volume, sounds, onChange }: {
  value: string | undefined;
  volume: number | undefined;
  sounds: string[];
  onChange: (v: string | undefined) => void;
}) {
  const [open, setOpen] = useState(false);
  const [filter, setFilter] = useState('');
  const ref = useRef<HTMLDivElement>(null);
  const filtered = useMemo(
    () => sounds.filter(s => s.toLowerCase().includes(filter.toLowerCase())),
    [sounds, filter]
  );
  useEffect(() => {
    if (!open) return;
    function onDown(e: MouseEvent) {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false);
    }
    document.addEventListener('mousedown', onDown);
    return () => document.removeEventListener('mousedown', onDown);
  }, [open]);
  return (
    <div className="relative flex items-center gap-1" ref={ref}>
      <button
        type="button"
        className="w-32 bg-game-bg border border-game-border px-2 py-1 text-xs font-mono text-left truncate hover:border-game-text"
        onClick={() => { setOpen(o => !o); setFilter(''); }}
        title={value}
      >
        {value ?? <span className="text-game-textDim">— none —</span>}
      </button>
      {value && (
        <button
          type="button"
          title="Preview sound"
          className="text-game-textDim hover:text-game-primary text-xs px-1"
          onClick={() => { const a = new Audio(`/sounds/${value}`); a.volume = Math.min(1, (volume ?? 128) / 128); a.play().catch(() => {}); }}
        >▶</button>
      )}
      {open && (
        <div className="absolute z-50 top-full left-0 mt-0.5 w-48 bg-[#050a05] border border-game-border shadow-lg flex flex-col">
          <input
            autoFocus
            type="text"
            placeholder="filter..."
            className="bg-game-bg border-b border-game-border px-2 py-1 text-xs font-mono"
            value={filter}
            onChange={e => setFilter(e.target.value)}
          />
          <div className="overflow-y-auto max-h-48">
            <button
              type="button"
              className="w-full text-left px-2 py-1 text-xs text-game-textDim hover:bg-game-border/30"
              onClick={() => { onChange(undefined); setOpen(false); }}
            >— none —</button>
            {filtered.map(s => (
              <div key={s} className="flex items-center">
                <button
                  type="button"
                  className={`flex-1 text-left px-2 py-1 text-xs font-mono hover:bg-game-border/30 ${s === value ? 'text-game-primary' : ''}`}
                  onClick={() => { onChange(s); setOpen(false); }}
                >{s}</button>
                <button
                  type="button"
                  title="Preview"
                  className="px-2 text-game-textDim hover:text-game-primary text-xs"
                  onClick={() => { const a = new Audio(`/sounds/${s}`); a.volume = Math.min(1, (volume ?? 128) / 128); a.play().catch(() => {}); }}
                >▶</button>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

/** Frame row in the sequence editor. */
function FrameRow({
  f, idx, sounds, onChange, onDelete, onMoveUp, onMoveDown,
}: {
  f: FrameDef;
  idx: number;
  sounds: string[];
  onChange: (f: FrameDef) => void;
  onDelete: () => void;
  onMoveUp: () => void;
  onMoveDown: () => void;
}) {
  return (
    <div className="flex items-center gap-2 py-1 border-b border-game-border/50">
      <span className="text-game-textDim text-xs w-5 text-right">{idx}</span>
      <input
        type="number" min={0} max={228}
        className="w-16 bg-game-bg border border-game-border px-2 py-1 text-xs font-mono text-center"
        value={f.bank}
        onChange={e => onChange({ ...f, bank: +e.target.value })}
      />
      <input
        type="number" min={0} max={239}
        className="w-16 bg-game-bg border border-game-border px-2 py-1 text-xs font-mono text-center"
        value={f.index}
        onChange={e => onChange({ ...f, index: +e.target.value })}
      />
      <input
        type="number" min={1} max={300}
        className="w-16 bg-game-bg border border-game-border px-2 py-1 text-xs font-mono text-center"
        value={f.duration}
        onChange={e => onChange({ ...f, duration: +e.target.value })}
      />
      <img
        src={`/api/sprites/${f.bank}/${f.index}`}
        alt=""
        className="w-8 h-8 object-contain border border-game-border bg-black"
        style={{ imageRendering: 'pixelated' }}
      />
      <SoundPicker
        value={f.sound}
        volume={f.soundVolume}
        sounds={sounds}
        onChange={v => onChange({ ...f, sound: v })}
      />
      <input
        type="number" min={0} max={128}
        title="Volume (0-128)"
        className="w-14 bg-game-bg border border-game-border px-2 py-1 text-xs font-mono text-center"
        value={f.soundVolume ?? ''}
        placeholder="vol"
        onChange={e => {
          const v = e.target.value === '' ? undefined : +e.target.value;
          onChange({ ...f, soundVolume: v });
        }}
      />
      <div className="flex gap-1 ml-auto">
        <button onClick={onMoveUp} className="text-game-textDim hover:text-game-text text-xs px-1">↑</button>
        <button onClick={onMoveDown} className="text-game-textDim hover:text-game-text text-xs px-1">↓</button>
        <button onClick={onDelete} className="text-game-danger text-xs px-1">✕</button>
      </div>
    </div>
  );
}

export default function AnimationTab({
  actorId, def, onChange,
}: {
  actorId: string;
  def: ActorDef;
  onChange: (patch: Partial<ActorDef>) => void;
}) {
  const sounds = useSounds();
  const sequences = getSequences(def);
  const [selectedSeq, setSelectedSeq] = useState<string | null>(
    Object.keys(sequences)[0] ?? null
  );
  const [newSeqName, setNewSeqName] = useState('');
  const [scale, setScale] = useState(1);

  const seq = selectedSeq ? sequences[selectedSeq] : null;

  function updateSequences(next: Sequences) {
    onChange({ sequences: next });
  }

  function addSequence() {
    const name = newSeqName.trim().toUpperCase();
    if (!name || sequences[name]) return;
    updateSequences({ ...sequences, [name]: { loop: true, frames: [] } });
    setSelectedSeq(name);
    setNewSeqName('');
  }

  function deleteSequence(name: string) {
    if (!confirm(`Delete sequence "${name}"?`)) return;
    const next = { ...sequences };
    delete next[name];
    updateSequences(next);
    setSelectedSeq(Object.keys(next)[0] ?? null);
  }

  function updateSeq(patch: Partial<AnimSequence>) {
    if (!selectedSeq) return;
    updateSequences({ ...sequences, [selectedSeq]: { ...seq!, ...patch } });
  }

  function addFrame() {
    if (!seq) return;
    updateSeq({ frames: [...seq.frames, { bank: 9, index: 0, duration: 8 }] });
  }

  function updateFrame(i: number, f: FrameDef) {
    if (!seq) return;
    const next = [...seq.frames];
    next[i] = f;
    updateSeq({ frames: next });
  }

  function deleteFrame(i: number) {
    if (!seq) return;
    updateSeq({ frames: seq.frames.filter((_, idx) => idx !== i) });
  }

  function moveFrame(i: number, dir: -1 | 1) {
    if (!seq) return;
    const next = [...seq.frames];
    const j = i + dir;
    if (j < 0 || j >= next.length) return;
    [next[i], next[j]] = [next[j], next[i]];
    updateSeq({ frames: next });
  }

  // Timeline: each tick = 1px width, capped at 4px per tick for readability
  const TICK_W = 4;
  const timelineFrames = seq?.frames ?? [];
  const totalTicks = timelineFrames.reduce((s, f) => s + f.duration, 0);

  return (
    <div className="flex h-full">
      {/* Sequence list sidebar */}
      <div className="w-52 border-r border-game-border flex flex-col">
        <div className="px-4 py-3 text-xs text-game-textDim tracking-widest border-b border-game-border">SEQUENCES</div>
        <div className="flex-1 overflow-y-auto">
          {Object.keys(sequences).map(name => (
            <div
              key={name}
              className={`flex items-center justify-between px-4 py-2 cursor-pointer group ${
                name === selectedSeq ? 'bg-game-primary/10 text-game-primary' : 'hover:bg-game-bgCard text-game-text'
              }`}
              onClick={() => setSelectedSeq(name)}
            >
              <span className="text-xs font-mono">{name}</span>
              <button
                onClick={e => { e.stopPropagation(); deleteSequence(name); }}
                className="text-game-danger opacity-0 group-hover:opacity-100 text-xs"
              >✕</button>
            </div>
          ))}
        </div>
        <div className="p-3 border-t border-game-border flex gap-2">
          <input
            className="flex-1 bg-game-bg border border-game-border px-2 py-1 text-xs uppercase"
            placeholder="SEQ_NAME"
            value={newSeqName}
            onChange={e => setNewSeqName(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && addSequence()}
          />
          <button onClick={addSequence} className="text-game-primary text-xs px-2 border border-game-primary hover:bg-game-primary/10">+</button>
        </div>
      </div>

      {/* Main editor area */}
      <div className="flex-1 flex flex-col min-w-0 p-4 gap-4">
        {!seq ? (
          <div className="text-game-textDim text-sm">Select or create a sequence.</div>
        ) : (
          <>
            {/* Sequence settings */}
            <div className="flex items-center gap-6">
              <label className="flex items-center gap-2 text-xs text-game-textDim cursor-pointer">
                <input
                  type="checkbox"
                  checked={seq.loop}
                  onChange={e => updateSeq({ loop: e.target.checked })}
                  className="accent-game-primary"
                />
                LOOP
              </label>
              <span className="text-xs text-game-textDim">{timelineFrames.length} frames · {totalTicks} ticks ({(totalTicks / 60).toFixed(2)}s)</span>
            </div>

            <div className="flex gap-4 flex-1 min-h-0">
              {/* Frame list */}
              <div className="flex-1 overflow-y-auto">
                <div className="flex text-xs text-game-textDim gap-2 mb-1 px-2">
                  <span className="w-5" />
                  <span className="w-16 text-center">BANK</span>
                  <span className="w-16 text-center">FRAME</span>
                  <span className="w-16 text-center">TICKS</span>
                  <span className="w-8" />
                  <span className="w-28 text-center">SOUND</span>
                  <span className="w-14 text-center">VOL</span>
                </div>
                {timelineFrames.map((f, i) => (
                  <FrameRow
                    key={i}
                    f={f}
                    idx={i}
                    sounds={sounds}
                    onChange={nf => updateFrame(i, nf)}
                    onDelete={() => deleteFrame(i)}
                    onMoveUp={() => moveFrame(i, -1)}
                    onMoveDown={() => moveFrame(i, 1)}
                  />
                ))}
                <button
                  onClick={addFrame}
                  className="mt-2 text-xs text-game-primary border border-game-primary px-3 py-1 hover:bg-game-primary/10"
                >
                  + ADD FRAME
                </button>
              </div>

              {/* Preview canvas */}
              <div className="flex flex-col items-center gap-2">
                <div className="flex items-center gap-2">
                  <span className="text-xs text-game-textDim tracking-widest">PREVIEW</span>
                  {[1, 2, 3, 4].map(s => (
                    <button
                      key={s}
                      type="button"
                      onClick={() => setScale(s)}
                      className={`text-xs px-1.5 py-0.5 border ${scale === s ? 'border-game-primary text-game-primary' : 'border-game-border text-game-textDim hover:border-game-text'}`}
                    >{s}×</button>
                  ))}
                </div>
                <PreviewCanvas sequence={seq} scale={scale} />
              </div>
            </div>

            {/* Timeline bar */}
            {timelineFrames.length > 0 && (
              <div className="border border-game-border p-2">
                <div className="text-xs text-game-textDim mb-1">TIMELINE ({totalTicks} ticks)</div>
                <div className="flex h-6 overflow-x-auto">
                  {timelineFrames.map((f, i) => (
                    <div
                      key={i}
                      title={`Frame ${i}: bank${f.bank}:${f.index} — ${f.duration} ticks`}
                      className="h-full border-r border-game-bg flex items-center justify-center text-[9px] text-game-textDim overflow-hidden shrink-0"
                      style={{
                        width: Math.max(TICK_W * f.duration, 20),
                        backgroundColor: `hsl(${(i * 47) % 360}, 30%, 22%)`,
                      }}
                    >
                      {f.duration > 3 ? i : ''}
                    </div>
                  ))}
                </div>
              </div>
            )}
          </>
        )}
      </div>
    </div>
  );
}
