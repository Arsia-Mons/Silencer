'use client';
import { useEffect, useRef, useState, useCallback } from 'react';
import Link from 'next/link';
import { useParams, useRouter } from 'next/navigation';
import { useAuth } from '../../../lib/auth';
import Sidebar from '../../../components/Sidebar';
import * as vfxStore from '../../../lib/vfx-store';
import type { VFXPreset } from '../../../lib/vfx-store';

// ── Palette ───────────────────────────────────────────────────────────────────
const INPUT  = 'bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 w-full focus:border-[#00a328] outline-none';
const LABEL  = 'text-[9px] font-mono text-[#4a7a4a] tracking-widest uppercase';
const SELECT = INPUT + ' cursor-pointer';

// ── Particle simulator ────────────────────────────────────────────────────────
interface Particle {
  x: number; y: number;
  vx: number; vy: number;
  age: number; lifetime: number;
  startSize: number; endSize: number;
  startR: number; startG: number; startB: number;
  endR: number; endG: number; endB: number;
  startAlpha: number; endAlpha: number;
}

function hexToRgb(hex: string): [number, number, number] {
  const h = hex.replace('#', '');
  return [
    parseInt(h.slice(0, 2), 16),
    parseInt(h.slice(2, 4), 16),
    parseInt(h.slice(4, 6), 16),
  ];
}

function spawnParticle(preset: VFXPreset, cx: number, cy: number): Particle {
  const angleRad = ((preset.angle - 90) * Math.PI) / 180;
  const halfSpread = ((preset.spread / 2) * Math.PI) / 180;
  const dir = angleRad + (Math.random() - 0.5) * 2 * halfSpread;
  const spd = preset.speed + (Math.random() - 0.5) * 2 * preset.speedVariance;
  const [sr, sg, sb] = hexToRgb(preset.startColor || '#ff8800');
  const [er, eg, eb] = hexToRgb(preset.endColor || '#440000');
  return {
    x: cx, y: cy,
    vx: Math.cos(dir) * spd,
    vy: Math.sin(dir) * spd,
    age: 0,
    lifetime: preset.particleLifetime * (0.8 + Math.random() * 0.4),
    startSize: preset.startSize, endSize: preset.endSize,
    startR: sr, startG: sg, startB: sb,
    endR: er, endG: eg, endB: eb,
    startAlpha: preset.startAlpha, endAlpha: preset.endAlpha,
  };
}

function ParticleCanvas({ preset }: { preset: VFXPreset | null }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const stateRef = useRef<{ particles: Particle[]; lastTime: number; elapsed: number; emitAccum: number }>({
    particles: [], lastTime: 0, elapsed: 0, emitAccum: 0,
  });
  const rafRef = useRef<number>(0);
  const presetRef = useRef(preset);
  presetRef.current = preset;

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const s = stateRef.current;
    s.particles = [];
    s.elapsed = 0;
    s.emitAccum = 0;
    s.lastTime = performance.now();

    function tick(now: number) {
      const p = presetRef.current;
      if (!p || p.effectType !== 'particles') {
        ctx!.clearRect(0, 0, canvas!.width, canvas!.height);
        rafRef.current = requestAnimationFrame(tick);
        return;
      }
      const dt = Math.min((now - s.lastTime) / 1000, 0.05);
      s.lastTime = now;
      s.elapsed += dt;

      const cx = canvas!.width / 2;
      const cy = canvas!.height / 2;

      // Burst mode: fire once per cycle
      const cycleDur = p.duration > 0 ? p.duration : 2.0;
      const cycleTime = s.elapsed % cycleDur;
      const prevCycle = (s.elapsed - dt) % cycleDur;
      if (p.burstCount > 0 && prevCycle > cycleTime) {
        for (let i = 0; i < p.burstCount; i++) s.particles.push(spawnParticle(p, cx, cy));
      }

      // Continuous emission
      if (p.emissionRate > 0 && p.burstCount === 0) {
        s.emitAccum += p.emissionRate * dt;
        while (s.emitAccum >= 1) {
          s.particles.push(spawnParticle(p, cx, cy));
          s.emitAccum--;
        }
      }

      // Update
      s.particles = s.particles.filter(pt => pt.age < pt.lifetime);
      for (const pt of s.particles) {
        pt.vy += p.gravity * dt;
        pt.x += pt.vx * dt;
        pt.y += pt.vy * dt;
        pt.age += dt;
      }

      // Draw
      ctx!.clearRect(0, 0, canvas!.width, canvas!.height);
      for (const pt of s.particles) {
        const t = pt.age / pt.lifetime;
        const size = pt.startSize + (pt.endSize - pt.startSize) * t;
        const r = Math.round(pt.startR + (pt.endR - pt.startR) * t);
        const g = Math.round(pt.startG + (pt.endG - pt.startG) * t);
        const b = Math.round(pt.startB + (pt.endB - pt.startB) * t);
        const a = pt.startAlpha + (pt.endAlpha - pt.startAlpha) * t;
        ctx!.fillStyle = `rgba(${r},${g},${b},${a})`;
        ctx!.fillRect(pt.x - size / 2, pt.y - size / 2, size, size);
      }

      // Crosshair at emit origin
      ctx!.strokeStyle = '#1a2e1a';
      ctx!.lineWidth = 1;
      ctx!.beginPath(); ctx!.moveTo(cx - 8, cy); ctx!.lineTo(cx + 8, cy); ctx!.stroke();
      ctx!.beginPath(); ctx!.moveTo(cx, cy - 8); ctx!.lineTo(cx, cy + 8); ctx!.stroke();

      rafRef.current = requestAnimationFrame(tick);
    }

    rafRef.current = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(rafRef.current);
  }, [preset?.id]); // restart simulator when switching presets

  if (!preset || preset.effectType !== 'particles') {
    return (
      <div className="w-full h-48 border border-[#1a2e1a] bg-[#050c05] flex items-center justify-center">
        <span className="text-[10px] font-mono text-[#2a4a2a]">
          {preset?.effectType === 'screen-shake' ? '⟳ Screen shake — no particle preview' : 'Select a preset to preview'}
        </span>
      </div>
    );
  }

  return (
    <canvas
      ref={canvasRef}
      width={320} height={192}
      className="w-full border border-[#1a2e1a] bg-[#050c05]"
      style={{ imageRendering: 'pixelated' }}
    />
  );
}

