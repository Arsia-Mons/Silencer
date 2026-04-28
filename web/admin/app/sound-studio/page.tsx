'use client';
import { useEffect, useRef, useState, useCallback, DragEvent } from 'react';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { apiFetch } from '../../lib/api';

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

  const audioRef = useRef<HTMLAudioElement | null>(null);
  const [playing, setPlaying] = useState<string | null>(null);
  const fileInputRef = useRef<HTMLInputElement | null>(null);

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

  // ── Playback ────────────────────────────────────────────────────────────────

  function play(name: string) {
    if (audioRef.current) {
      audioRef.current.pause();
      audioRef.current.src = '';
    }
    if (playing === name) {
      setPlaying(null);
      return;
    }
    const token = typeof window !== 'undefined' ? localStorage.getItem('token') : '';
    // We use a Blob URL so we can pass auth header
    fetch(`/api/sounds/${encodeURIComponent(name)}/play`, {
      headers: { Authorization: `Bearer ${token}` },
    })
      .then(r => r.blob())
      .then(blob => {
        const url = URL.createObjectURL(blob);
        const audio = new Audio(url);
        audioRef.current = audio;
        audio.play();
        setPlaying(name);
        audio.onended = () => {
          setPlaying(null);
          URL.revokeObjectURL(url);
        };
        audio.onerror = () => {
          setPlaying(null);
          URL.revokeObjectURL(url);
          setError(`Could not play ${name}`);
        };
      })
      .catch(e => { setError(e.message); setPlaying(null); });
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
                {sounds.map(s => {
                  const isPlaying = playing === s.name;
                  const size = s.source === 'bin' ? s.adpcmBytes : s.size;
                  return (
                    <tr
                      key={s.name}
                      style={{
                        borderBottom: '1px solid #222',
                        opacity: s.pendingDelete ? 0.4 : 1,
                        background: isPlaying ? '#1a2a1a' : s.source === 'staged' ? '#1a1a2a' : 'transparent',
                      }}
                    >
                      <td style={{ padding: '4px 8px', textAlign: 'center' }}>
                        <button
                          onClick={() => play(s.name)}
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
        </div>
      </div>
    </div>
  );
}
