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

function formatBytes(n: number | null | undefined): string {
  if (n == null) return '—';
  if (n < 1024) return `${n} B`;
  if (n < 1048576) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / 1048576).toFixed(1)} MB`;
}

export default function SoundStudioPage() {
  useAuth();

  const [sounds, setSounds] = useState<SoundEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [status, setStatus] = useState('');
  const [repacking, setRepacking] = useState(false);
  const [dragOver, setDragOver] = useState(false);

  const audioCtxRef = useRef<AudioContext | null>(null);
  const audioSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const [playing, setPlaying] = useState<string | null>(null);
  const [selectedIdx, setSelectedIdx] = useState<number>(-1);
  const fileInputRef = useRef<HTMLInputElement | null>(null);
  const rowRefs = useRef<(HTMLTableRowElement | null)[]>([]);
  const soundsRef = useRef<SoundEntry[]>([]);

  const load = useCallback(async () => {
    setLoading(true);
    setError('');
    try {
      const data = await apiFetch('/sounds') as SoundEntry[];
      setSounds(data);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { load(); }, [load]);

  // Keep soundsRef in sync so keydown handler can read latest sounds
  useEffect(() => { soundsRef.current = sounds; }, [sounds]);

  // Arrow key navigation: up/down moves selection and plays the sound
  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') return;
      // Ignore if focus is inside an input/button
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === 'INPUT' || tag === 'BUTTON' || tag === 'SELECT') return;
      e.preventDefault();
      const list = soundsRef.current.filter(s => !s.pendingDelete);
      if (!list.length) return;
      setSelectedIdx(prev => {
        const next = e.key === 'ArrowDown'
          ? Math.min(prev + 1, list.length - 1)
          : Math.max(prev - 1, 0);
        // Scroll selected row into view
        rowRefs.current[next]?.scrollIntoView({ block: 'nearest' });
        // Play the selected sound
        play(list[next].name);
        return next;
      });
    }
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // ── Playback via Web Audio API (handles IMA ADPCM WAV natively) ─────────────

  async function play(name: string) {
    // Stop whatever is playing
    if (audioSourceRef.current) {
      try { audioSourceRef.current.stop(); } catch {}
      audioSourceRef.current = null;
    }
    if (playing === name) { setPlaying(null); return; }

    const token = typeof window !== 'undefined' ? localStorage.getItem('zs_token') : '';
    try {
      const r = await fetch(`/api/sounds/${encodeURIComponent(name)}/play`, {
        headers: { Authorization: `Bearer ${token}` },
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
      // Use our JS IMA ADPCM decoder — browsers don't support decodeAudioData for ADPCM WAV
      const decoded = await decodeAdpcmWav(arrayBuf, audioCtx);
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

  // ── Upload ──────────────────────────────────────────────────────────────────

  async function uploadFile(file: File) {
    const name = file.name.replace(/[^a-zA-Z0-9!._-]/g, '_');
    setStatus(`Uploading ${name}…`);
    try {
      const buf = await file.arrayBuffer();
      await apiFetch('/sounds', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/octet-stream',
          'X-Filename': name,
        },
        body: buf,
      });
      setStatus(`Staged ${name}`);
      load();
    } catch (e: any) {
      setError(e.message);
      setStatus('');
    }
  }

  function onFileInput(e: React.ChangeEvent<HTMLInputElement>) {
    const files = Array.from(e.target.files || []);
    files.forEach(uploadFile);
    e.target.value = '';
  }

  function onDrop(e: DragEvent<HTMLDivElement>) {
    e.preventDefault();
    setDragOver(false);
    Array.from(e.dataTransfer.files).forEach(uploadFile);
  }

  // ── Delete / Restore ────────────────────────────────────────────────────────

  async function deleteSnd(name: string) {
    try {
      await apiFetch(`/sounds/${encodeURIComponent(name)}`, { method: 'DELETE' });
      setStatus(`${name} marked for deletion`);
      load();
    } catch (e: any) { setError(e.message); }
  }

  async function restoreSnd(name: string) {
    try {
      await apiFetch(`/sounds/${encodeURIComponent(name)}/restore`, { method: 'POST' });
      setStatus(`${name} restored`);
      load();
    } catch (e: any) { setError(e.message); }
  }

  // ── Repack ──────────────────────────────────────────────────────────────────

  async function repack() {
    if (!confirm('Rebuild sound.bin now? The existing file will be replaced.')) return;
    setRepacking(true);
    setStatus('Repacking…');
    setError('');
    try {
      const result = await apiFetch('/sounds/repack', { method: 'POST' }) as { numsounds: number; totalSize: number };
      setStatus(`Repacked — ${result.numsounds} sounds, ${formatBytes(result.totalSize)}`);
      load();
    } catch (e: any) {
      setError(e.message);
      setStatus('');
    } finally {
      setRepacking(false);
    }
  }

  // ── Derived ─────────────────────────────────────────────────────────────────

  const staged = sounds.filter(s => s.source === 'staged');
  const pendingDels = sounds.filter(s => s.pendingDelete);
  const hasPending = staged.length > 0 || pendingDels.length > 0;

  return (
    <div style={{ display: 'flex', height: '100vh', fontFamily: 'monospace', background: '#111', color: '#ccc' }}>
      <Sidebar />

      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {/* Header */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 16, padding: '12px 20px', borderBottom: '1px solid #333', background: '#151515' }}>
          <span style={{ fontSize: 18, color: '#aaa', fontWeight: 'bold' }}>[ SOUND STUDIO ]</span>
          <span style={{ color: '#555', fontSize: 12 }}>sound.bin packer</span>

          {hasPending && (
            <span style={{ background: '#f90', color: '#000', padding: '2px 8px', borderRadius: 4, fontSize: 11 }}>
              {staged.length > 0 && `+${staged.length} staged`}
              {staged.length > 0 && pendingDels.length > 0 && '  '}
              {pendingDels.length > 0 && `-${pendingDels.length} deletions`}
            </span>
          )}

          <div style={{ flex: 1 }} />

          <button
            onClick={() => fileInputRef.current?.click()}
            style={{ padding: '5px 12px', background: '#333', color: '#ccc', border: '1px solid #555', borderRadius: 4, cursor: 'pointer', fontFamily: 'monospace' }}
          >
            + Upload WAV
          </button>
          <input ref={fileInputRef} type="file" accept=".wav,audio/*" multiple style={{ display: 'none' }} onChange={onFileInput} />

          <button
            onClick={repack}
            disabled={repacking}
            style={{
              padding: '5px 16px',
              background: hasPending ? '#4a8' : '#555',
              color: hasPending ? '#fff' : '#999',
              border: `1px solid ${hasPending ? '#6ca' : '#666'}`,
              borderRadius: 4, cursor: repacking ? 'wait' : 'pointer', fontFamily: 'monospace',
              fontWeight: 'bold',
            }}
          >
            {repacking ? 'Repacking…' : '⚡ Save & Repack'}
          </button>
        </div>

        {/* Status bar */}
        {(status || error) && (
          <div style={{
            padding: '6px 20px', fontSize: 12,
            background: error ? '#3a0' + '0' : '#1a1a1a',
            color: error ? '#f66' : '#8c8',
            borderBottom: '1px solid #222',
          }}>
            {error || status}
            <button onClick={() => { setError(''); setStatus(''); }}
              style={{ marginLeft: 12, background: 'none', border: 'none', color: '#555', cursor: 'pointer', fontFamily: 'monospace' }}>
              ×
            </button>
          </div>
        )}

        {/* Drop zone + list */}
        <div
          style={{ flex: 1, overflow: 'auto', padding: 20, position: 'relative' }}
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
            }}>
              Drop WAV files to stage
            </div>
          )}

          {loading ? (
            <div style={{ color: '#555', padding: 40, textAlign: 'center' }}>Loading…</div>
          ) : (
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 13 }}>
              <thead>
                <tr style={{ color: '#555', borderBottom: '1px solid #333' }}>
                  <th style={{ textAlign: 'left', padding: '6px 8px', width: 24 }}></th>
                  <th style={{ textAlign: 'left', padding: '6px 8px' }}>Name</th>
                  <th style={{ textAlign: 'left', padding: '6px 8px' }}>Source</th>
                  <th style={{ textAlign: 'right', padding: '6px 8px' }}>Size</th>
                  <th style={{ textAlign: 'right', padding: '6px 8px' }}>Actions</th>
                </tr>
              </thead>
              <tbody>
                {sounds.filter(s => !s.pendingDelete || true).map((s, i) => {
                  const visibleIdx = sounds.filter(x => !x.pendingDelete).indexOf(s);
                  const isPlaying = playing === s.name;
                  const isSelected = !s.pendingDelete && visibleIdx === selectedIdx;
                  const size = s.source === 'bin' ? s.adpcmBytes : s.size;
                  return (
                    <tr
                      key={s.name}
                      ref={el => { if (!s.pendingDelete) rowRefs.current[visibleIdx] = el; }}
                      onClick={() => {
                        if (!s.pendingDelete) { setSelectedIdx(visibleIdx); play(s.name); }
                      }}
                      style={{
                        borderBottom: '1px solid #222',
                        opacity: s.pendingDelete ? 0.4 : 1,
                        cursor: s.pendingDelete ? 'default' : 'pointer',
                        background: isPlaying
                          ? '#1a2a1a'
                          : isSelected
                          ? '#1e1e28'
                          : s.source === 'staged' ? '#1a1a2a' : 'transparent',
                        outline: isSelected ? '1px solid #445' : 'none',
                      }}
                    >
                      <td style={{ padding: '4px 8px', textAlign: 'center' }}>
                        <button
                          onClick={e => { e.stopPropagation(); if (!s.pendingDelete) { setSelectedIdx(visibleIdx); play(s.name); } }}
                          disabled={s.pendingDelete}
                          title={isPlaying ? 'Stop' : 'Play'}
                          style={{
                            background: 'none', border: 'none',
                            color: isPlaying ? '#4a8' : '#777',
                            cursor: 'pointer', fontSize: 14, padding: 0,
                          }}
                        >
                          {isPlaying ? '⏹' : '▶'}
                        </button>
                      </td>
                      <td style={{ padding: '4px 8px', color: s.pendingDelete ? '#555' : '#ddd' }}>
                        {s.name}
                        {s.pendingDelete && <span style={{ marginLeft: 8, color: '#f66', fontSize: 11 }}>[pending deletion]</span>}
                        {s.source === 'staged' && <span style={{ marginLeft: 8, color: '#88f', fontSize: 11 }}>[staged]</span>}
                      </td>
                      <td style={{ padding: '4px 8px', color: '#555', fontSize: 11 }}>
                        {s.source === 'bin' ? 'sound.bin' : 'staging'}
                      </td>
                      <td style={{ padding: '4px 8px', textAlign: 'right', color: '#666', fontVariantNumeric: 'tabular-nums' }}>
                        {formatBytes(size)}
                      </td>
                      <td style={{ padding: '4px 8px', textAlign: 'right' }}>
                        {s.pendingDelete ? (
                          <button
                            onClick={() => restoreSnd(s.name)}
                            style={{ background: 'none', border: '1px solid #555', color: '#8a8', borderRadius: 3, padding: '1px 8px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}
                          >
                            Restore
                          </button>
                        ) : (
                          <button
                            onClick={() => deleteSnd(s.name)}
                            style={{ background: 'none', border: '1px solid #444', color: '#a55', borderRadius: 3, padding: '1px 8px', cursor: 'pointer', fontFamily: 'monospace', fontSize: 11 }}
                          >
                            Delete
                          </button>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          )}

          {!loading && sounds.length === 0 && (
            <div style={{ color: '#444', textAlign: 'center', padding: 60 }}>
              No sounds found in sound.bin<br />
              <span style={{ fontSize: 12 }}>Upload WAV files and click Save & Repack to add them.</span>
            </div>
          )}
        </div>

        {/* Footer: sound count summary */}
        <div style={{ padding: '6px 20px', borderTop: '1px solid #222', background: '#151515', fontSize: 11, color: '#444', display: 'flex', gap: 16 }}>
          <span>{sounds.filter(s => s.source === 'bin' && !s.pendingDelete).length} in bin</span>
          {staged.length > 0 && <span style={{ color: '#88f' }}>{staged.length} staged</span>}
          {pendingDels.length > 0 && <span style={{ color: '#f66' }}>{pendingDels.length} pending deletion</span>}
          <span>drag &amp; drop WAV files anywhere to stage</span>
          <span style={{ marginLeft: 'auto', color: '#333' }}>↑↓ navigate &amp; play</span>
        </div>
      </div>
    </div>
  );
}
