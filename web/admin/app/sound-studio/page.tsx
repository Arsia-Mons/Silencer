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
  role: string | null;
  loop: boolean;
  category: string | null;
  volumeCalls: { ctx: string; vol: number | string }[];
}

interface LevelInfo {
  peak: number;
  rms: number;
  waveform: Float32Array;
}

interface MusicFile {
  name: string;
  size: number;
}

type FilterMode = 'all' | 'cpp' | 'actordef' | 'orphaned' | 'missing' | 'ambient';
type TabMode = 'sounds' | 'music';

const CATEGORY_LABELS: Record<string, string> = {
  player: 'Player', npc: 'NPCs / Enemies', weapon: 'Weapons',
  world: 'World / Objects', ui: 'UI', ambient: 'Ambient',
};
const CATEGORY_ORDER = ['player','npc','weapon','world','ui','ambient'];

function formatBytes(n: number | null | undefined): string {
  if (n == null) return '—';
  if (n < 1024) return `${n} B`;
  if (n < 1048576) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / 1048576).toFixed(1)} MB`;
}

function volColor(v: number | string): string {
  if (typeof v !== 'number') return '#888';
  if (v >= 112) return '#f66';
  if (v >= 64) return '#fa4';
  return '#8c8';
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

  // ── State ───────────────────────────────────────────────────────────────────
  const [tab, setTab] = useState<TabMode>('sounds');
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
  const [groupByCategory, setGroupByCategory] = useState(false);
  const [collapsedCats, setCollapsedCats] = useState<Set<string>>(new Set());
  const [normalize, setNormalize] = useState(false);
  const [distance, setDistance] = useState(0);

  // Rename dialog
  const [renameTarget, setRenameTarget] = useState<string | null>(null);
  const [renameValue, setRenameValue] = useState('');
  const [renaming, setRenaming] = useState(false);

  // Music tab
  const [musicFiles, setMusicFiles] = useState<MusicFile[]>([]);
  const [musicLoading, setMusicLoading] = useState(false);
  const [playingMusic, setPlayingMusic] = useState<string | null>(null);

  const audioCtxRef = useRef<AudioContext | null>(null);
  const gainNodeRef = useRef<GainNode | null>(null);
  const audioSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const loopingSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const musicSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const [playing, setPlaying] = useState<string | null>(null);
  const [looping, setLooping] = useState<string | null>(null);
  const [selectedIdx, setSelectedIdx] = useState<number>(-1);
  const fileInputRef = useRef<HTMLInputElement | null>(null);
  const rowRefs = useRef<(HTMLTableRowElement | null)[]>([]);
  const soundsRef = useRef<SoundEntry[]>([]);
  const waveformCanvasRef = useRef<HTMLCanvasElement | null>(null);

  const getToken = () => typeof window !== 'undefined' ? localStorage.getItem('zs_token') : '';

  // ── Load ────────────────────────────────────────────────────────────────────
  const load = useCallback(async () => {
    setLoading(true); setError('');
    try {
      const [soundsData, refsData] = await Promise.all([
        apiFetch('/sounds') as Promise<SoundEntry[]>,
        apiFetch('/sounds/refs') as Promise<Record<string, SoundRef>>,
      ]);
      setSounds(soundsData); setRefs(refsData);
    } catch (e: any) { setError(e.message); }
    finally { setLoading(false); }
  }, []);

  useEffect(() => { load(); }, [load]);
  useEffect(() => { soundsRef.current = sounds; }, [sounds]);

  const loadMusic = useCallback(async () => {
    setMusicLoading(true);
    try {
      const data = await apiFetch('/sounds/music') as MusicFile[];
      setMusicFiles(data);
    } catch {}
    finally { setMusicLoading(false); }
  }, []);

  useEffect(() => { if (tab === 'music') loadMusic(); }, [tab, loadMusic]);

  // ── Distance gain update ────────────────────────────────────────────────────
  useEffect(() => {
    if (gainNodeRef.current) {
      gainNodeRef.current.gain.value = Math.max(0, 1 - distance / 500);
    }
  }, [distance]);

  // ── Waveform canvas ─────────────────────────────────────────────────────────
  useEffect(() => {
    const visible = sounds.filter(s => !s.pendingDelete);
    const sel = visible[selectedIdx];
    if (!waveformCanvasRef.current) return;
    const canvas = waveformCanvasRef.current;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (!sel) return;
    const lvl = levels[sel.name];
    const w = canvas.width, h = canvas.height;
    if (!lvl) {
      ctx.fillStyle = '#333'; ctx.font = '10px monospace';
      ctx.fillText('play to show waveform', 6, h / 2 + 3);
      return;
    }
    ctx.fillStyle = '#1a2a1a'; ctx.fillRect(0, 0, w, h);
    const mid = h / 2;
    ctx.strokeStyle = '#4a8'; ctx.lineWidth = 1;
    ctx.beginPath();
    for (let i = 0; i < lvl.waveform.length; i++) {
      const x = (i / lvl.waveform.length) * w;
      const amp = lvl.waveform[i] * mid;
      ctx.moveTo(x, mid - amp); ctx.lineTo(x, mid + amp);
    }
    ctx.stroke();
  }, [levels, selectedIdx, sounds]);

  // ── Arrow key navigation ────────────────────────────────────────────────────
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

  // ── Audio context helpers ───────────────────────────────────────────────────
  function getAudioCtx() {
    if (!audioCtxRef.current || audioCtxRef.current.state === 'closed') {
      audioCtxRef.current = new AudioContext();
    }
    return audioCtxRef.current;
  }

  async function fetchAndDecode(name: string): Promise<AudioBuffer> {
    const r = await fetch(`/api/sounds/${encodeURIComponent(name)}/play`, {
      headers: { Authorization: `Bearer ${getToken()}` },
    });
    if (!r.ok) {
      const err = await r.json().catch(() => ({ error: r.statusText })) as { error?: string };
      throw new Error(err.error || r.statusText);
    }
    const arrayBuf = await r.arrayBuffer();
    const audioCtx = getAudioCtx();
    if (audioCtx.state === 'suspended') await audioCtx.resume();
    return decodeAdpcmWav(arrayBuf, audioCtx);
  }

  // ── Playback (with distance gain) ───────────────────────────────────────────
  async function play(name: string) {
    if (audioSourceRef.current) {
      try { audioSourceRef.current.stop(); } catch {}
      audioSourceRef.current = null;
    }
    if (playing === name) { setPlaying(null); return; }
    try {
      const decoded = await fetchAndDecode(name);
      setLevels(prev => ({ ...prev, [name]: computeLevel(decoded) }));
      const audioCtx = getAudioCtx();
      const gain = audioCtx.createGain();
      gain.gain.value = Math.max(0, 1 - distance / 500);
      gainNodeRef.current = gain;
      const source = audioCtx.createBufferSource();
      source.buffer = decoded;
      source.connect(gain); gain.connect(audioCtx.destination);
      source.start();
      audioSourceRef.current = source;
      setPlaying(name);
      source.onended = () => { setPlaying(null); audioSourceRef.current = null; };
    } catch (e: any) { setPlaying(null); setError(`Could not play ${name}: ${e.message}`); }
  }

  async function toggleLoop(name: string) {
    if (loopingSourceRef.current) {
      try { loopingSourceRef.current.stop(); } catch {}
      loopingSourceRef.current = null; setLooping(null);
      if (looping === name) return;
    }
    try {
      const decoded = await fetchAndDecode(name);
      setLevels(prev => ({ ...prev, [name]: computeLevel(decoded) }));
      const audioCtx = getAudioCtx();
      const gain = audioCtx.createGain();
      gain.gain.value = Math.max(0, 1 - distance / 500);
      gainNodeRef.current = gain;
      const source = audioCtx.createBufferSource();
      source.buffer = decoded; source.loop = true;
      source.connect(gain); gain.connect(audioCtx.destination);
      source.start();
      loopingSourceRef.current = source; setLooping(name);
    } catch (e: any) { setError(`Could not loop ${name}: ${e.message}`); }
  }

  // ── Music playback ──────────────────────────────────────────────────────────
  async function playMusic(name: string) {
    if (musicSourceRef.current) {
      try { musicSourceRef.current.stop(); } catch {}
      musicSourceRef.current = null; setPlayingMusic(null);
      if (playingMusic === name) return;
    }
    try {
      const r = await fetch(`/api/sounds/music/${encodeURIComponent(name)}`, {
        headers: { Authorization: `Bearer ${getToken()}` },
      });
      if (!r.ok) throw new Error(r.statusText);
      const arrayBuf = await r.arrayBuffer();
      const audioCtx = getAudioCtx();
      if (audioCtx.state === 'suspended') await audioCtx.resume();
      // MP3/OGG decode natively via browser
      const decoded = await audioCtx.decodeAudioData(arrayBuf);
      const source = audioCtx.createBufferSource();
      source.buffer = decoded; source.loop = true;
      source.connect(audioCtx.destination); source.start();
      musicSourceRef.current = source; setPlayingMusic(name);
    } catch (e: any) { setError(`Could not play music ${name}: ${e.message}`); }
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
      setStatus(`Staged ${name}`); load();
    } catch (e: any) { setError(e.message); setStatus(''); }
  }
  function onFileInput(e: React.ChangeEvent<HTMLInputElement>) {
    Array.from(e.target.files || []).forEach(uploadFile); e.target.value = '';
  }
  function onDrop(e: DragEvent<HTMLDivElement>) {
    e.preventDefault(); setDragOver(false);
    Array.from(e.dataTransfer.files).forEach(uploadFile);
  }

  // ── Delete / Restore / Rename / Repack ─────────────────────────────────────
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
  function openRename(name: string) { setRenameTarget(name); setRenameValue(name); }
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
      if (result.updatedActors.length) msg += `. Updated: ${result.updatedActors.join(', ')}`;
      if (result.cppWarning) msg += '. ⚠ C++ source needs manual update!';
      setStatus(msg); setRenameTarget(null); load();
    } catch (e: any) { setError(e.message); }
    finally { setRenaming(false); }
  }
  async function repack() {
    if (!confirm('Rebuild sound.bin now?')) return;
    setRepacking(true); setStatus('Repacking…'); setError('');
    try {
      const result = await apiFetch('/sounds/repack', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ normalize }),
      }) as { numsounds: number; totalSize: number };
      setStatus(`Repacked — ${result.numsounds} sounds, ${formatBytes(result.totalSize)}${normalize ? ' (normalized)' : ''}`);
      load();
    } catch (e: any) { setError(e.message); setStatus(''); }
    finally { setRepacking(false); }
  }

  // ── Category helpers ────────────────────────────────────────────────────────
  function toggleCat(cat: string) {
    setCollapsedCats(prev => {
      const next = new Set(prev);
      next.has(cat) ? next.delete(cat) : next.add(cat);
      return next;
    });
  }

  // ── Derived ─────────────────────────────────────────────────────────────────
  const staged = sounds.filter(s => s.source === 'staged');
  const pendingDels = sounds.filter(s => s.pendingDelete);
  const hasPending = staged.length > 0 || pendingDels.length > 0;

  const missingNames = Object.entries(refs)
    .filter(([, r]) => !r.inBin && (r.cpp || r.actordefs.length > 0))
    .map(([name]) => name).sort();

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
    if (filter === 'ambient') return !!(ref?.role) || !!(ref?.loop);
    return true;
  }

  const visibleEntries = allEntries.filter(matchesFilter);
  const visibleNonDeleted = visibleEntries.filter(s => !s.pendingDelete);
  const selectedSound = visibleNonDeleted[selectedIdx];
  const selectedRef = selectedSound ? refs[selectedSound.name] : null;
  const selectedLevel = selectedSound ? levels[selectedSound.name] : null;

  // Build grouped entries for category view
  type GroupRow = { type: 'header'; cat: string } | { type: 'sound'; entry: SoundEntry & { missing?: boolean } };
  function buildGroupedRows(): GroupRow[] {
    const rows: GroupRow[] = [];
    const catMap = new Map<string, (SoundEntry & { missing?: boolean })[]>();
    const uncategorized: (SoundEntry & { missing?: boolean })[] = [];
    for (const e of visibleEntries) {
      const cat = refs[e.name]?.category || null;
      if (cat) {
        if (!catMap.has(cat)) catMap.set(cat, []);
        catMap.get(cat)!.push(e);
      } else {
        uncategorized.push(e);
      }
    }
    for (const cat of CATEGORY_ORDER) {
      const items = catMap.get(cat);
      if (!items?.length) continue;
      rows.push({ type: 'header', cat });
      if (!collapsedCats.has(cat)) items.forEach(e => rows.push({ type: 'sound', entry: e }));
    }
    if (uncategorized.length) {
      rows.push({ type: 'header', cat: '__other' });
      if (!collapsedCats.has('__other')) uncategorized.forEach(e => rows.push({ type: 'sound', entry: e }));
    }
    return rows;
  }

  // Computed volume at current distance
  const distanceGain = Math.max(0, 1 - distance / 500);

  // ── Render ──────────────────────────────────────────────────────────────────
  return (
    <div style={{ display: 'flex', height: '100vh', fontFamily: 'monospace', background: '#111', color: '#ccc' }}>
      <Sidebar />

      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>

        {/* Header */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 10, padding: '8px 14px', borderBottom: '1px solid #333', background: '#151515', flexWrap: 'wrap' }}>
          <span style={{ fontSize: 15, color: '#aaa', fontWeight: 'bold' }}>[ SOUND STUDIO ]</span>

          {/* Tab switcher */}
          <div style={{ display: 'flex', gap: 2, marginLeft: 6 }}>
            {(['sounds','music'] as TabMode[]).map(t => (
              <button key={t} onClick={() => setTab(t)}
                style={{ padding: '2px 10px', fontSize: 11, fontFamily: 'monospace',
                  background: tab === t ? '#2a2a3a' : 'transparent',
                  border: `1px solid ${tab === t ? '#66a' : '#333'}`,
                  color: tab === t ? '#aaf' : '#666', borderRadius: 3, cursor: 'pointer' }}>
                {t}
              </button>
            ))}
          </div>

          {tab === 'sounds' && hasPending && (
            <span style={{ background: '#f90', color: '#000', padding: '1px 7px', borderRadius: 4, fontSize: 10 }}>
              {staged.length > 0 && `+${staged.length}`}{staged.length > 0 && pendingDels.length > 0 && ' '}{pendingDels.length > 0 && `-${pendingDels.length}`}
            </span>
          )}

          <div style={{ flex: 1 }} />

          {tab === 'sounds' && <>
            <label style={{ display: 'flex', alignItems: 'center', gap: 5, fontSize: 11, color: '#777', cursor: 'pointer' }}>
              <input type="checkbox" checked={normalize} onChange={e => setNormalize(e.target.checked)} />
              normalize
            </label>
            <button onClick={() => fileInputRef.current?.click()}
              style={{ padding: '3px 9px', background: '#333', color: '#ccc', border: '1px solid #555', borderRadius: 4, cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}>
              + Upload WAV
            </button>
            <input ref={fileInputRef} type="file" accept=".wav,audio/*" multiple style={{ display: 'none' }} onChange={onFileInput} />
            <button onClick={repack} disabled={repacking}
              style={{ padding: '3px 12px', fontSize: 11,
                background: hasPending ? '#4a8' : '#555',
                color: hasPending ? '#fff' : '#999',
                border: `1px solid ${hasPending ? '#6ca' : '#666'}`,
                borderRadius: 4, cursor: repacking ? 'wait' : 'pointer', fontFamily: 'monospace', fontWeight: 'bold' }}>
              {repacking ? 'Repacking…' : '⚡ Repack'}
            </button>
          </>}
        </div>

        {/* Status bar */}
        {(status || error) && (
          <div style={{ padding: '4px 14px', fontSize: 11, background: error ? '#200' : '#1a1a1a', color: error ? '#f66' : '#8c8', borderBottom: '1px solid #222' }}>
            {error || status}
            <button onClick={() => { setError(''); setStatus(''); }}
              style={{ marginLeft: 8, background: 'none', border: 'none', color: '#555', cursor: 'pointer', fontFamily: 'monospace' }}>×</button>
          </div>
        )}

        {/* ── SOUNDS TAB ───────────────────────────────────────────────────── */}
        {tab === 'sounds' && <>

          {/* Missing banner */}
          {missingNames.length > 0 && filter !== 'missing' && (
            <div style={{ padding: '4px 14px', fontSize: 11, background: '#2a1400', color: '#f90', borderBottom: '1px solid #333', display: 'flex', alignItems: 'center', gap: 10 }}>
              <span>⚠ {missingNames.length} sound{missingNames.length > 1 ? 's' : ''} referenced by game but missing from sound.bin</span>
              <button onClick={() => setFilter('missing')}
                style={{ background: 'none', border: '1px solid #f90', color: '#f90', borderRadius: 3, padding: '0 5px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                show missing
              </button>
            </div>
          )}

          {/* Filter / search / group bar */}
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, padding: '5px 14px', borderBottom: '1px solid #222', background: '#131313', flexWrap: 'wrap' }}>
            <input type="text" placeholder="search…" value={search} onChange={e => setSearch(e.target.value)}
              style={{ padding: '2px 7px', background: '#222', border: '1px solid #444', borderRadius: 3, color: '#ccc', fontFamily: 'monospace', fontSize: 11, width: 140 }} />
            {(['all','cpp','actordef','ambient','orphaned','missing'] as FilterMode[]).map(f => (
              <button key={f} onClick={() => setFilter(f)}
                style={{ padding: '1px 7px', fontSize: 10, fontFamily: 'monospace',
                  background: filter === f ? '#2a3a4a' : 'transparent',
                  border: `1px solid ${filter === f ? '#4af' : '#2a2a2a'}`,
                  color: filter === f ? '#8cf' : '#555', borderRadius: 3, cursor: 'pointer' }}>
                {f}{f === 'missing' && missingNames.length > 0 ? ` (${missingNames.length})` : ''}
              </button>
            ))}
            <button onClick={() => setGroupByCategory(g => !g)}
              style={{ padding: '1px 7px', fontSize: 10, fontFamily: 'monospace',
                background: groupByCategory ? '#2a3a2a' : 'transparent',
                border: `1px solid ${groupByCategory ? '#4a8' : '#2a2a2a'}`,
                color: groupByCategory ? '#8c8' : '#555', borderRadius: 3, cursor: 'pointer' }}>
              group
            </button>
            <span style={{ marginLeft: 'auto', color: '#333', fontSize: 10 }}>{visibleEntries.length} shown</span>
          </div>

          {/* Main: list + inspector */}
          <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>

            {/* Sound list */}
            <div style={{ flex: 1, overflow: 'auto', position: 'relative' }}
              onDragOver={e => { e.preventDefault(); setDragOver(true); }}
              onDragLeave={() => setDragOver(false)} onDrop={onDrop}>
              {dragOver && (
                <div style={{ position: 'absolute', inset: 0, background: 'rgba(80,200,120,0.12)', border: '2px dashed #4a8', zIndex: 10, display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: 18, color: '#4a8', pointerEvents: 'none' }}>
                  Drop WAV files to stage
                </div>
              )}
              {loading ? <div style={{ color: '#555', padding: 40, textAlign: 'center' }}>Loading…</div> : (
                <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 11 }}>
                  <thead>
                    <tr style={{ color: '#444', borderBottom: '1px solid #2a2a2a', position: 'sticky', top: 0, background: '#131313' }}>
                      <th style={{ textAlign: 'left', padding: '4px 5px', width: 20 }}></th>
                      <th style={{ textAlign: 'left', padding: '4px 5px' }}>Name</th>
                      <th style={{ textAlign: 'left', padding: '4px 5px', width: 72 }}>Refs</th>
                      <th style={{ textAlign: 'left', padding: '4px 5px', width: 110 }}>Level</th>
                      <th style={{ textAlign: 'right', padding: '4px 5px', width: 62 }}>Size</th>
                      <th style={{ textAlign: 'right', padding: '4px 5px', width: 98 }}></th>
                    </tr>
                  </thead>
                  <tbody>
                    {(() => {
                      const rows = groupByCategory ? buildGroupedRows() : visibleEntries.map(e => ({ type: 'sound' as const, entry: e }));
                      return rows.map((row, ri) => {
                        if (row.type === 'header') {
                          const cat = row.cat;
                          const label = cat === '__other' ? 'Uncategorized' : CATEGORY_LABELS[cat] || cat;
                          const collapsed = collapsedCats.has(cat);
                          return (
                            <tr key={`cat-${cat}`} onClick={() => toggleCat(cat)} style={{ cursor: 'pointer', background: '#181818', borderBottom: '1px solid #2a2a2a' }}>
                              <td colSpan={6} style={{ padding: '5px 8px', fontSize: 11, color: '#8af', fontWeight: 'bold' }}>
                                {collapsed ? '▶' : '▼'} {label}
                              </td>
                            </tr>
                          );
                        }
                        const s = row.entry;
                        const visIdx = visibleNonDeleted.indexOf(s as any);
                        const isPlaying = playing === s.name;
                        const isLooping = looping === s.name;
                        const isSelected = !s.pendingDelete && visIdx >= 0 && visIdx === selectedIdx;
                        const size = s.source === 'bin' ? s.adpcmBytes : s.size;
                        const ref = refs[s.name];
                        const lvl = levels[s.name];
                        const isMissing = !!(s as any).missing;
                        const isLoop = ref?.loop;
                        return (
                          <tr key={s.name}
                            ref={el => { if (!s.pendingDelete && visIdx >= 0) rowRefs.current[visIdx] = el; }}
                            onClick={() => {
                              if (!s.pendingDelete && !isMissing) { setSelectedIdx(visIdx); play(s.name); }
                              else if (!s.pendingDelete) setSelectedIdx(visIdx);
                            }}
                            style={{
                              borderBottom: '1px solid #1a1a1a',
                              opacity: s.pendingDelete ? 0.4 : isMissing ? 0.7 : 1,
                              cursor: s.pendingDelete ? 'default' : 'pointer',
                              background: isMissing ? '#1e1000' : isPlaying ? '#1a2a1a' : isSelected ? '#1e1e28' : s.source === 'staged' ? '#1a1a2a' : 'transparent',
                              outline: isSelected ? '1px solid #445' : 'none',
                            }}>
                            <td style={{ padding: '3px 5px', textAlign: 'center' }}>
                              {!isMissing ? (
                                <button onClick={e => { e.stopPropagation(); if (!s.pendingDelete) { setSelectedIdx(visIdx); play(s.name); } }}
                                  disabled={s.pendingDelete}
                                  style={{ background: 'none', border: 'none', color: isPlaying ? '#4a8' : '#555', cursor: 'pointer', fontSize: 12, padding: 0 }}>
                                  {isPlaying ? '⏹' : '▶'}
                                </button>
                              ) : <span style={{ color: '#f90', fontSize: 10 }}>✗</span>}
                            </td>
                            <td style={{ padding: '3px 5px', color: s.pendingDelete ? '#555' : isMissing ? '#f90' : '#ddd' }}>
                              {s.name}
                              {s.pendingDelete && <span style={{ marginLeft: 5, color: '#f66', fontSize: 9 }}>[del]</span>}
                              {s.source === 'staged' && <span style={{ marginLeft: 5, color: '#88f', fontSize: 9 }}>[staged]</span>}
                              {isMissing && <span style={{ marginLeft: 5, color: '#f90', fontSize: 9 }}>[missing]</span>}
                              {ref?.role && <span style={{ marginLeft: 5, color: '#8af', fontSize: 9 }}>[{ref.role}]</span>}
                              {!ref?.role && isLoop && <span style={{ marginLeft: 5, color: '#68a', fontSize: 9 }}>[loop]</span>}
                            </td>
                            <td style={{ padding: '3px 5px' }}>
                              <span style={{ display: 'inline-flex', gap: 2 }}>
                                {ref?.cpp && <span title="Referenced in C++" style={{ background: '#2a3a2a', border: '1px solid #3a5a3a', color: '#8c8', borderRadius: 2, padding: '0 3px', fontSize: 9 }}>C++</span>}
                                {ref?.actordefs?.length > 0 && <span title={ref.actordefs.join(', ')} style={{ background: '#1a2a3a', border: '1px solid #2a4a6a', color: '#68a', borderRadius: 2, padding: '0 3px', fontSize: 9 }}>ADef</span>}
                                {!ref?.cpp && !ref?.actordefs?.length && !isMissing && <span style={{ color: '#333', fontSize: 9 }}>—</span>}
                              </span>
                            </td>
                            <td style={{ padding: '3px 5px' }}>
                              {lvl ? (
                                <div style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
                                  <div style={{ width: 70, height: 5, background: '#222', borderRadius: 2, overflow: 'hidden' }}>
                                    <div style={{ width: `${Math.round(lvl.peak * 100)}%`, height: '100%', background: lvl.peak > 0.9 ? '#f44' : lvl.peak > 0.6 ? '#fa4' : '#4a8', borderRadius: 2 }} />
                                  </div>
                                  <span style={{ color: '#444', fontSize: 9 }}>{Math.round(lvl.peak * 100)}%</span>
                                </div>
                              ) : <span style={{ color: '#2a2a2a', fontSize: 9 }}>—</span>}
                            </td>
                            <td style={{ padding: '3px 5px', textAlign: 'right', color: '#444', fontVariantNumeric: 'tabular-nums' }}>{formatBytes(size)}</td>
                            <td style={{ padding: '3px 5px', textAlign: 'right' }}>
                              <span style={{ display: 'inline-flex', gap: 3 }}>
                                {isLoop && !isMissing && (
                                  <button onClick={e => { e.stopPropagation(); toggleLoop(s.name); }}
                                    title={isLooping ? 'Stop loop' : 'Loop test'}
                                    style={{ background: isLooping ? '#1a2a3a' : 'none', border: `1px solid ${isLooping ? '#4af' : '#2a2a2a'}`, color: isLooping ? '#8cf' : '#556', borderRadius: 3, padding: '0 4px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                                    {isLooping ? '⏹↺' : '↺'}
                                  </button>
                                )}
                                {!isMissing && !s.pendingDelete && (
                                  <button onClick={e => { e.stopPropagation(); openRename(s.name); }}
                                    style={{ background: 'none', border: '1px solid #2a2a2a', color: '#666', borderRadius: 3, padding: '0 4px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>✎</button>
                                )}
                                {s.pendingDelete ? (
                                  <button onClick={e => { e.stopPropagation(); restoreSnd(s.name); }}
                                    style={{ background: 'none', border: '1px solid #555', color: '#8a8', borderRadius: 3, padding: '0 5px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>Restore</button>
                                ) : !isMissing ? (
                                  <button onClick={e => { e.stopPropagation(); deleteSnd(s.name); }}
                                    style={{ background: 'none', border: '1px solid #2a2a2a', color: '#844', borderRadius: 3, padding: '0 5px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>✕</button>
                                ) : null}
                              </span>
                            </td>
                          </tr>
                        );
                      });
                    })()}
                  </tbody>
                </table>
              )}
              {!loading && visibleEntries.length === 0 && (
                <div style={{ color: '#444', textAlign: 'center', padding: 50, fontSize: 12 }}>No sounds match filter.</div>
              )}
            </div>

            {/* Inspector sidebar */}
            {selectedSound && (
              <div style={{ width: 252, borderLeft: '1px solid #1e1e1e', background: '#131313', display: 'flex', flexDirection: 'column', overflow: 'hidden', flexShrink: 0 }}>

                {/* Sound name + basic info */}
                <div style={{ padding: '8px 10px', borderBottom: '1px solid #1e1e1e' }}>
                  <div style={{ fontSize: 11, color: '#aaa', fontWeight: 'bold', wordBreak: 'break-all' }}>{selectedSound.name}</div>
                  {selectedRef?.role && <div style={{ marginTop: 3, fontSize: 10, color: '#8af' }}>🌐 {selectedRef.role === 'BG_BASE' ? 'Base ambient (indoor)' : selectedRef.role === 'BG_AMBIENT' ? 'Ambient hum' : 'Outside wind'}</div>}
                  {!selectedRef?.role && selectedRef?.loop && <div style={{ marginTop: 3, fontSize: 10, color: '#68a' }}>↺ Looping sound in game</div>}
                  {selectedRef?.category && <div style={{ marginTop: 2, fontSize: 10, color: '#666' }}>Category: {CATEGORY_LABELS[selectedRef.category] || selectedRef.category}</div>}
                  {selectedLevel && (
                    <div style={{ marginTop: 4, fontSize: 10, color: '#555', display: 'flex', gap: 10 }}>
                      <span>Peak <span style={{ color: selectedLevel.peak > 0.9 ? '#f44' : '#9a9' }}>{(selectedLevel.peak * 100).toFixed(0)}%</span></span>
                      <span>RMS <span style={{ color: '#9a9' }}>{(selectedLevel.rms * 100).toFixed(0)}%</span></span>
                    </div>
                  )}
                </div>

                {/* Waveform */}
                <div style={{ padding: '6px 10px', borderBottom: '1px solid #1e1e1e' }}>
                  <canvas ref={waveformCanvasRef} width={232} height={44}
                    style={{ width: '100%', height: 44, display: 'block', borderRadius: 2, background: '#1a2a1a' }} />
                </div>

                {/* Distance / volume preview */}
                <div style={{ padding: '7px 10px', borderBottom: '1px solid #1e1e1e' }}>
                  <div style={{ fontSize: 10, color: '#555', marginBottom: 4 }}>DISTANCE PREVIEW</div>
                  <input type="range" min={0} max={500} value={distance} onChange={e => setDistance(Number(e.target.value))}
                    style={{ width: '100%', accentColor: '#4a8' }} />
                  <div style={{ fontSize: 10, color: '#666', display: 'flex', justifyContent: 'space-between', marginTop: 2 }}>
                    <span>{distance}px from source</span>
                    <span style={{ color: distanceGain > 0 ? '#9a9' : '#f66' }}>
                      {distanceGain > 0 ? `${(distanceGain * 100).toFixed(0)}% vol` : 'inaudible'}
                    </span>
                  </div>
                </div>

                {/* Volume map */}
                {selectedRef?.volumeCalls && selectedRef.volumeCalls.length > 0 && (
                  <div style={{ padding: '7px 10px', borderBottom: '1px solid #1e1e1e', overflow: 'auto', maxHeight: 160 }}>
                    <div style={{ fontSize: 10, color: '#555', marginBottom: 4 }}>VOLUME IN GAME</div>
                    {selectedRef.volumeCalls.map((vc, i) => (
                      <div key={i} style={{ display: 'flex', justifyContent: 'space-between', fontSize: 10, padding: '2px 0', borderBottom: '1px solid #1a1a1a' }}>
                        <span style={{ color: '#888', flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{vc.ctx}</span>
                        <span style={{ color: volColor(vc.vol), marginLeft: 6, flexShrink: 0, fontVariantNumeric: 'tabular-nums' }}>{vc.vol}/128</span>
                      </div>
                    ))}
                  </div>
                )}

                {/* Actordef usage */}
                {selectedRef?.actordefs && selectedRef.actordefs.length > 0 && (
                  <div style={{ padding: '7px 10px', borderBottom: '1px solid #1e1e1e' }}>
                    <div style={{ fontSize: 10, color: '#555', marginBottom: 4 }}>USED BY ACTORS</div>
                    {selectedRef.actordefs.map(actor => (
                      <div key={actor} style={{ fontSize: 10, color: '#68a', padding: '2px 0' }}>{actor}</div>
                    ))}
                  </div>
                )}

                {/* No refs */}
                {selectedRef && !selectedRef.cpp && !selectedRef.actordefs?.length && (
                  <div style={{ padding: '8px 10px', fontSize: 10, color: '#444' }}>
                    No references found — safe to remove.
                  </div>
                )}
              </div>
            )}
          </div>

          {/* Footer */}
          <div style={{ padding: '4px 14px', borderTop: '1px solid #1e1e1e', background: '#151515', fontSize: 10, color: '#444', display: 'flex', gap: 14, flexWrap: 'wrap' }}>
            <span>{sounds.filter(s => s.source === 'bin' && !s.pendingDelete).length} in bin</span>
            {staged.length > 0 && <span style={{ color: '#88f' }}>{staged.length} staged</span>}
            {pendingDels.length > 0 && <span style={{ color: '#f66' }}>{pendingDels.length} pending deletion</span>}
            {missingNames.length > 0 && <span style={{ color: '#f90' }}>{missingNames.length} missing</span>}
            <span>drop WAV to stage</span>
            <span style={{ marginLeft: 'auto', color: '#2a2a2a' }}>↑↓ navigate &amp; play</span>
          </div>
        </>}

        {/* ── MUSIC TAB ────────────────────────────────────────────────────── */}
        {tab === 'music' && (
          <div style={{ flex: 1, overflow: 'auto', padding: 16 }}>
            <div style={{ fontSize: 11, color: '#555', marginBottom: 10 }}>
              Music files in <code>shared/assets/</code> — loaded by the game separately from sound.bin.
            </div>
            {musicLoading ? (
              <div style={{ color: '#555' }}>Loading…</div>
            ) : musicFiles.length === 0 ? (
              <div style={{ color: '#444' }}>No music files found.</div>
            ) : (
              <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 12 }}>
                <thead>
                  <tr style={{ color: '#444', borderBottom: '1px solid #2a2a2a' }}>
                    <th style={{ textAlign: 'left', padding: '5px 8px', width: 28 }}></th>
                    <th style={{ textAlign: 'left', padding: '5px 8px' }}>File</th>
                    <th style={{ textAlign: 'right', padding: '5px 8px', width: 80 }}>Size</th>
                    <th style={{ textAlign: 'left', padding: '5px 8px', width: 100 }}>Role</th>
                  </tr>
                </thead>
                <tbody>
                  {musicFiles.map(f => {
                    const isPlaying = playingMusic === f.name;
                    const role = f.name.toLowerCase().includes('closer') ? 'Menu music' : f.name.toLowerCase().includes('game') ? 'In-game music' : null;
                    return (
                      <tr key={f.name}
                        onClick={() => playMusic(f.name)}
                        style={{ borderBottom: '1px solid #1a1a1a', cursor: 'pointer', background: isPlaying ? '#1a2a1a' : 'transparent' }}>
                        <td style={{ padding: '5px 8px', textAlign: 'center' }}>
                          <button onClick={e => { e.stopPropagation(); playMusic(f.name); }}
                            style={{ background: 'none', border: 'none', color: isPlaying ? '#4a8' : '#666', cursor: 'pointer', fontSize: 14, padding: 0 }}>
                            {isPlaying ? '⏹' : '▶'}
                          </button>
                        </td>
                        <td style={{ padding: '5px 8px', color: '#ddd' }}>{f.name}</td>
                        <td style={{ padding: '5px 8px', textAlign: 'right', color: '#555' }}>{formatBytes(f.size)}</td>
                        <td style={{ padding: '5px 8px', color: '#68a', fontSize: 11 }}>{role || '—'}</td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            )}
            {playingMusic && (
              <div style={{ marginTop: 14, padding: '8px 12px', background: '#1a2a1a', border: '1px solid #3a5a3a', borderRadius: 4, fontSize: 11, color: '#8c8', display: 'flex', alignItems: 'center', gap: 10 }}>
                <span>▶ looping: {playingMusic}</span>
                <button onClick={() => { try { musicSourceRef.current?.stop(); } catch {} musicSourceRef.current = null; setPlayingMusic(null); }}
                  style={{ background: 'none', border: '1px solid #4a8', color: '#8c8', borderRadius: 3, padding: '1px 8px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}>
                  Stop
                </button>
              </div>
            )}
          </div>
        )}
      </div>

      {/* Rename dialog */}
      {renameTarget && (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.7)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 100 }}
          onClick={() => setRenameTarget(null)}>
          <div onClick={e => e.stopPropagation()}
            style={{ background: '#1a1a1a', border: '1px solid #444', borderRadius: 6, padding: 18, width: 320, fontFamily: 'monospace' }}>
            <div style={{ fontSize: 12, color: '#aaa', marginBottom: 10 }}>Rename sound</div>
            <input autoFocus value={renameValue} onChange={e => setRenameValue(e.target.value)}
              onKeyDown={e => { if (e.key === 'Enter') submitRename(); if (e.key === 'Escape') setRenameTarget(null); }}
              style={{ width: '100%', padding: '5px 8px', background: '#222', border: '1px solid #555', borderRadius: 3, color: '#ddd', fontFamily: 'monospace', fontSize: 12, boxSizing: 'border-box' }} />
            {refs[renameTarget]?.cpp && (
              <div style={{ marginTop: 7, fontSize: 10, color: '#f90' }}>⚠ Hardcoded in C++ — source won't be auto-updated.</div>
            )}
            {refs[renameTarget]?.actordefs?.length > 0 && (
              <div style={{ marginTop: 5, fontSize: 10, color: '#8af' }}>✓ Actordefs will be updated: {refs[renameTarget].actordefs.join(', ')}</div>
            )}
            <div style={{ marginTop: 10, display: 'flex', gap: 7, justifyContent: 'flex-end' }}>
              <button onClick={() => setRenameTarget(null)}
                style={{ padding: '4px 10px', background: 'none', border: '1px solid #444', color: '#888', borderRadius: 3, cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}>Cancel</button>
              <button onClick={submitRename} disabled={renaming}
                style={{ padding: '4px 10px', background: '#2a3a4a', border: '1px solid #4af', color: '#8cf', borderRadius: 3, cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}>
                {renaming ? 'Renaming…' : 'Rename'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