// ── Main page ─────────────────────────────────────────────────────────────────
export default function VFXDetailPage() {
  useAuth();
  const { id } = useParams() as { id: string };
  const router = useRouter();
  const [presets, setPresets] = useState<VFXPreset[]>([]);
  const [preset, setPreset] = useState<VFXPreset | null>(null);
  const [dirty, setDirty] = useState(false);
  const [search, setSearch] = useState('');
  const [folderName, setFolderName] = useState<string | null>(null);
  const selectedRef = useRef<HTMLAnchorElement | null>(null);

  // Load store on mount / when id changes
  useEffect(() => {
    if (!vfxStore.isLoaded()) { router.replace('/vfx'); return; }
    setFolderName(vfxStore.getFolderName());
    setPresets(vfxStore.listAll());
    const found = vfxStore.getById(id);
    setPreset(found ? { ...found } : null);
    setDirty(false);
  }, [id, router]);

  // Scroll selected into view
  useEffect(() => {
    selectedRef.current?.scrollIntoView({ block: 'nearest' });
  }, [id]);

  // Arrow key navigation
  useEffect(() => {
    function onKey(e: KeyboardEvent) {
      const tag = (e.target as HTMLElement).tagName.toLowerCase();
      if (['input', 'textarea', 'select'].includes(tag)) return;
      if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') return;
      e.preventDefault();
      const filtered = search
        ? presets.filter(p => p.id.includes(search) || p.name.toLowerCase().includes(search.toLowerCase()))
        : presets;
      const idx = filtered.findIndex(p => p.id === id);
      const next = e.key === 'ArrowDown' ? Math.min(idx + 1, filtered.length - 1) : Math.max(idx - 1, 0);
      if (filtered[next] && filtered[next].id !== id) router.push(`/vfx/${filtered[next].id}`, { scroll: false } as never);
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [id, presets, search, router]);

  const patch = useCallback((partial: Partial<VFXPreset>) => {
    setPreset(prev => prev ? { ...prev, ...partial } : prev);
    setDirty(true);
  }, []);

  const patchNum = useCallback((key: keyof VFXPreset, val: string) => {
    patch({ [key]: val === '' ? 0 : parseFloat(val) } as Partial<VFXPreset>);
  }, [patch]);

  function save() {
    if (!preset) return;
    vfxStore.setPreset(preset);
    setPresets(vfxStore.listAll());
    setDirty(false);
  }

  function handleDownload() {
    save();
    vfxStore.downloadJson(folderName ? `${folderName}.json` : 'vfx-presets.json');
  }

  function addPreset() {
    const base = `effect-${Date.now().toString(36)}`;
    const np: VFXPreset = { ...vfxStore.DEFAULT_PRESET, id: base, name: 'New Effect' };
    vfxStore.addPreset(np);
    setPresets(vfxStore.listAll());
    router.push(`/vfx/${base}`, { scroll: false } as never);
  }

  function duplicatePreset() {
    if (!preset) return;
    save();
    const newId = `${preset.id}-copy`;
    const np: VFXPreset = { ...preset, id: newId, name: `${preset.name} (copy)` };
    vfxStore.addPreset(np);
    setPresets(vfxStore.listAll());
    router.push(`/vfx/${newId}`, { scroll: false } as never);
  }

  function deletePreset() {
    if (!preset || !confirm(`Delete "${preset.name}"?`)) return;
    vfxStore.removePreset(preset.id);
    const remaining = vfxStore.listAll();
    setPresets(remaining);
    if (remaining.length > 0) router.push(`/vfx/${remaining[0].id}`, { scroll: false } as never);
    else router.push('/vfx');
  }

  function closeFolder() {
    vfxStore.clear();
    router.push('/vfx');
  }

  if (!vfxStore.isLoaded()) return null;

  const filtered = search
    ? presets.filter(p => p.id.includes(search) || p.name.toLowerCase().includes(search.toLowerCase()))
    : presets;

  return (
    <div className="flex min-h-screen bg-[#080f08] text-[#d1fad7]">
      <Sidebar />

      <div className="flex-1 flex flex-col overflow-hidden">
        {/* Header */}
        <div className="border-b border-[#1a2e1a] px-4 py-2 flex items-center gap-3 shrink-0">
          <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">✦ VFX EDITOR</span>
          {folderName && (
            <span className="text-[10px] font-mono text-[#2a4a2a]">· {folderName}</span>
          )}
          <div className="ml-auto flex gap-2">
            <button onClick={handleDownload}
              className="text-[10px] font-mono text-[#4a7a4a] hover:text-[#00a328] border border-[#1a2e1a] hover:border-[#00a328] px-2 py-0.5 transition-colors">
              ↓ DOWNLOAD JSON
            </button>
            <button onClick={closeFolder}
              className="text-[10px] font-mono text-[#4a7a4a] hover:text-red-400 border border-[#1a2e1a] hover:border-red-400 px-2 py-0.5 transition-colors">
              ✕ CLOSE
            </button>
          </div>
        </div>

        <div className="flex flex-1 overflow-hidden">
          {/* ── Left: preset list ── */}
          <div className="w-56 border-r border-[#1a2e1a] flex flex-col shrink-0">
            {/* Search */}
            <div className="px-3 py-2 border-b border-[#1a2e1a] flex gap-1">
              <input
                type="text"
                placeholder="filter…"
                value={search}
                onChange={e => setSearch(e.target.value)}
                className="flex-1 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-[10px] font-mono px-2 py-0.5 focus:border-[#00a328] outline-none"
              />
              <button onClick={addPreset}
                title="Add preset"
                className="px-2 text-[#4a7a4a] hover:text-[#00a328] border border-[#1a2e1a] hover:border-[#00a328] text-xs transition-colors">
                +
              </button>
            </div>
            <div className="px-3 py-1 border-b border-[#1a2e1a] shrink-0">
              <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">
                PRESETS ({filtered.length}{search ? `/${presets.length}` : ''})
              </span>
            </div>
            <div className="flex-1 overflow-y-auto">
              {filtered.map(p => (
                <Link key={p.id} href={`/vfx/${p.id}`} scroll={false}
                  ref={p.id === id ? selectedRef : null}
                  className={`flex flex-col px-3 py-2 border-b border-[#1a2e1a] transition-colors ${
                    p.id === id ? 'bg-[#00a328] text-black' : 'hover:bg-[#0a180a] text-[#d1fad7]'
                  }`}>
                  <span className="text-xs font-mono truncate">{p.name}</span>
                  <span className={`text-[10px] font-mono ${p.id === id ? 'text-black/60' : 'text-[#4a7a4a]'}`}>
                    {p.id} · {p.effectType}
                  </span>
                </Link>
              ))}
            </div>
          </div>

          {/* ── Right: editor + preview ── */}
          {preset ? (
            <div className="flex-1 overflow-y-auto p-5 space-y-4">

              {/* Actions */}
              <div className="flex items-center gap-2 flex-wrap">
                <button onClick={handleDownload} disabled={!dirty}
                  className={`px-3 py-1 text-xs font-mono border transition-colors ${dirty
                    ? 'border-[#00a328] text-[#00a328] hover:bg-[#00a328]/10'
                    : 'border-[#1a2e1a] text-[#4a7a4a] cursor-not-allowed'}`}>
                  ↓ DOWNLOAD vfx-presets.json
                </button>
                <button onClick={duplicatePreset}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:text-[#00a328] hover:border-[#00a328] transition-colors">
                  ⊕ DUPLICATE
                </button>
                <button onClick={deletePreset}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:text-red-400 hover:border-red-400 transition-colors">
                  ✕ DELETE
                </button>
              </div>

              {/* Live preview */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <div className="flex items-center justify-between">
                  <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">PREVIEW</span>
                  <span className="text-[9px] font-mono text-[#2a4a2a]">LIVE · auto-loops</span>
                </div>
                <ParticleCanvas preset={preset} />
              </section>

              {/* Identity */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">IDENTITY</span>
                <div className="grid grid-cols-2 gap-3">
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>ID</span>
                    <input type="text" className={INPUT} value={preset.id}
                      onChange={e => patch({ id: e.target.value.toLowerCase().replace(/\s+/g, '-') })} />
                  </label>
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>EFFECT TYPE</span>
                    <select className={SELECT} value={preset.effectType}
                      onChange={e => patch({ effectType: e.target.value as VFXPreset['effectType'] })}>
                      <option value="particles">Particles</option>
                      <option value="sprite-flash">Sprite Flash</option>
                      <option value="screen-shake">Screen Shake</option>
                    </select>
                  </label>
                  <label className="col-span-2 flex flex-col gap-1">
                    <span className={LABEL}>NAME</span>
                    <input type="text" className={INPUT} value={preset.name}
                      onChange={e => patch({ name: e.target.value })} />
                  </label>
                  <label className="col-span-2 flex flex-col gap-1">
                    <span className={LABEL}>DESCRIPTION</span>
                    <textarea className={INPUT} rows={3} value={preset.description ?? ''}
                      onChange={e => patch({ description: e.target.value })} />
                  </label>
                </div>
              </section>

              {/* Particle params */}
              {preset.effectType === 'particles' && (
                <>
                  <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">EMISSION</span>
                    <div className="grid grid-cols-3 gap-3">
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>RATE (p/s)</span>
                        <input type="number" className={INPUT} value={preset.emissionRate}
                          onChange={e => patchNum('emissionRate', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>BURST COUNT</span>
                        <input type="number" className={INPUT} value={preset.burstCount}
                          onChange={e => patchNum('burstCount', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>DURATION (s)</span>
                        <input type="number" step="0.1" className={INPUT} value={preset.duration}
                          onChange={e => patchNum('duration', e.target.value)} />
                      </label>
                    </div>
                    <p className="text-[9px] font-mono text-[#2a4a2a]">
                      Rate&gt;0 + Burst=0 → continuous. Burst&gt;0 → one-shot burst. Duration=0 → infinite loop in preview.
                    </p>
                  </section>

                  <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">PARTICLE LIFETIME & SIZE</span>
                    <div className="grid grid-cols-3 gap-3">
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>LIFETIME (s)</span>
                        <input type="number" step="0.05" className={INPUT} value={preset.particleLifetime}
                          onChange={e => patchNum('particleLifetime', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>START SIZE (px)</span>
                        <input type="number" className={INPUT} value={preset.startSize}
                          onChange={e => patchNum('startSize', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>END SIZE (px)</span>
                        <input type="number" className={INPUT} value={preset.endSize}
                          onChange={e => patchNum('endSize', e.target.value)} />
                      </label>
                    </div>
                  </section>

                  <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">COLOR & ALPHA</span>
                    <div className="grid grid-cols-2 gap-3">
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>START COLOR</span>
                        <div className="flex gap-2 items-center">
                          <input type="color" value={preset.startColor}
                            onChange={e => patch({ startColor: e.target.value })}
                            className="w-8 h-7 border border-[#1a2e1a] bg-[#080f08] cursor-pointer" />
                          <input type="text" className={INPUT} value={preset.startColor}
                            onChange={e => patch({ startColor: e.target.value })} />
                        </div>
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>END COLOR</span>
                        <div className="flex gap-2 items-center">
                          <input type="color" value={preset.endColor}
                            onChange={e => patch({ endColor: e.target.value })}
                            className="w-8 h-7 border border-[#1a2e1a] bg-[#080f08] cursor-pointer" />
                          <input type="text" className={INPUT} value={preset.endColor}
                            onChange={e => patch({ endColor: e.target.value })} />
                        </div>
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>START ALPHA</span>
                        <input type="number" step="0.05" min="0" max="1" className={INPUT} value={preset.startAlpha}
                          onChange={e => patchNum('startAlpha', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>END ALPHA</span>
                        <input type="number" step="0.05" min="0" max="1" className={INPUT} value={preset.endAlpha}
                          onChange={e => patchNum('endAlpha', e.target.value)} />
                      </label>
                    </div>
                  </section>

                  <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">MOTION</span>
                    <div className="grid grid-cols-3 gap-3">
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>SPEED (px/s)</span>
                        <input type="number" className={INPUT} value={preset.speed}
                          onChange={e => patchNum('speed', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>SPEED VARIANCE</span>
                        <input type="number" className={INPUT} value={preset.speedVariance}
                          onChange={e => patchNum('speedVariance', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>GRAVITY (px/s²)</span>
                        <input type="number" className={INPUT} value={preset.gravity}
                          onChange={e => patchNum('gravity', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>SPREAD (°)</span>
                        <input type="number" min="0" max="360" className={INPUT} value={preset.spread}
                          onChange={e => patchNum('spread', e.target.value)} />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className={LABEL}>ANGLE (°)</span>
                        <input type="number" min="-360" max="360" className={INPUT} value={preset.angle}
                          onChange={e => patchNum('angle', e.target.value)} />
                      </label>
                    </div>
                    <p className="text-[9px] font-mono text-[#2a4a2a]">
                      Angle: 0 = right, -90 = up. Spread: 360 = omnidirectional.
                    </p>
                  </section>
                </>
              )}

              {/* Screen shake params */}
              {preset.effectType === 'screen-shake' && (
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SCREEN SHAKE</span>
                  <div className="grid grid-cols-2 gap-3">
                    <label className="flex flex-col gap-1">
                      <span className={LABEL}>INTENSITY (px)</span>
                      <input type="number" className={INPUT} value={preset.shakeIntensity ?? 8}
                        onChange={e => patchNum('shakeIntensity', e.target.value)} />
                    </label>
                    <label className="flex flex-col gap-1">
                      <span className={LABEL}>DURATION (s)</span>
                      <input type="number" step="0.05" className={INPUT} value={preset.shakeDuration ?? 0.25}
                        onChange={e => patchNum('shakeDuration', e.target.value)} />
                    </label>
                  </div>
                </section>
              )}

              {/* Sprite flash params */}
              {preset.effectType === 'sprite-flash' && (
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SPRITE FLASH</span>
                  <div className="grid grid-cols-2 gap-3">
                    <label className="flex flex-col gap-1">
                      <span className={LABEL}>FLASH COLOR</span>
                      <div className="flex gap-2 items-center">
                        <input type="color" value={preset.flashColor ?? '#ffffff'}
                          onChange={e => patch({ flashColor: e.target.value })}
                          className="w-8 h-7 border border-[#1a2e1a] bg-[#080f08] cursor-pointer" />
                        <input type="text" className={INPUT} value={preset.flashColor ?? '#ffffff'}
                          onChange={e => patch({ flashColor: e.target.value })} />
                      </div>
                    </label>
                    <label className="flex flex-col gap-1">
                      <span className={LABEL}>DURATION (s)</span>
                      <input type="number" step="0.05" className={INPUT} value={preset.flashDuration ?? 0.1}
                        onChange={e => patchNum('flashDuration', e.target.value)} />
                    </label>
                  </div>
                </section>
              )}

              {/* GAS note */}
              <div className="border border-[#1a2e1a]/50 rounded p-3">
                <p className="text-[9px] font-mono text-[#2a4a2a]">
                  ℹ GAS extension — vfx-presets.json is consumed by the level designer trigger system.
                  In-game particle rendering requires a C++ particle system (tracked separately).
                </p>
              </div>

            </div>
          ) : (
            <div className="flex-1 flex items-center justify-center">
              <span className="text-[10px] font-mono text-[#2a4a2a]">Preset not found.</span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
