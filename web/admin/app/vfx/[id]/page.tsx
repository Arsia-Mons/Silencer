'use client';
import { useEffect, useRef, useState, useCallback } from 'react';
import Link from 'next/link';
import { useParams, useRouter } from 'next/navigation';
import { useAuth } from '../../../lib/auth';
import Sidebar from '../../../components/Sidebar';
import * as vfxStore from '../../../lib/vfx-store';
import type { EffectDef } from '../../../lib/vfx-store';

const INPUT  = 'bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 w-full focus:border-[#00a328] outline-none';
const LABEL  = 'text-[9px] font-mono text-[#4a7a4a] tracking-widest uppercase';

// ── Live sprite animation preview ─────────────────────────────────────────────
function AnimPreview({ bank, frames, fps, loop, pingPong }: {
  bank: number; frames: number[]; fps: number; loop: boolean; pingPong: boolean;
}) {
  const [frameIdx, setFrameIdx] = useState(0);
  const [dir, setDir] = useState(1);
  const [playing, setPlaying] = useState(true);

  // Reset to frame 0 when animation params change
  useEffect(() => { setFrameIdx(0); setDir(1); }, [bank, JSON.stringify(frames), fps, loop, pingPong]); // eslint-disable-line react-hooks/exhaustive-deps

  useEffect(() => {
    if (!playing || frames.length === 0 || fps <= 0) return;
    const ms = 1000 / fps;
    const id = setInterval(() => {
      setFrameIdx(prev => {
        const next = prev + dir;
        if (pingPong) {
          if (next >= frames.length) { setDir(-1); return prev - 1 < 0 ? 0 : prev - 1; }
          if (next < 0) { setDir(1); return 1 < frames.length ? 1 : 0; }
          return next;
        }
        if (next >= frames.length) return loop ? 0 : prev;
        return next;
      });
    }, ms);
    return () => clearInterval(id);
  }, [playing, frames.length, fps, loop, pingPong, dir]);

  if (frames.length === 0) {
    return (
      <div className="w-32 h-32 border border-[#1a2e1a] bg-[#050c05] flex items-center justify-center">
        <span className="text-[10px] font-mono text-[#2a4a2a]">no frames</span>
      </div>
    );
  }

  const currentFrame = frames[Math.min(frameIdx, frames.length - 1)];

  return (
    <div className="flex flex-col items-start gap-2">
      {/* eslint-disable-next-line @next/next/no-img-element */}
      <img
        src={`/api/sprites/${bank}/${currentFrame}`}
        alt={`bank ${bank} frame ${currentFrame}`}
        className="border border-[#1a2e1a] bg-[#050c05]"
        style={{ imageRendering: 'pixelated', minWidth: 64, minHeight: 64, maxWidth: 192, maxHeight: 192, objectFit: 'contain' }}
      />
      <div className="flex items-center gap-2">
        <button onClick={() => setPlaying(p => !p)}
          className="text-[10px] font-mono px-2 py-0.5 border border-[#1a2e1a] text-[#4a7a4a] hover:text-[#00a328] hover:border-[#00a328] transition-colors">
          {playing ? '⏸ PAUSE' : '▶ PLAY'}
        </button>
        <span className="text-[9px] font-mono text-[#2a4a2a]">
          frame {frameIdx + 1}/{frames.length} · idx {currentFrame}
        </span>
      </div>
    </div>
  );
}

