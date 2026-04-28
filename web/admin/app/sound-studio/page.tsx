'use client';
import { useEffect, useRef, useState, useCallback, DragEvent } from 'react';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { apiFetch } from '../../lib/api';
import { decodeAdpcmWav } from './adpcm';

interface SoundEntry {
  name: string;
  storedLength: number | null;
  adpcmBytes: number | null;
  size?: number;
  source: 'bin' | 'staged';
  pendingDelete: boolean;
}

interface SoundRef {
  inBin: boolean;
  cpp: boolean;
  actordefs: string[];
  role: string | null; // 'BG_BASE' | 'BG_AMBIENT' | 'BG_OUTSIDE' | null
}

interface LevelInfo {
  peak: number;  // 0-1
  rms: number;   // 0-1
  waveform: Float32Array; // downsampled PCM for waveform drawing
}

type FilterMode = 'all' | 'cpp' | 'actordef' | 'orphaned' | 'missing' | 'ambient';

function formatBytes(n: number | null | undefined): string {
  if (n == null) return '—';
  if (n < 1024) return `${n} B`;
  if (n < 1048576) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / 1048576).toFixed(1)} MB`;
}

function computeLevel(audioBuffer: AudioBuffer): LevelInfo {
  const data = audioBuffer.getChannelData(0);
  let peak = 0, sumSq = 0;
  for (let i = 0; i < data.length; i++) {
    const abs = Math.abs(data[i]);
    if (abs > peak) peak = abs;
    sumSq += data[i] * data[i];
  }
  const rms = Math.sqrt(sumSq / data.length);

  // Downsample to ~200 points for waveform display
  const pts = 200;
  const step = Math.max(1, Math.floor(data.length / pts));
  const waveform = new Float32Array(pts);
  for (let i = 0; i < pts; i++) {
    let max = 0;
    for (let j = 0; j < step && i * step + j < data.length; j++) {
      const v = Math.abs(data[i * step + j]);
      if (v > max) max = v;
    }
    waveform[i] = max;
  }
  return { peak, rms, waveform };
}

export default function SoundStudioPage() {
  useAuth();

  const [sounds, setSounds] = useState<SoundEntry[]>([]);
  const [refs, setRefs] = useState<Record<string, SoundRef>>({});
  const [levels, setLevels] = useState<Record<string, LevelInfo>>({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [status, setStatus] = useState('');
  const [repacking, setRepacking] = useState(false);
  const [dragOver, setDragOver] = useState(false);
  const [search, setSearch] = useState('');
  const [filter, setFilter] = useState<FilterMode>('all');
  const [normalize, setNormalize] = useState(false);

  // Rename dialog
  const [renameTarget, setRenameTarget] = useState<string | null>(null);
  const [renameValue, setRenameValue] = useState('');
  const [renaming, setRenaming] = useState(false);

  const audioCtxRef = useRef<AudioContext | null>(null);
  const audioSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const loopingSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const [playing, setPlaying] = useState<string | null>(null);
  const [looping, setLooping] = useState<string | null>(null);
  const [selectedIdx, setSelectedIdx] = useState<number>(-1);
  const fileInputRef = useRef<HTMLInputElement | null>(null);
  const rowRefs = useRef<(HTMLTableRowElement | null)[]>([]);
  const soundsRef = useRef<SoundEntry[]>([]);
  const waveformCanvasRef = useRef<HTMLCanvasElement | null>(null);

  const getToken = () => typeof window !== 'undefined' ? localStorage.getItem('zs_token') : '';

  const load = useCallback(async () => {
    setLoading(true);
    setError('');
    try {
      const [soundsData, refsData] = await Promise.all([
        apiFetch('/sounds') as Promise<SoundEntry[]>,
        apiFetch('/sounds/refs') as Promise<Record<string, SoundRef>>,
      ]);
      setSounds(soundsData);
      setRefs(refsData);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { load(); }, [load]);
  useEffect(() => { soundsRef.current = sounds; }, [sounds]);

  // Draw waveform in inspector canvas when levels change for selected sound
  useEffect(() => {
    const visible = sounds.filter(s => !s.pendingDelete);
    const sel = visible[selectedIdx];
    if (!sel || !waveformCanvasRef.current) return;
    const lvl = levels[sel.name];
    const canvas = waveformCanvasRef.current;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (!lvl) {
      ctx.fillStyle = '#333';
      ctx.font = '11px monospace';
      ctx.fillText('play to show waveform', 8, canvas.height / 2 + 4);
      return;
    }
    const { waveform } = lvl;
    const w = canvas.width, h = canvas.height;
    const mid = h / 2;
    ctx.fillStyle = '#1a2a1a';
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = '#4a8';
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let i = 0; i < waveform.length; i++) {
      const x = (i / waveform.length) * w;
      const amp = waveform[i] * mid;
      ctx.moveTo(x, mid - amp);
      ctx.lineTo(x, mid + amp);
    }
    ctx.stroke();
  }, [levels, selectedIdx, sounds]);

  // Arrow key navigation
  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') return;
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === 'INPUT' || tag === 'BUTTON' || tag === 'SELECT') return;
      e.preventDefault();
      const list = soundsRef.current.filter(s => !s.pendingDelete);
      if (!list.length) return;
      setSelectedIdx(prev => {
        const next = e.key === 'ArrowDown'
          ? Math.min(prev + 1, list.length - 1)
          : Math.max(prev - 1, 0);
        rowRefs.current[next]?.scrollIntoView({ block: 'nearest' });
        play(list[next].name);
        return next;
      });
    }
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // ── Playback ────────────────────────────────────────────────────────────────

  async function fetchAndDecode(name: string): Promise<AudioBuffer> {
    const r = await fetch(`/api/sounds/${encodeURIComponent(name)}/play`, {
      headers: { Authorization: `Bearer ${getToken()}` },
    });
    if (!r.ok) {
      const err = await r.json().catch(() => ({ error: r.statusText })) as { error?: string };
      throw new Error(err.error || r.statusText);
    }
    const arrayBuf = await r.arrayBuffer();
    if (!audioCtxRef.current || audioCtxRef.current.state === 'closed') {
      audioCtxRef.current = new AudioContext();
    }
    const audioCtx = audioCtxRef.current;
    if (audioCtx.state === 'suspended') await audioCtx.resume();
    return decodeAdpcmWav(arrayBuf, audioCtx);
  }

  async function play(name: string) {
    if (audioSourceRef.current) {
      try { audioSourceRef.current.stop(); } catch {}
      audioSourceRef.current = null;
    }
    if (playing === name) { setPlaying(null); return; }
    try {
      const decoded = await fetchAndDecode(name);
      // Cache level info
      setLevels(prev => ({ ...prev, [name]: computeLevel(decoded) }));
      const audioCtx = audioCtxRef.current!;
      const source = audioCtx.createBufferSource();
      source.buffer = decoded;
      source.connect(audioCtx.destination);
      source.start();
      audioSourceRef.current = source;
      setPlaying(name);
      source.onended = () => { setPlaying(null); audioSourceRef.current = null; };
    } catch (e: any) {
      setPlaying(null);
      setError(`Could not play ${name}: ${e.message}`);
    }
  }

  async function toggleLoop(name: string) {
    // Stop any existing loop
    if (loopingSourceRef.current) {
      try { loopingSourceRef.current.stop(); } catch {}
      loopingSourceRef.current = null;
      setLooping(null);
      if (looping === name) return;
    }
    try {
      const decoded = await fetchAndDecode(name);
      setLevels(prev => ({ ...prev, [name]: computeLevel(decoded) }));
      const audioCtx = audioCtxRef.current!;
      const source = audioCtx.createBufferSource();
      source.buffer = decoded;
      source.loop = true;
      source.connect(audioCtx.destination);
      source.start();
      loopingSourceRef.current = source;
      setLooping(name);
    } catch (e: any) {
      setError(`Could not loop ${name}: ${e.message}`);
    }
  }

  // ── Upload ──────────────────────────────────────────────────────────────────

  async function uploadFile(file: File) {
    const name = file.name.replace(/[^a-zA-Z0-9!._-]/g, '_');
    setStatus(`Uploading ${name}…`);
    try {
      const buf = await file.arrayBuffer();
      await apiFetch('/sounds', {
        method: 'POST',
        headers: { 'Content-Type': 'application/octet-stream', 'X-Filename': name },
        body: buf,
      });
      setStatus(`Staged ${name}`);
      load();
    } catch (e: any) {
      setError(e.message); setStatus('');
    }
  }

  function onFileInput(e: React.ChangeEvent<HTMLInputElement>) {
    Array.from(e.target.files || []).forEach(uploadFile);
    e.target.value = '';
  }

  function onDrop(e: DragEvent<HTMLDivElement>) {
    e.preventDefault(); setDragOver(false);
    Array.from(e.dataTransfer.files).forEach(uploadFile);
  }

  // ── Delete / Restore ────────────────────────────────────────────────────────

  async function deleteSnd(name: string) {
    try {
      await apiFetch(`/sounds/${encodeURIComponent(name)}`, { method: 'DELETE' });
      setStatus(`${name} marked for deletion`); load();
    } catch (e: any) { setError(e.message); }
  }

  async function restoreSnd(name: string) {
    try {
      await apiFetch(`/sounds/${encodeURIComponent(name)}/restore`, { method: 'POST' });
      setStatus(`${name} restored`); load();
    } catch (e: any) { setError(e.message); }
  }

  // ── Rename ──────────────────────────────────────────────────────────────────

  function openRename(name: string) {
    setRenameTarget(name);
    setRenameValue(name);
  }

  async function submitRename() {
    if (!renameTarget || !renameValue.trim() || renaming) return;
    setRenaming(true);
    try {
      const result = await apiFetch(`/sounds/${encodeURIComponent(renameTarget)}/rename`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ newName: renameValue.trim() }),
      }) as { newName: string; updatedActors: string[]; cppWarning: boolean };
      let msg = `Renamed ${renameTarget} → ${result.newName}`;
      if (result.updatedActors.length) msg += `. Updated actordefs: ${result.updatedActors.join(', ')}`;
      if (result.cppWarning) msg += '. ⚠ C++ source references old name — needs code update!';
      setStatus(msg);
      setRenameTarget(null);
      load();
    } catch (e: any) {
      setError(e.message);
    } finally {
      setRenaming(false);
    }
  }

  // ── Repack ──────────────────────────────────────────────────────────────────

  async function repack() {
    if (!confirm('Rebuild sound.bin now? The existing file will be replaced.')) return;
    setRepacking(true); setStatus('Repacking…'); setError('');
    try {
      const result = await apiFetch('/sounds/repack', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ normalize }),
      }) as { numsounds: number; totalSize: number };
      setStatus(`Repacked — ${result.numsounds} sounds, ${formatBytes(result.totalSize)}${normalize ? ' (normalized)' : ''}`);
      load();
    } catch (e: any) {
      setError(e.message); setStatus('');
    } finally {
      setRepacking(false);
    }
  }

  // ── Derived / filtering ─────────────────────────────────────────────────────

  const staged = sounds.filter(s => s.source === 'staged');
  const pendingDels = sounds.filter(s => s.pendingDelete);
  const hasPending = staged.length > 0 || pendingDels.length > 0;

  // Missing: in refs but not in bin/staged
  const missingNames = Object.entries(refs)
    .filter(([, r]) => !r.inBin && (r.cpp || r.actordefs.length > 0))
    .map(([name]) => name)
    .sort();

  // Build display list: sounds + missing entries
  const allEntries: (SoundEntry & { missing?: boolean })[] = [
    ...sounds,
    ...missingNames.map(name => ({
      name, storedLength: null, adpcmBytes: null, source: 'bin' as const,
      pendingDelete: false, missing: true,
    })),
  ];

  function matchesFilter(s: SoundEntry & { missing?: boolean }): boolean {
    const ref = refs[s.name];
    if (search && !s.name.toLowerCase().includes(search.toLowerCase())) return false;
    if (s.missing) return filter === 'missing';
    if (filter === 'missing') return false;
    if (filter === 'all') return true;
    if (filter === 'cpp') return !!(ref?.cpp);
    if (filter === 'actordef') return !!(ref?.actordefs?.length);
    if (filter === 'orphaned') return !!ref && !ref.cpp && !ref.actordefs?.length;
    if (filter === 'ambient') return !!(ref?.role);
    return true;
  }

  const visibleEntries = allEntries.filter(matchesFilter);

  // Remap selectedIdx to visible list
  const visibleNonDeleted = visibleEntries.filter(s => !s.pendingDelete);
  const selectedSound = visibleNonDeleted[selectedIdx];
  const selectedRef = selectedSound ? refs[selectedSound.name] : null;
  const selectedLevel = selectedSound ? levels[selectedSound.name] : null;

  // ── Render ──────────────────────────────────────────────────────────────────

  return (
    <div style={{ display: 'flex', height: '100vh', fontFamily: 'monospace', background: '#111', color: '#ccc' }}>
      <Sidebar />

      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {/* Header */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 12, padding: '10px 16px', borderBottom: '1px solid #333', background: '#151515', flexWrap: 'wrap' }}>
          <span style={{ fontSize: 16, color: '#aaa', fontWeight: 'bold' }}>[ SOUND STUDIO ]</span>
          <span style={{ color: '#555', fontSize: 11 }}>sound.bin packer</span>

          {hasPending && (
            <span style={{ background: '#f90', color: '#000', padding: '2px 8px', borderRadius: 4, fontSize: 11 }}>
              {staged.length > 0 && `+${staged.length} staged`}
              {staged.length > 0 && pendingDels.length > 0 && '  '}
              {pendingDels.length > 0 && `-${pendingDels.length} deletions`}
            </span>
          )}

          <div style={{ flex: 1 }} />

          <label style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 11, color: '#888', cursor: 'pointer' }}>
            <input type="checkbox" checked={normalize} onChange={e => setNormalize(e.target.checked)} />
            normalize
          </label>

          <button
            onClick={() => fileInputRef.current?.click()}
            style={{ padding: '4px 10px', background: '#333', color: '#ccc', border: '1px solid #555', borderRadius: 4, cursor: 'pointer', fontFamily: 'monospace', fontSize: 12 }}
          >
            + Upload WAV
          </button>
          <input ref={fileInputRef} type="file" accept=".wav,audio/*" multiple style={{ display: 'none' }} onChange={onFileInput} />

          <button
            onClick={repack}
            disabled={repacking}
            style={{
              padding: '4px 14px', fontSize: 12,
              background: hasPending ? '#4a8' : '#555',
              color: hasPending ? '#fff' : '#999',
              border: `1px solid ${hasPending ? '#6ca' : '#666'}`,
              borderRadius: 4, cursor: repacking ? 'wait' : 'pointer', fontFamily: 'monospace', fontWeight: 'bold',
            }}
          >
            {repacking ? 'Repacking…' : '⚡ Save & Repack'}
          </button>
        </div>

        {/* Status bar */}
        {(status || error) && (
          <div style={{
            padding: '5px 16px', fontSize: 11,
            background: error ? '#200' : '#1a1a1a',
            color: error ? '#f66' : '#8c8',
            borderBottom: '1px solid #222',
          }}>
            {error || status}
            <button onClick={() => { setError(''); setStatus(''); }}
              style={{ marginLeft: 10, background: 'none', border: 'none', color: '#555', cursor: 'pointer', fontFamily: 'monospace' }}>×</button>
          </div>
        )}

        {/* Missing sounds banner */}
        {missingNames.length > 0 && filter !== 'missing' && (
          <div style={{ padding: '5px 16px', fontSize: 11, background: '#2a1400', color: '#f90', borderBottom: '1px solid #333', display: 'flex', alignItems: 'center', gap: 10 }}>
            <span>⚠ {missingNames.length} sound{missingNames.length > 1 ? 's' : ''} referenced by game but missing from sound.bin</span>
            <button onClick={() => setFilter('missing')}
              style={{ background: 'none', border: '1px solid #f90', color: '#f90', borderRadius: 3, padding: '0 6px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
              Show missing
            </button>
          </div>
        )}

        {/* Search + filter bar */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '7px 16px', borderBottom: '1px solid #222', background: '#131313' }}>
          <input
            type="text"
            placeholder="search…"
            value={search}
            onChange={e => setSearch(e.target.value)}
            style={{ padding: '3px 8px', background: '#222', border: '1px solid #444', borderRadius: 3, color: '#ccc', fontFamily: 'monospace', fontSize: 12, width: 160 }}
          />
          {(['all','cpp','actordef','ambient','orphaned','missing'] as FilterMode[]).map(f => (
            <button key={f} onClick={() => setFilter(f)}
              style={{
                padding: '2px 8px', fontSize: 11, fontFamily: 'monospace',
                background: filter === f ? '#2a3a4a' : 'transparent',
                border: `1px solid ${filter === f ? '#4af' : '#333'}`,
                color: filter === f ? '#8cf' : '#666',
                borderRadius: 3, cursor: 'pointer',
              }}>
              {f}{f === 'missing' && missingNames.length > 0 ? ` (${missingNames.length})` : ''}
            </button>
          ))}
          <span style={{ marginLeft: 'auto', color: '#444', fontSize: 11 }}>{visibleEntries.length} shown</span>
        </div>

        {/* Main content: list + inspector */}
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>

          {/* Sound list */}
          <div
            style={{ flex: 1, overflow: 'auto', padding: '0 0 4px 0', position: 'relative' }}
            onDragOver={e => { e.preventDefault(); setDragOver(true); }}
            onDragLeave={() => setDragOver(false)}
            onDrop={onDrop}
          >
            {dragOver && (
              <div style={{
                position: 'absolute', inset: 0, background: 'rgba(80,200,120,0.12)',
                border: '2px dashed #4a8', zIndex: 10,
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                fontSize: 20, color: '#4a8', pointerEvents: 'none',
              }}>Drop WAV files to stage</div>
            )}

            {loading ? (
              <div style={{ color: '#555', padding: 40, textAlign: 'center' }}>Loading…</div>
            ) : (
              <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 12 }}>
                <thead>
                  <tr style={{ color: '#444', borderBottom: '1px solid #2a2a2a', position: 'sticky', top: 0, background: '#131313' }}>
                    <th style={{ textAlign: 'left', padding: '5px 6px', width: 22 }}></th>
                    <th style={{ textAlign: 'left', padding: '5px 6px' }}>Name</th>
                    <th style={{ textAlign: 'left', padding: '5px 6px', width: 80 }}>Refs</th>
                    <th style={{ textAlign: 'left', padding: '5px 6px', width: 120 }}>Level</th>
                    <th style={{ textAlign: 'right', padding: '5px 6px', width: 70 }}>Size</th>
                    <th style={{ textAlign: 'right', padding: '5px 6px', width: 110 }}>Actions</th>
                  </tr>
                </thead>
                <tbody>
                  {visibleEntries.map((s, listIdx) => {
                    const visIdx = visibleNonDeleted.indexOf(s as any);
                    const isPlaying = playing === s.name;
                    const isLooping = looping === s.name;
                    const isSelected = !s.pendingDelete && visIdx >= 0 && visIdx === selectedIdx;
                    const size = s.source === 'bin' ? s.adpcmBytes : s.size;
                    const ref = refs[s.name];
                    const lvl = levels[s.name];
                    const isMissing = !!(s as any).missing;

                    return (
                      <tr
                        key={s.name}
                        ref={el => { if (!s.pendingDelete && visIdx >= 0) rowRefs.current[visIdx] = el; }}
                        onClick={() => {
                          if (!s.pendingDelete && !isMissing) {
                            setSelectedIdx(visIdx);
                            play(s.name);
                          } else if (!s.pendingDelete && isMissing) {
                            setSelectedIdx(visIdx);
                          }
                        }}
                        style={{
                          borderBottom: '1px solid #1e1e1e',
                          opacity: s.pendingDelete ? 0.4 : isMissing ? 0.7 : 1,
                          cursor: s.pendingDelete ? 'default' : 'pointer',
                          background: isMissing
                            ? '#1e1000'
                            : isPlaying
                            ? '#1a2a1a'
                            : isSelected
                            ? '#1e1e28'
                            : s.source === 'staged' ? '#1a1a2a' : 'transparent',
                          outline: isSelected ? '1px solid #445' : 'none',
                        }}
                      >
                        {/* Play button */}
                        <td style={{ padding: '3px 6px', textAlign: 'center' }}>
                          {!isMissing ? (
                            <button
                              onClick={e => { e.stopPropagation(); if (!s.pendingDelete) { setSelectedIdx(visIdx); play(s.name); } }}
                              disabled={s.pendingDelete}
                              title={isPlaying ? 'Stop' : 'Play'}
                              style={{ background: 'none', border: 'none', color: isPlaying ? '#4a8' : '#555', cursor: 'pointer', fontSize: 13, padding: 0 }}
                            >
                              {isPlaying ? '⏹' : '▶'}
                            </button>
                          ) : (
                            <span style={{ color: '#f90', fontSize: 11 }}>✗</span>
                          )}
                        </td>

                        {/* Name + badges */}
                        <td style={{ padding: '3px 6px', color: s.pendingDelete ? '#555' : isMissing ? '#f90' : '#ddd' }}>
                          <span>{s.name}</span>
                          {s.pendingDelete && <span style={{ marginLeft: 6, color: '#f66', fontSize: 10 }}>[del]</span>}
                          {s.source === 'staged' && <span style={{ marginLeft: 6, color: '#88f', fontSize: 10 }}>[staged]</span>}
                          {isMissing && <span style={{ marginLeft: 6, color: '#f90', fontSize: 10 }}>[missing]</span>}
                          {ref?.role && <span style={{ marginLeft: 6, color: '#8af', fontSize: 10 }}>[{ref.role}]</span>}
                        </td>

                        {/* Ref badges */}
                        <td style={{ padding: '3px 6px' }}>
                          <span style={{ display: 'inline-flex', gap: 3 }}>
                            {ref?.cpp && (
                              <span title="Referenced in C++ source" style={{ background: '#2a3a2a', border: '1px solid #3a5a3a', color: '#8c8', borderRadius: 2, padding: '0 4px', fontSize: 10 }}>C++</span>
                            )}
                            {ref?.actordefs?.length > 0 && (
                              <span title={`Used by: ${ref.actordefs.join(', ')}`} style={{ background: '#1a2a3a', border: '1px solid #2a4a6a', color: '#68a', borderRadius: 2, padding: '0 4px', fontSize: 10 }}>ADef</span>
                            )}
                            {!ref?.cpp && !ref?.actordefs?.length && !isMissing && (
                              <span title="No code references found" style={{ color: '#444', fontSize: 10 }}>—</span>
                            )}
                          </span>
                        </td>

                        {/* Level bar */}
                        <td style={{ padding: '3px 6px' }}>
                          {lvl ? (
                            <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                              <div style={{ width: 80, height: 6, background: '#222', borderRadius: 2, overflow: 'hidden' }}>
                                <div style={{
                                  width: `${Math.round(lvl.peak * 100)}%`, height: '100%',
                                  background: lvl.peak > 0.9 ? '#f44' : lvl.peak > 0.6 ? '#fa4' : '#4a8',
                                  borderRadius: 2,
                                }} />
                              </div>
                              <span style={{ color: '#555', fontSize: 10 }}>{Math.round(lvl.peak * 100)}%</span>
                            </div>
                          ) : (
                            <span style={{ color: '#333', fontSize: 10 }}>—</span>
                          )}
                        </td>

                        {/* Size */}
                        <td style={{ padding: '3px 6px', textAlign: 'right', color: '#555', fontVariantNumeric: 'tabular-nums' }}>
                          {formatBytes(size)}
                        </td>

                        {/* Actions */}
                        <td style={{ padding: '3px 6px', textAlign: 'right' }}>
                          <span style={{ display: 'inline-flex', gap: 4 }}>
                            {ref?.role && !isMissing && (
                              <button
                                onClick={e => { e.stopPropagation(); toggleLoop(s.name); }}
                                title={isLooping ? 'Stop loop' : 'Loop (ambient test)'}
                                style={{ background: isLooping ? '#1a2a3a' : 'none', border: `1px solid ${isLooping ? '#4af' : '#333'}`, color: isLooping ? '#8cf' : '#555', borderRadius: 3, padding: '0 5px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}
                              >
                                {isLooping ? '⏹↺' : '↺'}
                              </button>
                            )}
                            {!isMissing && !s.pendingDelete && (
                              <button
                                onClick={e => { e.stopPropagation(); openRename(s.name); }}
                                style={{ background: 'none', border: '1px solid #333', color: '#777', borderRadius: 3, padding: '0 5px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}
                              >✎</button>
                            )}
                            {s.pendingDelete ? (
                              <button onClick={e => { e.stopPropagation(); restoreSnd(s.name); }}
                                style={{ background: 'none', border: '1px solid #555', color: '#8a8', borderRadius: 3, padding: '0 6px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}>
                                Restore
                              </button>
                            ) : !isMissing ? (
                              <button onClick={e => { e.stopPropagation(); deleteSnd(s.name); }}
                                style={{ background: 'none', border: '1px solid #333', color: '#844', borderRadius: 3, padding: '0 6px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}>
                                ✕
                              </button>
                            ) : null}
                          </span>
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            )}

            {!loading && visibleEntries.length === 0 && (
              <div style={{ color: '#444', textAlign: 'center', padding: 60 }}>
                No sounds match the current filter.<br />
                <span style={{ fontSize: 11 }}>Try "all" or drop WAV files to add sounds.</span>
              </div>
            )}
          </div>

          {/* Inspector sidebar */}
          {selectedSound && (
            <div style={{ width: 260, borderLeft: '1px solid #222', background: '#131313', display: 'flex', flexDirection: 'column', overflow: 'hidden', flexShrink: 0 }}>
              <div style={{ padding: '10px 12px', borderBottom: '1px solid #222' }}>
                <div style={{ fontSize: 12, color: '#aaa', fontWeight: 'bold', wordBreak: 'break-all' }}>{selectedSound.name}</div>
                {selectedRef?.role && (
                  <div style={{ marginTop: 4, fontSize: 11, color: '#8af' }}>🌐 Ambient: {selectedRef.role}</div>
                )}
                {selectedLevel && (
                  <div style={{ marginTop: 6, fontSize: 11, color: '#666', display: 'flex', gap: 12 }}>
                    <span>Peak: <span style={{ color: selectedLevel.peak > 0.9 ? '#f44' : '#ccc' }}>{(selectedLevel.peak * 100).toFixed(1)}%</span></span>
                    <span>RMS: <span style={{ color: '#ccc' }}>{(selectedLevel.rms * 100).toFixed(1)}%</span></span>
                  </div>
                )}
              </div>

              {/* Waveform */}
              <div style={{ padding: '8px 12px', borderBottom: '1px solid #222' }}>
                <canvas
                  ref={waveformCanvasRef}
                  width={236}
                  height={48}
                  style={{ width: '100%', height: 48, display: 'block', borderRadius: 2, background: '#1a2a1a' }}
                />
              </div>

              {/* C++ usage */}
              {selectedRef?.cpp && (
                <div style={{ padding: '8px 12px', borderBottom: '1px solid #1e1e1e' }}>
                  <div style={{ fontSize: 10, color: '#555', marginBottom: 4 }}>C++ REFERENCES</div>
                  <div style={{ fontSize: 11, color: '#8c8' }}>Referenced in game source</div>
                  {selectedRef.role && (
                    <div style={{ fontSize: 10, color: '#666', marginTop: 2 }}>
                      Role: {selectedRef.role === 'BG_BASE' ? 'Base ambient (indoor)' : selectedRef.role === 'BG_AMBIENT' ? 'Ambient hum' : 'Outside wind'}
                    </div>
                  )}
                </div>
              )}

              {/* Actordef usage */}
              {selectedRef?.actordefs && selectedRef.actordefs.length > 0 && (
                <div style={{ padding: '8px 12px', flex: 1, overflow: 'auto' }}>
                  <div style={{ fontSize: 10, color: '#555', marginBottom: 4 }}>USED BY ACTORS</div>
                  {selectedRef.actordefs.map(actor => (
                    <div key={actor} style={{ fontSize: 11, color: '#68a', padding: '2px 0', borderBottom: '1px solid #1a1a1a' }}>
                      {actor}
                    </div>
                  ))}
                </div>
              )}

              {/* No refs */}
              {selectedRef && !selectedRef.cpp && (!selectedRef.actordefs || selectedRef.actordefs.length === 0) && (
                <div style={{ padding: '8px 12px', fontSize: 11, color: '#444' }}>
                  No references found.<br />
                  <span style={{ fontSize: 10, color: '#333' }}>Orphaned sound — safe to remove.</span>
                </div>
              )}
            </div>
          )}
        </div>

        {/* Footer */}
        <div style={{ padding: '5px 16px', borderTop: '1px solid #222', background: '#151515', fontSize: 11, color: '#444', display: 'flex', gap: 16, flexWrap: 'wrap' }}>
          <span>{sounds.filter(s => s.source === 'bin' && !s.pendingDelete).length} in bin</span>
          {staged.length > 0 && <span style={{ color: '#88f' }}>{staged.length} staged</span>}
          {pendingDels.length > 0 && <span style={{ color: '#f66' }}>{pendingDels.length} pending deletion</span>}
          {missingNames.length > 0 && <span style={{ color: '#f90' }}>{missingNames.length} missing</span>}
          <span>drop WAV files to stage</span>
          <span style={{ marginLeft: 'auto', color: '#333' }}>↑↓ navigate &amp; play</span>
        </div>
      </div>

      {/* Rename dialog */}
      {renameTarget && (
        <div style={{
          position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.7)',
          display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 100,
        }} onClick={() => setRenameTarget(null)}>
          <div
            onClick={e => e.stopPropagation()}
            style={{ background: '#1a1a1a', border: '1px solid #444', borderRadius: 6, padding: 20, width: 340, fontFamily: 'monospace' }}
          >
            <div style={{ fontSize: 13, color: '#aaa', marginBottom: 12 }}>Rename sound</div>
            <input
              autoFocus
              value={renameValue}
              onChange={e => setRenameValue(e.target.value)}
              onKeyDown={e => { if (e.key === 'Enter') submitRename(); if (e.key === 'Escape') setRenameTarget(null); }}
              style={{ width: '100%', padding: '6px 10px', background: '#222', border: '1px solid #555', borderRadius: 3, color: '#ddd', fontFamily: 'monospace', fontSize: 13, boxSizing: 'border-box' }}
            />
            {refs[renameTarget]?.cpp && (
              <div style={{ marginTop: 8, fontSize: 11, color: '#f90' }}>
                ⚠ This sound is hardcoded in C++ — renaming here won't update the source code.
              </div>
            )}
            {refs[renameTarget]?.actordefs?.length > 0 && (
              <div style={{ marginTop: 6, fontSize: 11, color: '#8af' }}>
                ✓ Actordefs will be updated automatically ({refs[renameTarget].actordefs.join(', ')}).
              </div>
            )}
            <div style={{ marginTop: 12, display: 'flex', gap: 8, justifyContent: 'flex-end' }}>
              <button onClick={() => setRenameTarget(null)}
                style={{ padding: '5px 12px', background: 'none', border: '1px solid #444', color: '#888', borderRadius: 3, cursor: 'pointer', fontFamily: 'monospace' }}>
                Cancel
              </button>
              <button onClick={submitRename} disabled={renaming}
                style={{ padding: '5px 12px', background: '#2a3a4a', border: '1px solid #4af', color: '#8cf', borderRadius: 3, cursor: 'pointer', fontFamily: 'monospace' }}>
                {renaming ? 'Renaming…' : 'Rename'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

