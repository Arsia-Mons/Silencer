'use client';
import { useEffect, useRef, useState, useCallback, DragEvent } from 'react';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { apiFetch } from '../../lib/api';
import { useServerReachable } from '../../lib/socket';
import { decodeAdpcmWav } from './adpcm';

interface SoundEntry {
  name: string;
  storedLength: number | null;
  adpcmBytes: number | null;
  durationSec?: number;
  size?: number;
  source: 'bin' | 'staged';
  pendingDelete: boolean;
  pendingRenameTo: string | null;
}

interface SoundRef {
  inBin: boolean;
  cpp: boolean;
  actordefs: string[];
  role: string | null;
  loop: boolean;
  category: string | null;
  volumeCalls: { ctx: string; vol: number | string }[];
  fadeoutMs: number | null;
  soundSet: string | null;
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

interface PendingDiff {
  added: string[];
  modified: string[];
  deleted: string[];
  renamed: { from: string; to: string }[];
}

type FilterMode = 'all' | 'cpp' | 'actordef' | 'orphaned' | 'missing' | 'ambient' | 'headroom' | 'loop' | 'attenuated' | 'ui';
type TabMode = 'sounds' | 'music' | 'ambient';
type SortKey = 'name' | 'size' | 'duration' | 'level' | 'refs';

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

function formatDuration(sec: number | undefined | null): string {
  if (sec == null) return '—';
  if (sec < 1) return `${Math.round(sec * 1000)}ms`;
  return `${sec.toFixed(2)}s`;
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
  const serverReachable = useServerReachable();

  // ── State ───────────────────────────────────────────────────────────────────
  const [tab, setTab] = useState<TabMode>('sounds');
  const [sounds, setSounds] = useState<SoundEntry[]>([]);
  const [refs, setRefs] = useState<Record<string, SoundRef>>({});
  const [levels, setLevels] = useState<Record<string, LevelInfo>>({});
  const [binLoaded, setBinLoaded] = useState(false);
  const [loading, setLoading] = useState(false);
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
  const [inGameVol, setInGameVol] = useState(true);
  const [sortKey, setSortKey] = useState<SortKey | null>(null);
  const [sortDir, setSortDir] = useState<'asc' | 'desc'>('asc');

  // A→B compare
  const [compareA, setCompareA] = useState<string | null>(null);
  const [compareB, setCompareB] = useState<string | null>(null);
  const [comparePlaying, setComparePlaying] = useState<'A' | 'B' | null>(null);
  const compareSeqRef = useRef(0);

  // Repack diff
  const [diffData, setDiffData] = useState<PendingDiff | null>(null);
  const [showDiff, setShowDiff] = useState(false);
  const [diffLoading, setDiffLoading] = useState(false);

  // BG channel mixer (ambient tab)
  const [bgRunning, setBgRunning] = useState(false);
  const [bgIndoor, setBgIndoor] = useState(true);
  const [bgRatio, setBgRatio] = useState(0);
  const [bgMutes, setBgMutes] = useState([false, false, false]);

  // Volume call-site picker (which CPP_VOLUME_MAP entry to use for preview/play)
  const [selectedVolCtx, setSelectedVolCtx] = useState(0);
  const selectedVolCtxRef = useRef(0);

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
  const loopingGainRef = useRef<GainNode | null>(null);
  const musicSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const [playing, setPlaying] = useState<string | null>(null);
  const [looping, setLooping] = useState<string | null>(null);
  const [selectedIdx, setSelectedIdx] = useState<number>(-1);
  const [multiSel, setMultiSel] = useState<Set<string>>(new Set());
  const fileInputRef = useRef<HTMLInputElement | null>(null);
  const binInputRef = useRef<HTMLInputElement | null>(null);
  const rowRefs = useRef<(HTMLTableRowElement | null)[]>([]);
  const soundsRef = useRef<SoundEntry[]>([]);
  const visibleNonDeletedRef = useRef<(SoundEntry & { missing?: boolean })[]>([]);
  const waveformCanvasRef = useRef<HTMLCanvasElement | null>(null);
  const compareSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const bgSourcesRef = useRef<(AudioBufferSourceNode | null)[]>([null, null, null]);
  const bgGainsRef = useRef<(GainNode | null)[]>([null, null, null]);
  const decodedCacheRef = useRef<Map<string, AudioBuffer>>(new Map());

  useEffect(() => { selectedVolCtxRef.current = selectedVolCtx; }, [selectedVolCtx]);

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
      setBinLoaded(true);
    } catch (e: any) { setError(e.message); }
    finally { setLoading(false); }
  }, []);

  const handleBinFile = useCallback(async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    setLoading(true); setError('');
    try {
      const buf = await file.arrayBuffer();
      const token = getToken();
      const res = await fetch(`${(process.env.NEXT_PUBLIC_API_URL || '') + '/api'}/sounds/upload`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/octet-stream',
          ...(token ? { Authorization: `Bearer ${token}` } : {}),
        },
        body: buf,
      });
      if (!res.ok) throw new Error(`Upload failed: ${res.status} ${res.statusText}`);
      await load();
    } catch (e: any) { setError(e.message); setLoading(false); }
    if (e.target) e.target.value = '';
  }, [load]);

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
    if (!gainNodeRef.current) return;
    const activeName = playing || looping;
    let base = 1.0;
    if (inGameVol && activeName) {
      const vol = refs[activeName]?.volumeCalls?.[0]?.vol;
      if (typeof vol === 'number') base = vol / 128;
    }
    gainNodeRef.current.gain.value = Math.max(0, 1 - distance / 500) * base;
  }, [distance, inGameVol, playing, looping, refs]);

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

    // Game volume ceiling line
    const volCalls = refs[sel.name]?.volumeCalls;
    const gameVol = volCalls?.length ? (typeof volCalls[0].vol === 'number' ? volCalls[0].vol as number : 128) : 128;
    const volScale = gameVol / 128;
    if (volScale < 1) {
      const ceiling = mid * volScale;
      ctx.strokeStyle = '#2a4a2a'; ctx.lineWidth = 1; ctx.setLineDash([3, 3]);
      ctx.beginPath(); ctx.moveTo(0, mid - ceiling); ctx.lineTo(w, mid - ceiling); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(0, mid + ceiling); ctx.lineTo(w, mid + ceiling); ctx.stroke();
      ctx.setLineDash([]);
      ctx.fillStyle = '#3a6a3a'; ctx.font = '9px monospace';
      ctx.fillText(`${gameVol}/128`, 3, mid - ceiling - 2);
    }

    ctx.strokeStyle = '#4a8'; ctx.lineWidth = 1;
    ctx.beginPath();
    for (let i = 0; i < lvl.waveform.length; i++) {
      const x = (i / lvl.waveform.length) * w;
      const amp = lvl.waveform[i] * mid;
      ctx.moveTo(x, mid - amp); ctx.lineTo(x, mid + amp);
    }
    ctx.stroke();

    // Time axis ticks (every 500ms)
    const dur = (sel as any).durationSec ?? 0;
    if (dur > 0) {
      ctx.strokeStyle = '#2a3a2a'; ctx.lineWidth = 1; ctx.setLineDash([]);
      ctx.fillStyle = '#3a5a3a'; ctx.font = '8px monospace';
      for (let t = 0.5; t < dur; t += 0.5) {
        const x = Math.round((t / dur) * w);
        ctx.beginPath(); ctx.moveTo(x, h - 6); ctx.lineTo(x, h); ctx.stroke();
        if (x > 12) ctx.fillText(`${t.toFixed(1)}`, x + 2, h - 1);
      }
    }

    // Loop indicator
    const isLoop = refs[sel.name]?.loop;
    if (isLoop) {
      ctx.fillStyle = '#1a3a4a';
      ctx.fillRect(w - 34, 1, 33, 11);
      ctx.fillStyle = '#4a9a9a'; ctx.font = '8px monospace';
      ctx.fillText('↻ LOOP', w - 32, 10);
    }
  }, [levels, selectedIdx, sounds, refs]);

  // ── Arrow key navigation + Space to play ──────────────────────────────────
  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === 'INPUT' || tag === 'BUTTON' || tag === 'SELECT') return;
      const list = visibleNonDeletedRef.current;
      if (!list.length) return;
      if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
        e.preventDefault();
        setSelectedIdx(prev => {
          const next = e.key === 'ArrowDown'
            ? Math.min(prev + 1, list.length - 1)
            : Math.max(prev - 1, 0);
          rowRefs.current[next]?.scrollIntoView({ block: 'nearest' });
          setSelectedVolCtx(0);
          play(list[next].name);
          return next;
        });
      } else if (e.key === ' ') {
        e.preventDefault();
        setSelectedIdx(prev => {
          if (prev >= 0 && prev < list.length) play(list[prev].name);
          return prev;
        });
      }
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
    if (decodedCacheRef.current.has(name)) return decodedCacheRef.current.get(name)!;
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
    const decoded = await decodeAdpcmWav(arrayBuf, audioCtx);
    decodedCacheRef.current.set(name, decoded);
    return decoded;
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
      const calls = refs[name]?.volumeCalls;
      const vol = calls?.[selectedVolCtxRef.current]?.vol ?? calls?.[0]?.vol;
      const base = inGameVol && typeof vol === 'number' ? vol / 128 : 1.0;
      gain.gain.value = Math.max(0, 1 - distance / 500) * base;
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
      // Simulate game fadeout on stop if this sound has a fadeoutMs
      const fadeMs = refs[looping ?? '']?.fadeoutMs ?? 0;
      const src = loopingSourceRef.current;
      const gain = loopingGainRef.current;
      if (fadeMs && gain) {
        const audioCtx = getAudioCtx();
        const now = audioCtx.currentTime;
        gain.gain.setValueAtTime(gain.gain.value, now);
        gain.gain.linearRampToValueAtTime(0, now + fadeMs / 1000);
        setTimeout(() => { try { src.stop(); } catch {} }, fadeMs + 50);
      } else {
        try { src.stop(); } catch {}
      }
      loopingSourceRef.current = null; loopingGainRef.current = null; setLooping(null);
      if (looping === name) return;
    }
    try {
      const decoded = await fetchAndDecode(name);
      setLevels(prev => ({ ...prev, [name]: computeLevel(decoded) }));
      const audioCtx = getAudioCtx();
      const gain = audioCtx.createGain();
      const calls = refs[name]?.volumeCalls;
      const vol = calls?.[selectedVolCtxRef.current]?.vol ?? calls?.[0]?.vol;
      const base = inGameVol && typeof vol === 'number' ? vol / 128 : 1.0;
      gain.gain.value = Math.max(0, 1 - distance / 500) * base;
      gainNodeRef.current = gain; loopingGainRef.current = gain;
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
  async function clearStaged() {
    const toRemove = sounds.filter(s => s.source === 'staged');
    if (!toRemove.length) return;
    if (!confirm(`Remove ${toRemove.length} staged upload${toRemove.length > 1 ? 's' : ''}? Bin sounds are unaffected.`)) return;
    for (const s of toRemove) {
      try { await apiFetch(`/sounds/${encodeURIComponent(s.name)}`, { method: 'DELETE' }); } catch {}
    }
    setStatus(`Cleared ${toRemove.length} staged files`);
    load();
  }

  async function repack() {
    if (!confirm('Rebuild sound.bin now? The new file will download automatically.')) return;
    setRepacking(true); setStatus('Repacking…'); setError('');
    try {
      const r = await fetch('/api/sounds/repack', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${getToken()}` },
        body: JSON.stringify({ normalize }),
      });
      if (!r.ok) {
        const err = await r.json().catch(() => ({ error: r.statusText })) as { error?: string };
        throw new Error(err.error || r.statusText);
      }
      const blob = await r.blob();
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url; a.download = 'sound.bin'; a.click();
      URL.revokeObjectURL(url);
      setStatus(`Repacked — downloaded sound.bin${normalize ? ' (normalized)' : ''}`);
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

  // ── BG Channel Mixer ────────────────────────────────────────────────────────
  const BG_CHANNELS = ['wndloopb.wav', 'cphum11.wav', 'wndloop1.wav'] as const;

  function computeBgGains(indoor: boolean, ratio: number, mutes: boolean[]): number[] {
    const raw = indoor
      ? [32 / 128, 0, 0]
      : [0, (8 / 128) * (1 - ratio), (8 / 128) * ratio];
    return raw.map((g, i) => mutes[i] ? 0 : g);
  }

  function applyBgGains(gains: number[]) {
    bgGainsRef.current.forEach((g, i) => { if (g) g.gain.value = gains[i]; });
  }

  useEffect(() => {
    if (bgRunning) applyBgGains(computeBgGains(bgIndoor, bgRatio, bgMutes));
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [bgIndoor, bgRatio, bgMutes, bgRunning]);

  async function startBgMixer() {
    const audioCtx = getAudioCtx();
    if (audioCtx.state === 'suspended') await audioCtx.resume();
    try {
      const buffers = await Promise.all(BG_CHANNELS.map(n => fetchAndDecode(n)));
      const gains = computeBgGains(bgIndoor, bgRatio, bgMutes);
      const startAt = audioCtx.currentTime + 0.1;
      BG_CHANNELS.forEach((_, i) => {
        const gain = audioCtx.createGain();
        gain.gain.value = gains[i];
        bgGainsRef.current[i] = gain;
        const src = audioCtx.createBufferSource();
        src.buffer = buffers[i]; src.loop = true;
        src.connect(gain); gain.connect(audioCtx.destination);
        src.start(startAt);
        bgSourcesRef.current[i] = src;
      });
      setBgRunning(true);
    } catch (e: any) { setError(`BG mixer: ${e.message}`); }
  }

  function stopBgMixer() {
    bgSourcesRef.current.forEach(s => { try { s?.stop(); } catch {} });
    bgSourcesRef.current = [null, null, null];
    bgGainsRef.current = [null, null, null];
    setBgRunning(false);
  }

  // Stop BG mixer when leaving ambient tab
  useEffect(() => {
    if (tab !== 'ambient' && bgRunning) stopBgMixer();
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tab]);

  // ── A→B Compare ─────────────────────────────────────────────────────────────
  function stopCompare() {
    try { compareSourceRef.current?.stop(); } catch {}
    compareSourceRef.current = null;
    setComparePlaying(null);
    compareSeqRef.current++;
  }

  async function playCompareSlot(slot: 'A' | 'B') {
    const name = slot === 'A' ? compareA : compareB;
    if (!name) return;
    stopCompare();
    const seq = ++compareSeqRef.current;
    try {
      const decoded = await fetchAndDecode(name);
      if (compareSeqRef.current !== seq) return;
      const audioCtx = getAudioCtx();
      const gain = audioCtx.createGain();
      gain.gain.value = 1;
      const src = audioCtx.createBufferSource();
      src.buffer = decoded;
      src.connect(gain); gain.connect(audioCtx.destination);
      src.start();
      compareSourceRef.current = src;
      setComparePlaying(slot);
      src.onended = () => {
        if (compareSeqRef.current === seq) { setComparePlaying(null); compareSourceRef.current = null; }
      };
    } catch (e: any) { setError(`Compare: ${e.message}`); }
  }

  async function playAtoB() {
    if (!compareA || !compareB) return;
    stopCompare();
    const seq = ++compareSeqRef.current;
    try {
      const decodedA = await fetchAndDecode(compareA);
      if (compareSeqRef.current !== seq) return;
      const audioCtx = getAudioCtx();
      const gain = audioCtx.createGain();
      gain.gain.value = 1;
      const srcA = audioCtx.createBufferSource();
      srcA.buffer = decodedA;
      srcA.connect(gain); gain.connect(audioCtx.destination);
      srcA.start();
      compareSourceRef.current = srcA;
      setComparePlaying('A');
      srcA.onended = async () => {
        if (compareSeqRef.current !== seq) return;
        try {
          const decodedB = await fetchAndDecode(compareB!);
          if (compareSeqRef.current !== seq) return;
          const srcB = audioCtx.createBufferSource();
          srcB.buffer = decodedB;
          srcB.connect(gain); gain.connect(audioCtx.destination);
          srcB.start();
          compareSourceRef.current = srcB;
          setComparePlaying('B');
          srcB.onended = () => {
            if (compareSeqRef.current === seq) { setComparePlaying(null); compareSourceRef.current = null; }
          };
        } catch {}
      };
    } catch (e: any) { setError(`Compare: ${e.message}`); }
  }

  // ── Repack Diff ──────────────────────────────────────────────────────────────
  async function fetchDiff() {
    setDiffLoading(true);
    try {
      const data = await apiFetch('/sounds/pending') as PendingDiff;
      setDiffData(data);
      setShowDiff(true);
    } catch (e: any) { setError(e.message); }
    finally { setDiffLoading(false); }
  }

  async function repackFromDiff() {
    setShowDiff(false);
    await repack();
  }

  // ── Sort ────────────────────────────────────────────────────────────────────
  function toggleSort(key: SortKey) {
    if (sortKey === key) setSortDir(d => d === 'asc' ? 'desc' : 'asc');
    else { setSortKey(key); setSortDir('asc'); }
    setSelectedIdx(-1);
  }

  // ── Export decoded WAV ──────────────────────────────────────────────────────
  async function exportWav(name: string) {
    setStatus(`Exporting ${name}…`);
    try {
      const decoded = await fetchAndDecode(name);
      const samples = decoded.getChannelData(0);
      const int16 = new Int16Array(samples.length);
      for (let i = 0; i < samples.length; i++) {
        int16[i] = Math.max(-32768, Math.min(32767, Math.round(samples[i] * 32767)));
      }
      const dataBytes = int16.length * 2;
      const header = new ArrayBuffer(44);
      const v = new DataView(header);
      const ws = (off: number, s: string) => { for (let i = 0; i < s.length; i++) v.setUint8(off + i, s.charCodeAt(i)); };
      ws(0, 'RIFF'); v.setUint32(4, 36 + dataBytes, true);
      ws(8, 'WAVE'); ws(12, 'fmt '); v.setUint32(16, 16, true);
      v.setUint16(20, 1, true);     // PCM
      v.setUint16(22, 1, true);     // mono
      v.setUint32(24, 11025, true); // sample rate
      v.setUint32(28, 22050, true); // byte rate = 11025 * 2
      v.setUint16(32, 2, true);     // block align
      v.setUint16(34, 16, true);    // bits per sample
      ws(36, 'data'); v.setUint32(40, dataBytes, true);
      const blob = new Blob([header, int16.buffer], { type: 'audio/wav' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url; a.download = name.replace(/\.wav$/i, '_decoded.wav');
      a.click();
      setTimeout(() => URL.revokeObjectURL(url), 30000);
      setStatus(`Exported ${name}`);
    } catch (e: any) { setError(`Export failed: ${e.message}`); setStatus(''); }
  }

  // ── Derived ─────────────────────────────────────────────────────────────────
  const staged = sounds.filter(s => s.source === 'staged');
  const pendingDels = sounds.filter(s => s.pendingDelete);
  const pendingRenames = sounds.filter(s => s.pendingRenameTo);
  const hasPending = staged.length > 0 || pendingDels.length > 0 || pendingRenames.length > 0;

  const missingNames = Object.entries(refs)
    .filter(([, r]) => !r.inBin && (r.cpp || r.actordefs.length > 0))
    .map(([name]) => name).sort();

  const allEntries: (SoundEntry & { missing?: boolean })[] = [
    ...sounds,
    ...missingNames.map(name => ({
      name, storedLength: null, adpcmBytes: null, source: 'bin' as const,
      pendingDelete: false, pendingRenameTo: null, missing: true,
    })),
  ];

  function matchesFilter(s: SoundEntry & { missing?: boolean }): boolean {
    const ref = refs[s.name];
    if (search && !s.name.toLowerCase().includes(search.toLowerCase())) return false;
    if (s.missing) return filter === 'missing';
    if (filter === 'missing') return false;
    if (filter === 'headroom') return !!(levels[s.name]?.peak > 0.97 && ref?.volumeCalls?.some(vc => vc.vol === 128));
    if (filter === 'all') return true;
    if (filter === 'cpp') return !!(ref?.cpp);
    if (filter === 'actordef') return !!(ref?.actordefs?.length);
    if (filter === 'orphaned') return !!ref && !ref.cpp && !ref.actordefs?.length;
    if (filter === 'ambient') return !!(ref?.role) || !!(ref?.loop);
    if (filter === 'loop') return !!(ref?.loop);
    if (filter === 'attenuated') return !!(ref?.volumeCalls?.some((vc: { vol: number | string }) => typeof vc.vol === 'number' && vc.vol < 128));
    if (filter === 'ui') return ref?.category === 'ui';
    return true;
  }

  function willLowHeadroom(name: string): boolean {
    const lvl = levels[name];
    if (!lvl || lvl.peak <= 0.97) return false;
    return !!(refs[name]?.volumeCalls?.some(vc => vc.vol === 128));
  }

  const headroomCount = sounds.filter(s => willLowHeadroom(s.name)).length;

  function refCount(name: string): number {
    const r = refs[name];
    if (!r) return 0;
    return (r.cpp ? 1 : 0) + (r.actordefs?.length ?? 0);
  }
  const orphanedCount = sounds.filter(s => {
    const r = refs[s.name];
    return !!r && !r.cpp && !r.actordefs?.length;
  }).length;

  const filteredEntries = allEntries.filter(matchesFilter);
  const visibleEntries = sortKey ? [...filteredEntries].sort((a, b) => {
    let va: number | string = 0, vb: number | string = 0;
    if (sortKey === 'name')     { va = a.name.toLowerCase(); vb = b.name.toLowerCase(); }
    else if (sortKey === 'size')     { va = a.source === 'bin' ? (a.adpcmBytes ?? 0) : (a.size ?? 0); vb = b.source === 'bin' ? (b.adpcmBytes ?? 0) : (b.size ?? 0); }
    else if (sortKey === 'duration') { va = a.durationSec ?? 0; vb = b.durationSec ?? 0; }
    else if (sortKey === 'level')    {
      const gva = refs[a.name]?.volumeCalls?.[0]?.vol; const gvb = refs[b.name]?.volumeCalls?.[0]?.vol;
      va = (levels[a.name]?.peak ?? -1) * (typeof gva === 'number' ? gva / 128 : 1);
      vb = (levels[b.name]?.peak ?? -1) * (typeof gvb === 'number' ? gvb / 128 : 1);
    }
    else if (sortKey === 'refs')     { va = refCount(a.name); vb = refCount(b.name); }
    if (va < vb) return sortDir === 'asc' ? -1 : 1;
    if (va > vb) return sortDir === 'asc' ? 1 : -1;
    return 0;
  }) : filteredEntries;

  const visibleNonDeleted = visibleEntries.filter(s => !s.pendingDelete && !s.missing);
  // Keep ref in sync for arrow key handler (updated during render, safe for event callbacks)
  visibleNonDeletedRef.current = visibleNonDeleted;

  const selectedSound = visibleNonDeleted[selectedIdx];
  const selectedRef = selectedSound ? refs[selectedSound.name] : null;
  const selectedLevel = selectedSound ? levels[selectedSound.name] : null;

  // Derive set members from refs (for inspector)
  const setMembers: Record<string, string[]> = {};
  for (const [name, ref] of Object.entries(refs)) {
    if (ref.soundSet) {
      if (!setMembers[ref.soundSet]) setMembers[ref.soundSet] = [];
      setMembers[ref.soundSet].push(name);
    }
  }

  // Effective gain for distance preview panel
  const distAttenuation = Math.max(0, 1 - distance / 500);
  const inGameBaseGain = (() => {
    if (!inGameVol || !selectedRef) return 1.0;
    const calls = selectedRef.volumeCalls;
    const vol = calls?.[selectedVolCtx]?.vol ?? calls?.[0]?.vol;
    return typeof vol === 'number' ? vol / 128 : 1.0;
  })();
  const effectiveGain = distAttenuation * inGameBaseGain;
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

  // ── Render ──────────────────────────────────────────────────────────────────
  return (
    <div style={{ display: 'flex', height: '100vh', fontFamily: 'monospace', background: '#111', color: '#ccc' }}>
      <Sidebar wsConnected={serverReachable} />

      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>

        {/* Header */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 10, padding: '8px 14px', borderBottom: '1px solid #333', background: '#151515', flexWrap: 'wrap' }}>
          <span style={{ fontSize: 15, color: '#aaa', fontWeight: 'bold' }}>[ SOUND STUDIO ]</span>

          {/* Tab switcher */}
          <div style={{ display: 'flex', gap: 2, marginLeft: 6 }}>
            {(['sounds','music','ambient'] as TabMode[]).map(t => (
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
              {staged.length > 0 && `+${staged.length}`}
              {pendingDels.length > 0 && ` -${pendingDels.length}`}
              {pendingRenames.length > 0 && ` ~${pendingRenames.length}`}
            </span>
          )}

          <div style={{ flex: 1 }} />

          {tab === 'sounds' && <>
            <label style={{ display: 'flex', alignItems: 'center', gap: 5, fontSize: 11, color: '#777', cursor: 'pointer' }}>
              <input type="checkbox" checked={normalize} onChange={e => setNormalize(e.target.checked)} />
              normalize
            </label>
            {hasPending && (
              <button onClick={fetchDiff} disabled={diffLoading}
                style={{ padding: '3px 9px', background: '#1a1a2a', color: '#88f', border: '1px solid #446', borderRadius: 4, cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}>
                {diffLoading ? '…' : 'Diff'}
              </button>
            )}
            <button onClick={() => fileInputRef.current?.click()}
              style={{ padding: '3px 9px', background: '#333', color: '#ccc', border: '1px solid #555', borderRadius: 4, cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}>
              + Upload WAV
            </button>
            <input ref={fileInputRef} type="file" accept=".wav,audio/*" multiple style={{ display: 'none' }} onChange={onFileInput} />
            {staged.length > 0 && (
              <button onClick={clearStaged}
                style={{ padding: '3px 9px', background: '#1a1a1a', color: '#866', border: '1px solid #422', borderRadius: 4, cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}
                title="Remove all staged uploads (bin sounds unaffected)">
                clear staged ({staged.length})
              </button>
            )}
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
        {!binLoaded ? (
          <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
            <div style={{ textAlign: 'center', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 16, padding: 48, border: '1px solid #2a2a2a', borderRadius: 4 }}>
              <div style={{ fontSize: 32 }}>📦</div>
              <div style={{ fontSize: 13, color: '#666' }}>
                Select your local <code style={{ color: '#88a' }}>sound.bin</code> to upload and start editing.
              </div>
              <button
                    onClick={() => binInputRef.current?.click()}
                    disabled={loading}
                    style={{ padding: '10px 28px', border: '1px solid #66a', color: '#aaf', background: 'none', cursor: loading ? 'default' : 'pointer', fontFamily: 'monospace', fontSize: 13, letterSpacing: 2, opacity: loading ? 0.5 : 1 }}>
                {loading ? 'LOADING…' : '[ OPEN SOUND.BIN ]'}
              </button>
              <input ref={binInputRef} type="file" accept=".bin" style={{ display: 'none' }} onChange={handleBinFile} />
              {error && <div style={{ fontSize: 11, color: '#f66' }}>{error}</div>}
            </div>
          </div>
        ) : tab === 'sounds' && <>

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

          {/* Low headroom banner */}
          {headroomCount > 0 && filter !== 'headroom' && (
            <div style={{ padding: '4px 14px', fontSize: 11, background: '#1e0a0a', color: '#f44', borderBottom: '1px solid #333', display: 'flex', alignItems: 'center', gap: 10 }}>
              <span>⚠ {headroomCount} decoded sound{headroomCount > 1 ? 's' : ''} near full scale with vol=128 call sites</span>
              <button onClick={() => setFilter('headroom')}
                style={{ background: 'none', border: '1px solid #f44', color: '#f44', borderRadius: 3, padding: '0 5px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                show
              </button>
            </div>
          )}

          {/* Orphaned bulk-delete banner */}
          {filter === 'orphaned' && orphanedCount > 0 && (
            <div style={{ padding: '4px 14px', fontSize: 11, background: '#1a1a1a', color: '#666', borderBottom: '1px solid #2a2a2a', display: 'flex', alignItems: 'center', gap: 10 }}>
              <span>{orphanedCount} orphaned sounds not referenced by game</span>
              <button onClick={async () => {
                const toDelete = sounds.filter(s => { const r = refs[s.name]; return !!r && !r.cpp && !r.actordefs?.length && !s.pendingDelete; });
                if (!toDelete.length) return;
                if (!confirm(`Stage ${toDelete.length} orphaned sound${toDelete.length > 1 ? 's' : ''} for deletion?`)) return;
                for (const s of toDelete) { try { await apiFetch(`/sounds/${encodeURIComponent(s.name)}`, { method: 'DELETE' }); } catch {} }
                setStatus(`Staged ${toDelete.length} orphaned sounds for deletion`); load();
              }} style={{ background: 'none', border: '1px solid #444', color: '#888', borderRadius: 3, padding: '0 7px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                delete all orphaned
              </button>
            </div>
          )}

          {/* A→B compare bar */}
          {(compareA || compareB) && (
            <div style={{ padding: '4px 14px', fontSize: 11, background: '#0e1422', borderBottom: '1px solid #2a2a3a', display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>
              <span style={{ color: '#446', fontSize: 10 }}>A→B</span>
              {(['A','B'] as const).map(slot => {
                const name = slot === 'A' ? compareA : compareB;
                const active = comparePlaying === slot;
                return (
                  <span key={slot} style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                    <span style={{ color: '#556', fontSize: 9 }}>{slot}:</span>
                    {name ? (
                      <>
                        <span style={{ color: active ? '#8cf' : '#88a', fontFamily: 'monospace' }}>{name}</span>
                        <button onClick={() => playCompareSlot(slot)}
                          style={{ background: active ? '#1a2a3a' : 'none', border: `1px solid ${active ? '#4af' : '#334'}`, color: active ? '#8cf' : '#668', borderRadius: 3, padding: '0 5px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                          {active ? '⏹' : '▶'}
                        </button>
                        <button onClick={() => slot === 'A' ? setCompareA(null) : setCompareB(null)}
                          style={{ background: 'none', border: 'none', color: '#334', cursor: 'pointer', fontSize: 10 }}>×</button>
                      </>
                    ) : <span style={{ color: '#333', fontSize: 9 }}>—</span>}
                  </span>
                );
              })}
              {compareA && compareB && (
                <button onClick={playAtoB}
                  style={{ padding: '1px 8px', background: '#1a1a2e', border: '1px solid #446', color: '#88f', borderRadius: 3, cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                  A→B
                </button>
              )}
              {comparePlaying && (
                <button onClick={stopCompare}
                  style={{ padding: '1px 6px', background: 'none', border: '1px solid #446', color: '#668', borderRadius: 3, cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                  Stop
                </button>
              )}
            </div>
          )}

          {/* Filter / search / group bar */}
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, padding: '5px 14px', borderBottom: '1px solid #222', background: '#131313', flexWrap: 'wrap' }}>
            <input type="text" placeholder="search…" value={search} onChange={e => setSearch(e.target.value)}
              style={{ padding: '2px 7px', background: '#222', border: '1px solid #444', borderRadius: 3, color: '#ccc', fontFamily: 'monospace', fontSize: 11, width: 140 }} />
            {(['all','cpp','actordef','loop','ambient','attenuated','ui','orphaned','missing','headroom'] as FilterMode[]).map(f => (
              <button key={f} onClick={() => setFilter(f)}
                style={{ padding: '1px 7px', fontSize: 10, fontFamily: 'monospace',
                  background: filter === f ? '#2a3a4a' : 'transparent',
                  border: `1px solid ${filter === f ? '#4af' : '#2a2a2a'}`,
                  color: filter === f ? '#8cf' : '#555', borderRadius: 3, cursor: 'pointer' }}>
                {f}{f === 'missing' && missingNames.length > 0 ? ` (${missingNames.length})` : ''}{f === 'headroom' && headroomCount > 0 ? ` (${headroomCount})` : ''}
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

          {/* Multi-select bulk ops bar */}
          {multiSel.size > 0 && (
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '4px 14px', background: '#0e1e0e', borderBottom: '1px solid #2a4a2a', fontSize: 11 }}>
              <span style={{ color: '#4a8', fontFamily: 'monospace' }}>{multiSel.size} selected</span>
              <button onClick={() => setMultiSel(new Set())}
                style={{ background: 'none', border: '1px solid #333', color: '#555', borderRadius: 3, padding: '0 6px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>✕ clear</button>
              <button onClick={async () => {
                for (const name of multiSel) {
                  const s = sounds.find(x => x.name === name);
                  if (s && !s.pendingDelete) { try { await apiFetch(`/sounds/${encodeURIComponent(name)}`, { method: 'DELETE' }); } catch {} }
                }
                setMultiSel(new Set()); load();
              }} style={{ background: 'none', border: '1px solid #4a2a2a', color: '#f66', borderRadius: 3, padding: '0 8px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                ✕ stage delete ({multiSel.size})
              </button>
              <span style={{ color: '#333', fontSize: 9, marginLeft: 4 }}>Ctrl+click to toggle · Shift+click to range-select</span>
            </div>
          )}

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
                      {(['name','duration','size','level','refs'] as const).map(col => {
                        const labels: Record<string, string> = { name: 'Name', duration: 'Dur', size: 'Size', level: 'Peak', refs: 'Refs' };
                        const aligns: Record<string, 'left' | 'right'> = { name: 'left', duration: 'right', size: 'right', level: 'left', refs: 'right' };
                        const widths: Record<string, number> = { duration: 54, size: 62, level: 90, refs: 40 };
                        const active = sortKey === col;
                        return (
                          <th key={col} onClick={() => toggleSort(col as SortKey)}
                            style={{ textAlign: aligns[col] || 'left', padding: '4px 5px', width: widths[col], cursor: 'pointer',
                              color: active ? '#8cf' : '#444', userSelect: 'none' }}>
                            {labels[col]}{active ? (sortDir === 'asc' ? ' ▲' : ' ▼') : ''}
                          </th>
                        );
                      })}
                      <th style={{ textAlign: 'right', padding: '4px 5px', width: 110 }}></th>
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
                            onClick={(e) => {
                              if (s.pendingDelete) return;
                              if (e.ctrlKey || e.metaKey) {
                                setMultiSel(prev => { const n = new Set(prev); n.has(s.name) ? n.delete(s.name) : n.add(s.name); return n; });
                                return;
                              }
                              if (e.shiftKey && selectedIdx >= 0) {
                                const list = visibleNonDeletedRef.current;
                                const lo = Math.min(selectedIdx, visIdx), hi = Math.max(selectedIdx, visIdx);
                                setMultiSel(new Set(list.slice(lo, hi + 1).map(x => x.name)));
                                return;
                              }
                              setMultiSel(new Set());
                              if (!isMissing) { setSelectedIdx(visIdx); setSelectedVolCtx(0); play(s.name); }
                              else { setSelectedIdx(visIdx); setSelectedVolCtx(0); }
                            }}
                            style={{
                              borderBottom: '1px solid #1a1a1a',
                              opacity: s.pendingDelete ? 0.4 : isMissing ? 0.7 : 1,
                              cursor: s.pendingDelete ? 'default' : 'pointer',
                              background: multiSel.has(s.name) ? '#1e2a1e' : isMissing ? '#1e1000' : isPlaying ? '#1a2a1a' : isSelected ? '#1e1e28' : s.source === 'staged' ? '#1a1a2a' : 'transparent',
                              outline: multiSel.has(s.name) ? '1px solid #4a6' : isSelected ? '1px solid #445' : 'none',
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
                              {s.pendingRenameTo && <span style={{ marginLeft: 5, color: '#fa8', fontSize: 9 }}>[→{s.pendingRenameTo}]</span>}
                              {isMissing && <span style={{ marginLeft: 5, color: '#f90', fontSize: 9 }}>[missing]</span>}
                              {ref?.role && <span style={{ marginLeft: 5, color: '#8af', fontSize: 9 }}>[{ref.role}]</span>}
                              {!ref?.role && isLoop && <span style={{ marginLeft: 5, color: '#68a', fontSize: 9 }}>[loop]</span>}
                              {ref?.category === 'ui' && <span style={{ marginLeft: 5, color: '#a8f', fontSize: 9 }}>[ui]</span>}
                              {ref?.soundSet && <span style={{ marginLeft: 5, color: '#a86', fontSize: 9 }}>[{ref.soundSet}]</span>}
                            </td>
                            <td style={{ padding: '3px 5px', textAlign: 'right', color: '#383838', fontVariantNumeric: 'tabular-nums', fontSize: 10 }}>
                              {formatDuration((s as any).durationSec)}
                            </td>
                            <td style={{ padding: '3px 5px', textAlign: 'right', color: '#444', fontVariantNumeric: 'tabular-nums' }}>{formatBytes(size)}</td>
                            <td style={{ padding: '3px 5px' }}>
                              {lvl ? (() => {
                                const vcs = ref?.volumeCalls;
                                const gv = vcs?.length ? (typeof vcs[0].vol === 'number' ? vcs[0].vol as number : 128) : null;
                                const effectivePeak = gv != null ? lvl.peak * (gv / 128) : null;
                                const displayPeak = effectivePeak ?? lvl.peak;
                                const barColor = displayPeak > 0.9 ? '#f44' : displayPeak > 0.6 ? '#fa4' : '#4a8';
                                return (
                                  <div style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
                                    <div style={{ width: 50, height: 5, background: '#222', borderRadius: 2, overflow: 'hidden', position: 'relative' }}>
                                      {effectivePeak != null && (
                                        <div style={{ position: 'absolute', width: `${Math.round(lvl.peak * 100)}%`, height: '100%', background: '#2a2a2a', borderRadius: 2 }} />
                                      )}
                                      <div style={{ position: 'absolute', width: `${Math.round(displayPeak * 100)}%`, height: '100%', background: barColor, borderRadius: 2 }} />
                                    </div>
                                    <span style={{ color: '#444', fontSize: 9 }}>{Math.round(displayPeak * 100)}%</span>
                                    {gv != null && gv < 128 && <span style={{ color: '#3a5a3a', fontSize: 9 }}>{gv}</span>}
                                    {willLowHeadroom(s.name) && <span title="Low headroom at vol=128" style={{ color: '#f44', fontSize: 8, fontWeight: 'bold' }}>HDR</span>}
                                  </div>
                                );
                              })() : <span style={{ color: '#2a2a2a', fontSize: 9 }}>—</span>}
                            </td>
                            <td style={{ padding: '3px 5px', textAlign: 'right' }}>
                              <span style={{ display: 'inline-flex', gap: 2, alignItems: 'center' }}>
                                {ref?.cpp && <span title="Referenced in C++" style={{ background: '#2a3a2a', border: '1px solid #3a5a3a', color: '#8c8', borderRadius: 2, padding: '0 3px', fontSize: 9 }}>C++</span>}
                                {ref?.actordefs?.length > 0 && <span title={ref.actordefs.join(', ')} style={{ background: '#1a2a3a', border: '1px solid #2a4a6a', color: '#68a', borderRadius: 2, padding: '0 3px', fontSize: 9 }}>ADef×{ref.actordefs.length}</span>}
                                {!ref?.cpp && !ref?.actordefs?.length && !isMissing && <span style={{ color: '#2a2a2a', fontSize: 9 }}>—</span>}
                              </span>
                            </td>
                            <td style={{ padding: '3px 5px', textAlign: 'right' }}>
                              <span style={{ display: 'inline-flex', gap: 3 }}>
                                {!isMissing && (
                                  <>
                                    <button onClick={e => { e.stopPropagation(); setCompareA(s.name); }}
                                      title="Set as compare A"
                                      style={{ background: compareA === s.name ? '#1a2a1a' : 'none', border: `1px solid ${compareA === s.name ? '#4a8' : '#222'}`, color: compareA === s.name ? '#8c8' : '#445', borderRadius: 3, padding: '0 3px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 9 }}>A</button>
                                    <button onClick={e => { e.stopPropagation(); setCompareB(s.name); }}
                                      title="Set as compare B"
                                      style={{ background: compareB === s.name ? '#1a1a2a' : 'none', border: `1px solid ${compareB === s.name ? '#66a' : '#222'}`, color: compareB === s.name ? '#88f' : '#445', borderRadius: 3, padding: '0 3px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 9 }}>B</button>
                                  </>
                                )}
                                {isLoop && !isMissing && (
                                  <button onClick={e => { e.stopPropagation(); toggleLoop(s.name); }}
                                    title={isLooping ? 'Stop loop' : 'Loop test'}
                                    style={{ background: isLooping ? '#1a2a3a' : 'none', border: `1px solid ${isLooping ? '#4af' : '#2a2a2a'}`, color: isLooping ? '#8cf' : '#556', borderRadius: 3, padding: '0 4px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                                    {isLooping ? '⏹↺' : '↺'}
                                  </button>
                                )}
                                {!isMissing && !s.pendingDelete && (
                                  <button onClick={e => { e.stopPropagation(); exportWav(s.name); }}
                                    title="Export as decoded WAV"
                                    style={{ background: 'none', border: '1px solid #2a2a2a', color: '#556', borderRadius: 3, padding: '0 4px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>↓</button>
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
                  {selectedRef?.fadeoutMs && <div style={{ marginTop: 2, fontSize: 10, color: '#787' }}>↘ Fades out over {selectedRef.fadeoutMs}ms on stop</div>}
                  {selectedRef?.category && <div style={{ marginTop: 2, fontSize: 10, color: '#666' }}>Category: {CATEGORY_LABELS[selectedRef.category] || selectedRef.category}</div>}
                  {selectedSound.durationSec != null && <div style={{ marginTop: 2, fontSize: 10, color: '#555' }}>Duration: {formatDuration(selectedSound.durationSec)}</div>}
                  {selectedLevel && (
                    <div style={{ marginTop: 4, fontSize: 10, color: '#555', display: 'flex', gap: 10 }}>
                      <span>Peak <span style={{ color: selectedLevel.peak > 0.9 ? '#f44' : '#9a9' }}>{(selectedLevel.peak * 100).toFixed(0)}%</span></span>
                      <span>RMS <span style={{ color: '#9a9' }}>{(selectedLevel.rms * 100).toFixed(0)}%</span></span>
                    </div>
                  )}
                  {willLowHeadroom(selectedSound.name) && (
                    <div style={{ marginTop: 4, fontSize: 10, color: '#f44', display: 'flex', alignItems: 'center', gap: 4 }}>
                      ⚠ Low headroom at vol=128 call sites
                    </div>
                  )}
                  {selectedSound.adpcmBytes != null && selectedSound.adpcmBytes < 256 && (
                    <div style={{ marginTop: 4, fontSize: 10, color: '#f84', display: 'flex', alignItems: 'center', gap: 4 }}>
                      ⚠ &lt;256 ADPCM bytes — game skips this sound at load
                    </div>
                  )}
                  {normalize && selectedLevel && selectedLevel.peak > 0 && (
                    <div style={{ marginTop: 4, fontSize: 10, color: '#8af', display: 'flex', gap: 6 }}>
                      <span>Normalize: ×{(1 / selectedLevel.peak).toFixed(2)}</span>
                      <span style={{ color: '#556' }}>→ peak 100%</span>
                    </div>
                  )}
                  <div style={{ marginTop: 6 }}>
                    <button onClick={() => exportWav(selectedSound.name)}
                      style={{ padding: '2px 8px', background: '#1a1a2a', border: '1px solid #334', color: '#66a', borderRadius: 3, cursor: 'pointer', fontFamily: 'monospace', fontSize: 10 }}>
                      ↓ Export decoded WAV
                    </button>
                  </div>
                </div>

                {/* Waveform */}
                <div style={{ padding: '6px 10px', borderBottom: '1px solid #1e1e1e', position: 'relative' }}>
                  <canvas ref={waveformCanvasRef} width={232} height={44}
                    style={{ width: '100%', height: 44, display: 'block', borderRadius: 2, background: '#1a2a1a' }} />
                  {!levels[selectedSound.name] && (
                    <div style={{ position: 'absolute', inset: 6, display: 'flex', alignItems: 'center', justifyContent: 'center', pointerEvents: 'none' }}>
                      <span style={{ fontSize: 9, color: '#334' }}>▶ play to decode waveform</span>
                    </div>
                  )}
                </div>

                {/* Distance / volume preview */}
                <div style={{ padding: '7px 10px', borderBottom: '1px solid #1e1e1e' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 4 }}>
                    <div style={{ fontSize: 10, color: '#555' }}>DISTANCE PREVIEW</div>
                    <label style={{ fontSize: 9, color: inGameVol ? '#8a8' : '#555', display: 'flex', alignItems: 'center', gap: 3, cursor: 'pointer' }}>
                      <input type="checkbox" checked={inGameVol} onChange={e => setInGameVol(e.target.checked)} style={{ accentColor: '#4a8' }} />
                      in-game vol
                    </label>
                  </div>
                  <input type="range" min={0} max={500} value={distance} onChange={e => setDistance(Number(e.target.value))}
                    style={{ width: '100%', accentColor: '#4a8' }} />
                  <div style={{ fontSize: 10, color: '#666', display: 'flex', justifyContent: 'space-between', marginTop: 2 }}>
                    <span>{distance}px</span>
                    {inGameVol && selectedRef?.volumeCalls?.[0] && typeof selectedRef.volumeCalls[0].vol === 'number' && (
                      <span style={{ color: '#787' }}>{Math.round(selectedRef.volumeCalls[0].vol / 128 * 100)}% × {Math.round(distAttenuation * 100)}% =</span>
                    )}
                    <span style={{ color: effectiveGain > 0 ? '#9a9' : '#f66' }}>
                      {effectiveGain > 0 ? `${Math.round(effectiveGain * 100)}%` : 'silent'}
                    </span>
                  </div>
                </div>

                {/* Volume map */}
                {selectedRef?.volumeCalls && selectedRef.volumeCalls.length > 0 && (
                  <div style={{ padding: '7px 10px', borderBottom: '1px solid #1e1e1e', overflow: 'auto', maxHeight: 160 }}>
                    <div style={{ fontSize: 10, color: '#555', marginBottom: 4 }}>
                      VOLUME IN GAME {selectedRef.volumeCalls.length > 1 && <span style={{ color: '#446', fontSize: 9 }}>— click to preview at that vol</span>}
                    </div>
                    {selectedRef.volumeCalls.map((vc, i) => (
                      <div key={i} onClick={() => setSelectedVolCtx(i)}
                        style={{ display: 'flex', justifyContent: 'space-between', fontSize: 10, padding: '2px 4px',
                          borderBottom: '1px solid #1a1a1a',
                          background: selectedVolCtx === i && selectedRef.volumeCalls.length > 1 ? '#1a2a1a' : 'transparent',
                          borderLeft: `2px solid ${selectedVolCtx === i && selectedRef.volumeCalls.length > 1 ? '#4a8' : 'transparent'}`,
                          cursor: selectedRef.volumeCalls.length > 1 ? 'pointer' : 'default' }}>
                        <span style={{ color: selectedVolCtx === i && selectedRef.volumeCalls.length > 1 ? '#aaa' : '#666', flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{vc.ctx}</span>
                        <span style={{ color: volColor(vc.vol), marginLeft: 6, flexShrink: 0, fontVariantNumeric: 'tabular-nums' }}>{vc.vol}/128</span>
                      </div>
                    ))}
                  </div>
                )}

                {/* Sound set */}
                {selectedRef?.soundSet && setMembers[selectedRef.soundSet] && (
                  <div style={{ padding: '7px 10px', borderBottom: '1px solid #1e1e1e', overflow: 'auto', maxHeight: 180 }}>
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 2 }}>
                      <div style={{ fontSize: 10, color: '#555' }}>SOUND SET — {selectedRef.soundSet}</div>
                      <button onClick={() => { const ms = setMembers[selectedRef.soundSet!]!.filter(m => sounds.some(s => s.name === m)); if (ms.length) play(ms[Math.floor(Math.random() * ms.length)]); }}
                        style={{ background: 'none', border: '1px solid #2a3a2a', color: '#566', borderRadius: 3, padding: '0 5px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 9 }}>▶ rnd</button>
                    </div>
                    <div style={{ fontSize: 9, color: '#555', marginBottom: 6 }}>
                      {setMembers[selectedRef.soundSet].length} variants chosen randomly — replace together for consistency
                    </div>
                    {setMembers[selectedRef.soundSet].map(m => {
                      const exists = sounds.some(s => s.name === m);
                      return (
                        <div key={m} style={{ display: 'flex', alignItems: 'center', gap: 4, padding: '1px 0' }}>
                          <button onClick={() => play(m)} disabled={!exists}
                            style={{ background: 'none', border: 'none', color: exists ? '#556' : '#333', cursor: exists ? 'pointer' : 'default', fontSize: 10, padding: 0, fontFamily: 'monospace' }}>▶</button>
                          <span style={{ fontSize: 10, color: m === selectedSound.name ? '#ddd' : exists ? '#669' : '#444', flex: 1 }}>{m}</span>
                          {!exists && <span style={{ fontSize: 8, color: '#f90' }}>✗</span>}
                        </div>
                      );
                    })}
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
        {tab === 'music' && binLoaded && (
          <div style={{ flex: 1, overflow: 'auto', padding: 16 }}>
            <div style={{ fontSize: 11, color: '#555', marginBottom: 10 }}>
              Music files in <code>shared/assets/</code> — loaded by the game separately from sound.bin.
              Game default music volume: <span style={{ color: '#668' }}>48/128 (37.5%)</span> — set in <code>config.cpp</code>.
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
        {tab === 'ambient' && binLoaded && (
          <div style={{ flex: 1, overflow: 'auto', padding: 16, fontFamily: 'monospace' }}>
            <div style={{ fontSize: 11, color: '#555', marginBottom: 12 }}>
              BG Channel Mixer — simulates how the game blends background ambient sounds based on outdoor tile coverage.
            </div>

            {/* Indoor/Outdoor toggle */}
            <div style={{ marginBottom: 12 }}>
              <div style={{ fontSize: 10, color: '#666', marginBottom: 5 }}>ENVIRONMENT</div>
              <div style={{ display: 'flex', gap: 6 }}>
                <button onClick={() => setBgIndoor(true)}
                  style={{ padding: '4px 12px', background: bgIndoor ? '#1e2a1e' : 'none', border: `1px solid ${bgIndoor ? '#4a8' : '#333'}`, color: bgIndoor ? '#8c8' : '#555', borderRadius: 3, cursor: 'pointer', fontSize: 11 }}>
                  🏠 Indoor
                </button>
                <button onClick={() => setBgIndoor(false)}
                  style={{ padding: '4px 12px', background: !bgIndoor ? '#1e2a1e' : 'none', border: `1px solid ${!bgIndoor ? '#4a8' : '#333'}`, color: !bgIndoor ? '#8c8' : '#555', borderRadius: 3, cursor: 'pointer', fontSize: 11 }}>
                  🌤 Outdoor
                </button>
              </div>
            </div>

            {/* Outdoor ratio slider */}
            {!bgIndoor && (
              <div style={{ marginBottom: 12 }}>
                <div style={{ fontSize: 10, color: '#666', marginBottom: 4 }}>OUTDOOR RATIO (outside tiles / max)</div>
                <input type="range" min={0} max={100} value={Math.round(bgRatio * 100)} onChange={e => setBgRatio(Number(e.target.value) / 100)}
                  style={{ width: 200, accentColor: '#4a8' }} />
                <span style={{ marginLeft: 8, fontSize: 10, color: '#8a8' }}>{Math.round(bgRatio * 100)}%</span>
              </div>
            )}

            {/* Channel status */}
            <div style={{ marginBottom: 12 }}>
              <div style={{ fontSize: 10, color: '#666', marginBottom: 6 }}>CHANNELS</div>
              {(['wndloopb.wav','cphum11.wav','wndloop1.wav'] as const).map((ch, i) => {
                const roles = ['BG_BASE','BG_AMBIENT','BG_OUTSIDE'] as const;
                const gains = computeBgGains(bgIndoor, bgRatio, bgMutes);
                const gain = gains[i];
                return (
                  <div key={ch} style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 6, padding: '5px 10px', background: '#161616', borderRadius: 3, border: '1px solid #222' }}>
                    <button onClick={() => setBgMutes(m => { const n = [...m]; n[i] = !n[i]; return n; })}
                      style={{ background: bgMutes[i] ? '#2a1a1a' : 'none', border: `1px solid ${bgMutes[i] ? '#844' : '#333'}`, color: bgMutes[i] ? '#f66' : '#668', borderRadius: 3, padding: '0 6px', cursor: 'pointer', fontSize: 10 }}>
                      {bgMutes[i] ? '🔇' : '🔊'}
                    </button>
                    <span style={{ color: '#88a', width: 90, fontSize: 10 }}>{ch}</span>
                    <span style={{ color: '#446', fontSize: 9, width: 70 }}>{roles[i]}</span>
                    <div style={{ flex: 1, height: 4, background: '#222', borderRadius: 2, overflow: 'hidden' }}>
                      <div style={{ width: `${Math.round(gain / (32 / 128) * 100)}%`, height: '100%', background: bgMutes[i] ? '#444' : '#4a8', maxWidth: '100%' }} />
                    </div>
                    <span style={{ color: bgMutes[i] ? '#444' : '#6a6', fontSize: 9, width: 36, textAlign: 'right' }}>{Math.round(gain * 128)}/128</span>
                  </div>
                );
              })}
            </div>

            {/* Start/Stop */}
            <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
              <button onClick={bgRunning ? stopBgMixer : startBgMixer}
                style={{ padding: '5px 16px', background: bgRunning ? '#2a1e1e' : '#1a2a1e', border: `1px solid ${bgRunning ? '#844' : '#4a8'}`, color: bgRunning ? '#f88' : '#8c8', borderRadius: 4, cursor: 'pointer', fontFamily: 'monospace', fontSize: 12 }}>
                {bgRunning ? '⏹ Stop' : '▶ Start'}
              </button>
              {bgRunning && <span style={{ fontSize: 10, color: '#4a8' }}>● playing</span>}
            </div>
          </div>
        )}
      </div>

      {/* Diff modal */}
      {showDiff && diffData && (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.75)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 100 }}
          onClick={() => setShowDiff(false)}>
          <div onClick={e => e.stopPropagation()}
            style={{ background: '#141414', border: '1px solid #333', borderRadius: 6, padding: 20, width: 440, maxHeight: '80vh', overflow: 'auto', fontFamily: 'monospace' }}>
            <div style={{ fontSize: 13, color: '#aaa', marginBottom: 14 }}>Pending changes</div>

            {diffData.added.length > 0 && (
              <div style={{ marginBottom: 12 }}>
                <div style={{ fontSize: 10, color: '#4a8', marginBottom: 4 }}>+ ADDED ({diffData.added.length})</div>
                {diffData.added.map(n => <div key={n} style={{ fontSize: 11, color: '#8c8', paddingLeft: 8 }}>{n}</div>)}
              </div>
            )}
            {diffData.modified.length > 0 && (
              <div style={{ marginBottom: 12 }}>
                <div style={{ fontSize: 10, color: '#fa4', marginBottom: 4 }}>~ MODIFIED ({diffData.modified.length})</div>
                {diffData.modified.map(n => <div key={n} style={{ fontSize: 11, color: '#da8', paddingLeft: 8 }}>{n}</div>)}
              </div>
            )}
            {diffData.deleted.length > 0 && (
              <div style={{ marginBottom: 12 }}>
                <div style={{ fontSize: 10, color: '#f44', marginBottom: 4 }}>- DELETED ({diffData.deleted.length})</div>
                {diffData.deleted.map(n => <div key={n} style={{ fontSize: 11, color: '#f88', paddingLeft: 8 }}>{n}</div>)}
              </div>
            )}
            {diffData.renamed.length > 0 && (
              <div style={{ marginBottom: 12 }}>
                <div style={{ fontSize: 10, color: '#88f', marginBottom: 4 }}>→ RENAMED ({diffData.renamed.length})</div>
                {diffData.renamed.map(r => <div key={r.from} style={{ fontSize: 11, color: '#aaf', paddingLeft: 8 }}>{r.from} → {r.to}</div>)}
              </div>
            )}
            {diffData.added.length === 0 && diffData.modified.length === 0 && diffData.deleted.length === 0 && diffData.renamed.length === 0 && (
              <div style={{ color: '#555', fontSize: 11 }}>No pending changes.</div>
            )}

            <div style={{ marginTop: 16, display: 'flex', gap: 8, justifyContent: 'flex-end' }}>
              <button onClick={() => setShowDiff(false)}
                style={{ padding: '4px 10px', background: 'none', border: '1px solid #444', color: '#888', borderRadius: 3, cursor: 'pointer', fontSize: 11 }}>Cancel</button>
              <button onClick={repackFromDiff} disabled={repacking}
                style={{ padding: '4px 12px', background: '#2a4a2a', border: '1px solid #4a8', color: '#8c8', borderRadius: 3, cursor: 'pointer', fontWeight: 'bold', fontSize: 11 }}>
                {repacking ? 'Repacking…' : '⚡ Repack now'}
              </button>
            </div>
          </div>
        </div>
      )}

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