// ── Bank frame browser ─────────────────────────────────────────────────────────
function BankBrowser({ bank, selectedFrames, onAddFrame }: {
  bank: number;
  selectedFrames: number[];
  onAddFrame: (f: number) => void;
}) {
  const [frameCount, setFrameCount] = useState(16);
  const [loading, setLoading] = useState(false);

  // Probe how many frames exist by trying to load until 404
  useEffect(() => {
    let cancelled = false;
    setLoading(true);
    setFrameCount(0);
    async function probe() {
      let count = 0;
      for (let i = 0; i < 64; i++) {
        try {
          const res = await fetch(`/api/sprites/${bank}/${i}`, { method: 'HEAD' });
          if (!res.ok) break;
          if (cancelled) return;
          count = i + 1;
        } catch { break; }
      }
      if (!cancelled) { setFrameCount(count || 8); setLoading(false); }
    }
    probe();
    return () => { cancelled = true; };
  }, [bank]);

  return (
    <div className="flex flex-col gap-2">
      <div className="flex items-center gap-2">
        <span className="text-[10px] font-mono text-[#4a7a4a]">
          BANK {bank} · {loading ? 'probing…' : `${frameCount} frames`}
        </span>
        <span className="text-[9px] font-mono text-[#2a4a2a]">click to add to sequence</span>
      </div>
      <div className="flex flex-wrap gap-1">
        {Array.from({ length: frameCount }, (_, i) => (
          <button key={i} onClick={() => onAddFrame(i)}
            title={`Frame ${i}`}
            className="relative border border-[#1a2e1a] hover:border-[#00a328] bg-[#050c05] transition-colors flex-shrink-0">
            {/* eslint-disable-next-line @next/next/no-img-element */}
            <img
              src={`/api/sprites/${bank}/${i}`}
              alt={`f${i}`}
              width={48} height={48}
              style={{ imageRendering: 'pixelated', display: 'block', objectFit: 'contain' }}
            />
            <span className="absolute bottom-0 right-0 text-[8px] font-mono text-[#4a7a4a] bg-[#080f08]/80 px-0.5">{i}</span>
          </button>
        ))}
      </div>
    </div>
  );
}

// ── Main page ─────────────────────────────────────────────────────────────────
export default function AnimDetailPage() {
  useAuth();
  const { id } = useParams() as { id: string };
  const router = useRouter();
  const [effects, setEffects] = useState<EffectDef[]>([]);
  const [anim, setAnim] = useState<EffectDef | null>(null);
  const [dirty, setDirty] = useState(false);
  const [search, setSearch] = useState('');
  const [folderName, setFolderName] = useState<string | null>(null);
  const selectedRef = useRef<HTMLAnchorElement | null>(null);

  useEffect(() => {
    if (!vfxStore.isLoaded()) { router.replace('/vfx'); return; }
    setFolderName(vfxStore.getFolderName());
    setEffects(vfxStore.listAll());
    const found = vfxStore.getById(id);
    setAnim(found ? { ...found } : null);
    setDirty(false);
  }, [id, router]);

  useEffect(() => { selectedRef.current?.scrollIntoView({ block: 'nearest' }); }, [id]);

  useEffect(() => {
    function onKey(e: KeyboardEvent) {
      const tag = (e.target as HTMLElement).tagName.toLowerCase();
      if (['input', 'textarea', 'select'].includes(tag)) return;
      if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') return;
      e.preventDefault();
      const filtered = search
        ? effects.filter(a => a.id.includes(search) || a.name.toLowerCase().includes(search.toLowerCase()))
        : effects;
      const idx = filtered.findIndex(a => a.id === id);
      const next = e.key === 'ArrowDown' ? Math.min(idx + 1, filtered.length - 1) : Math.max(idx - 1, 0);
      if (filtered[next] && filtered[next].id !== id) router.push(`/vfx/${filtered[next].id}`, { scroll: false } as never);
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [id, effects, search, router]);

  const patch = useCallback((partial: Partial<EffectDef>) => {
    setAnim(prev => prev ? { ...prev, ...partial } : prev);
    setDirty(true);
  }, []);

  function save() {
    if (!anim) return;
    vfxStore.setEffect(anim);
    setEffects(vfxStore.listAll());
    setDirty(false);
  }

  function handleDownload() {
    save();
    vfxStore.downloadJson(folderName ? `${folderName}.json` : 'effects.json');
  }

  function addEffect() {
    const newId = `anim-${Date.now().toString(36)}`;
    const na: EffectDef = { ...vfxStore.DEFAULT_EFFECT, id: newId, name: 'New Effect' };
    vfxStore.addEffect(na);
    setEffects(vfxStore.listAll());
    router.push(`/vfx/${newId}`, { scroll: false } as never);
  }

  function duplicateAnimation() {
    if (!anim) return;
    save();
    const newId = `${anim.id}-copy`;
    const na: EffectDef = { ...anim, id: newId, name: `${anim.name} (copy)` };
    vfxStore.addEffect(na);
    setEffects(vfxStore.listAll());
    router.push(`/vfx/${newId}`, { scroll: false } as never);
  }

  function deleteAnimation() {
    if (!anim || !confirm(`Delete "${anim.name}"?`)) return;
    vfxStore.removeEffect(anim.id);
    const remaining = vfxStore.listAll();
    setEffects(remaining);
    if (remaining.length > 0) router.push(`/vfx/${remaining[0].id}`, { scroll: false } as never);
    else router.push('/vfx');
  }

  function addFrame(f: number) {
    if (!anim) return;
    patch({ frames: [...anim.frames, f] });
  }

  function removeFrame(idx: number) {
    if (!anim) return;
    patch({ frames: anim.frames.filter((_, i) => i !== idx) });
  }

  function moveFrame(idx: number, dir: -1 | 1) {
    if (!anim) return;
    const frames = [...anim.frames];
    const swap = idx + dir;
    if (swap < 0 || swap >= frames.length) return;
    [frames[idx], frames[swap]] = [frames[swap], frames[idx]];
    patch({ frames });
  }

  function setSequenceFromRange(from: number, to: number) {
    if (!anim) return;
    const frames: number[] = [];
    for (let i = from; i <= to; i++) frames.push(i);
    patch({ frames });
  }

  if (!vfxStore.isLoaded()) return null;

  const filtered = search
    ? effects.filter(a => a.id.includes(search) || a.name.toLowerCase().includes(search.toLowerCase()))
    : effects;

  return (
    <div className="flex min-h-screen bg-[#080f08] text-[#d1fad7]">
      <Sidebar />

      <div className="flex-1 flex flex-col overflow-hidden">
        {/* Header */}
        <div className="border-b border-[#1a2e1a] px-4 py-2 flex items-center gap-3 shrink-0">
          <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">✦ EFFECT EDITOR</span>
          {folderName && <span className="text-[10px] font-mono text-[#2a4a2a]">· {folderName}</span>}
          <div className="ml-auto flex gap-2">
            <button onClick={handleDownload}
              className="text-[10px] font-mono text-[#4a7a4a] hover:text-[#00a328] border border-[#1a2e1a] hover:border-[#00a328] px-2 py-0.5 transition-colors">
              ↓ DOWNLOAD JSON
            </button>
            <button onClick={() => { vfxStore.clear(); router.push('/vfx'); }}
              className="text-[10px] font-mono text-[#4a7a4a] hover:text-red-400 border border-[#1a2e1a] hover:border-red-400 px-2 py-0.5 transition-colors">
              ✕ CLOSE
            </button>
          </div>
        </div>

        <div className="flex flex-1 overflow-hidden">
          {/* ── Left: animation list ── */}
          <div className="w-56 border-r border-[#1a2e1a] flex flex-col shrink-0">
            <div className="px-3 py-2 border-b border-[#1a2e1a] flex gap-1">
              <input type="text" placeholder="filter…" value={search}
                onChange={e => setSearch(e.target.value)}
                className="flex-1 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-[10px] font-mono px-2 py-0.5 focus:border-[#00a328] outline-none" />
              <button onClick={addEffect} title="Add animation"
                className="px-2 text-[#4a7a4a] hover:text-[#00a328] border border-[#1a2e1a] hover:border-[#00a328] text-xs transition-colors">
                +
              </button>
            </div>
            <div className="px-3 py-1 border-b border-[#1a2e1a] shrink-0">
              <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">
                EFFECTS ({filtered.length}{search ? `/${effects.length}` : ''})
              </span>
            </div>
            <div className="flex-1 overflow-y-auto">
              {filtered.map(a => (
                <Link key={a.id} href={`/vfx/${a.id}`} scroll={false}
                  ref={a.id === id ? selectedRef : null}
                  className={`flex flex-col px-3 py-2 border-b border-[#1a2e1a] transition-colors ${
                    a.id === id ? 'bg-[#00a328] text-black' : 'hover:bg-[#0a180a] text-[#d1fad7]'
                  }`}>
                  <span className="text-xs font-mono truncate">{a.name}</span>
                  <span className={`text-[10px] font-mono ${a.id === id ? 'text-black/60' : 'text-[#4a7a4a]'}`}>
                    bank {a.bank} · {a.frames.length}f · {a.fps}fps
                  </span>
                </Link>
              ))}
            </div>
          </div>

          {/* ── Right: editor ── */}
          {anim ? (
            <div className="flex-1 overflow-y-auto p-5 space-y-4">

              {/* Actions */}
              <div className="flex items-center gap-2 flex-wrap">
                <button onClick={handleDownload} disabled={!dirty}
                  className={`px-3 py-1 text-xs font-mono border transition-colors ${dirty
                    ? 'border-[#00a328] text-[#00a328] hover:bg-[#00a328]/10'
                    : 'border-[#1a2e1a] text-[#4a7a4a] cursor-not-allowed'}`}>
                  ↓ DOWNLOAD JSON
                </button>
                <button onClick={duplicateAnimation}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:text-[#00a328] hover:border-[#00a328] transition-colors">
                  ⊕ DUPLICATE
                </button>
                <button onClick={deleteAnimation}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:text-red-400 hover:border-red-400 transition-colors">
                  ✕ DELETE
                </button>
              </div>

              {/* Identity */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className={LABEL}>IDENTITY</span>
                <div className="grid grid-cols-2 gap-3">
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>ID</span>
                    <input type="text" className={INPUT} value={anim.id}
                      onChange={e => patch({ id: e.target.value.toLowerCase().replace(/\s+/g, '-') })} />
                  </label>
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>NAME</span>
                    <input type="text" className={INPUT} value={anim.name}
                      onChange={e => patch({ name: e.target.value })} />
                  </label>
                  <label className="col-span-2 flex flex-col gap-1">
                    <span className={LABEL}>DESCRIPTION</span>
                    <textarea className={INPUT} rows={2} value={anim.description ?? ''}
                      onChange={e => patch({ description: e.target.value })} />
                  </label>
                </div>
              </section>

              {/* Playback + live preview */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-4">
                <span className={LABEL}>PLAYBACK & PREVIEW</span>
                <div className="flex gap-6 flex-wrap">
                  <AnimPreview
                    bank={anim.bank}
                    frames={anim.frames}
                    fps={anim.fps}
                    loop={anim.loop}
                    pingPong={anim.pingPong}
                  />
                  <div className="flex flex-col gap-3 flex-1 min-w-[160px]">
                    <label className="flex flex-col gap-1">
                      <span className={LABEL}>SPRITE BANK</span>
                      <input type="number" min="0" className={INPUT} value={anim.bank}
                        onChange={e => patch({ bank: parseInt(e.target.value) || 0 })} />
                    </label>
                    <label className="flex flex-col gap-1">
                      <span className={LABEL}>FPS</span>
                      <input type="number" min="1" max="60" className={INPUT} value={anim.fps}
                        onChange={e => patch({ fps: parseInt(e.target.value) || 1 })} />
                    </label>
                    <div className="flex gap-4">
                      <label className="flex items-center gap-2 cursor-pointer">
                        <input type="checkbox" checked={anim.loop}
                          onChange={e => patch({ loop: e.target.checked })}
                          className="accent-[#00a328]" />
                        <span className="text-[10px] font-mono text-[#4a7a4a]">Loop</span>
                      </label>
                      <label className="flex items-center gap-2 cursor-pointer">
                        <input type="checkbox" checked={anim.pingPong}
                          onChange={e => patch({ pingPong: e.target.checked })}
                          className="accent-[#00a328]" />
                        <span className="text-[10px] font-mono text-[#4a7a4a]">Ping-pong</span>
                      </label>
                    </div>
                  </div>
                </div>
              </section>

              {/* Frame sequence */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <div className="flex items-center justify-between flex-wrap gap-2">
                  <span className={LABEL}>FRAME SEQUENCE ({anim.frames.length} frames)</span>
                  <div className="flex gap-2 items-center">
                    <span className="text-[9px] font-mono text-[#2a4a2a]">Quick range:</span>
                    <input id="range-from" type="number" min="0" placeholder="0"
                      className="w-12 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-[10px] font-mono px-1 py-0.5 focus:border-[#00a328] outline-none" />
                    <span className="text-[9px] font-mono text-[#4a7a4a]">→</span>
                    <input id="range-to" type="number" min="0" placeholder="7"
                      className="w-12 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-[10px] font-mono px-1 py-0.5 focus:border-[#00a328] outline-none" />
                    <button onClick={() => {
                      const f = parseInt((document.getElementById('range-from') as HTMLInputElement)?.value || '0');
                      const t = parseInt((document.getElementById('range-to') as HTMLInputElement)?.value || '0');
                      if (!isNaN(f) && !isNaN(t) && t >= f) setSequenceFromRange(f, t);
                    }} className="text-[9px] font-mono px-2 py-0.5 border border-[#1a2e1a] text-[#4a7a4a] hover:text-[#00a328] hover:border-[#00a328] transition-colors">
                      SET
                    </button>
                    <button onClick={() => patch({ frames: [] })}
                      className="text-[9px] font-mono px-2 py-0.5 border border-[#1a2e1a] text-[#4a7a4a] hover:text-red-400 hover:border-red-400 transition-colors">
                      CLEAR
                    </button>
                  </div>
                </div>

                {/* Sequence row */}
                {anim.frames.length === 0 ? (
                  <p className="text-[10px] font-mono text-[#2a4a2a]">
                    No frames. Use the bank browser below to click frames and add them.
                  </p>
                ) : (
                  <div className="flex flex-wrap gap-1">
                    {anim.frames.map((f, i) => (
                      <div key={i} className="flex flex-col items-center gap-0.5 border border-[#1a2e1a] bg-[#050c05] p-1">
                        {/* eslint-disable-next-line @next/next/no-img-element */}
                        <img src={`/api/sprites/${anim.bank}/${f}`} alt={`f${f}`}
                          width={40} height={40}
                          style={{ imageRendering: 'pixelated', objectFit: 'contain', display: 'block' }} />
                        <span className="text-[8px] font-mono text-[#4a7a4a]">{f}</span>
                        <div className="flex gap-0.5">
                          <button onClick={() => moveFrame(i, -1)} disabled={i === 0}
                            className="text-[8px] text-[#4a7a4a] hover:text-[#00a328] disabled:opacity-20 px-0.5">←</button>
                          <button onClick={() => removeFrame(i)}
                            className="text-[8px] text-[#4a7a4a] hover:text-red-400 px-0.5">✕</button>
                          <button onClick={() => moveFrame(i, 1)} disabled={i === anim.frames.length - 1}
                            className="text-[8px] text-[#4a7a4a] hover:text-[#00a328] disabled:opacity-20 px-0.5">→</button>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </section>

              {/* Bank browser */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className={LABEL}>BANK BROWSER — click a frame to add to sequence</span>
                <BankBrowser
                  bank={anim.bank}
                  selectedFrames={anim.frames}
                  onAddFrame={addFrame}
                />
              </section>

            </div>
          ) : (
            <div className="flex-1 flex items-center justify-center">
              <span className="text-[10px] font-mono text-[#2a4a2a]">Animation not found.</span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
