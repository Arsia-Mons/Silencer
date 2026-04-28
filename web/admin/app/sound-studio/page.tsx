'use client';
import { useEffect, useRef, useState, useCallback } from 'react';
import { useAuth } from '../../lib/auth';
import { useWsConnected } from '../../lib/socket';
import Sidebar from '../../components/Sidebar';
import {
  listSounds, soundUrl, uploadSound, deleteSound,
  getSoundEvents, patchSoundEvent,
  type SoundFile, type SoundEvents,
} from '../../lib/api';

// Known game sound events — extend as new events are identified in C++ source
const KNOWN_EVENTS = [
  'WEAPON_FIRE_BLASTER',
  'WEAPON_FIRE_LASER',
  'WEAPON_FIRE_ROCKET',
  'WEAPON_FIRE_GRENADE',
  'WEAPON_FIRE_FLAME',
  'WEAPON_RELOAD',
  'WEAPON_EMPTY',
  'FOOTSTEP_METAL',
  'FOOTSTEP_STONE',
  'JUMP',
  'LAND',
  'PLAYER_HURT',
  'PLAYER_DEATH',
  'ENEMY_HURT',
  'ENEMY_DEATH',
  'ENEMY_ALERT',
  'PICKUP_WEAPON',
  'PICKUP_HEALTH',
  'PICKUP_AMMO',
  'DOOR_OPEN',
  'DOOR_CLOSE',
  'EXPLOSION',
  'UI_CLICK',
  'UI_CONFIRM',
  'AMBIENT_LOOP',
  'MUSIC_MENU',
  'MUSIC_GAME',
];

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1048576) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / 1048576).toFixed(1)} MB`;
}

export default function SoundStudioPage() {
  useAuth();
  const wsConnected = useWsConnected();

  const [sounds, setSounds] = useState<SoundFile[]>([]);
  const [events, setEvents] = useState<SoundEvents>({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  // Playback
  const audioRef = useRef<HTMLAudioElement | null>(null);
  const [playing, setPlaying] = useState<string | null>(null);

  // Upload
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [uploading, setUploading] = useState(false);
  const [uploadError, setUploadError] = useState('');

  // Drag-over state for drop zone
  const [dragOver, setDragOver] = useState(false);

  // Search/filter
  const [search, setSearch] = useState('');

  // Event filter
  const [eventSearch, setEventSearch] = useState('');

  // Pending event patch
  const [patchingEvent, setPatchingEvent] = useState<string | null>(null);

  async function reload() {
    try {
      const [s, e] = await Promise.all([listSounds(), getSoundEvents()]);
      setSounds(s);
      setEvents(e);
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => { reload(); }, []);

  function playSound(filename: string) {
    if (playing === filename) {
      audioRef.current?.pause();
      setPlaying(null);
      return;
    }
    if (audioRef.current) {
      audioRef.current.pause();
      audioRef.current.src = soundUrl(filename);
      audioRef.current.play().catch(() => {});
      setPlaying(filename);
    }
  }

  useEffect(() => {
    const audio = new Audio();
    audio.onended = () => setPlaying(null);
    audioRef.current = audio;
    return () => { audio.pause(); audioRef.current = null; };
  }, []);

  const handleFiles = useCallback(async (files: FileList | File[]) => {
    const arr = Array.from(files).filter(f =>
      /\.(wav|ogg|mp3)$/i.test(f.name)
    );
    if (!arr.length) { setUploadError('Only WAV, OGG, MP3 files are supported.'); return; }
    setUploading(true);
    setUploadError('');
    try {
      await Promise.all(arr.map(f => uploadSound(f)));
      await reload();
    } catch (err) {
      setUploadError((err as Error).message);
    } finally {
      setUploading(false);
    }
  }, []);

  async function handleDelete(filename: string) {
    if (!confirm(`Delete ${filename}?`)) return;
    try {
      await deleteSound(filename);
      if (playing === filename) { audioRef.current?.pause(); setPlaying(null); }
      await reload();
    } catch (err) {
      setError((err as Error).message);
    }
  }

  async function handleEventChange(event: string, filename: string) {
    setPatchingEvent(event);
    try {
      const updated = await patchSoundEvent(event, filename || null);
      setEvents(updated);
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setPatchingEvent(null);
    }
  }

  const filteredSounds = sounds.filter(s =>
    s.filename.toLowerCase().includes(search.toLowerCase())
  );
  const filteredEvents = KNOWN_EVENTS.filter(e =>
    e.toLowerCase().includes(eventSearch.toLowerCase())
  );

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text font-mono">
      <Sidebar wsConnected={wsConnected} />

      <div className="flex-1 flex flex-col overflow-hidden">
        {/* Header */}
        <div className="flex items-center justify-between px-6 py-4 border-b border-game-border bg-game-bgCard flex-shrink-0">
          <div>
            <h1 className="text-lg font-bold tracking-widest text-game-primary">[ SOUND STUDIO ]</h1>
            <p className="text-xs text-game-textDim mt-0.5">Browse, upload, and assign game sounds to events</p>
          </div>
          <div className="flex items-center gap-3">
            <input
              type="file"
              ref={fileInputRef}
              accept=".wav,.ogg,.mp3"
              multiple
              className="hidden"
              onChange={e => e.target.files && handleFiles(e.target.files)}
            />
            <button
              onClick={() => fileInputRef.current?.click()}
              disabled={uploading}
              className="px-3 py-1.5 text-xs border border-game-primary text-game-primary rounded hover:bg-game-primary hover:text-black transition-colors disabled:opacity-40"
            >
              {uploading ? 'UPLOADING…' : '↑ UPLOAD SOUND'}
            </button>
          </div>
        </div>

        {error && (
          <div className="mx-6 mt-4 px-3 py-2 bg-game-danger/10 border border-game-danger text-game-danger text-xs rounded">
            {error}
            <button className="ml-3 underline" onClick={() => setError('')}>dismiss</button>
          </div>
        )}

        <div className="flex-1 flex overflow-hidden">
          {/* Left panel — Sound Library */}
          <div className="w-80 flex flex-col border-r border-game-border overflow-hidden">
            <div className="px-4 py-3 border-b border-game-border flex items-center gap-2">
              <span className="text-xs text-game-textDim tracking-widest">LIBRARY</span>
              <span className="ml-auto text-xs text-game-muted">{sounds.length} files</span>
            </div>

            {/* Search */}
            <div className="px-3 py-2 border-b border-game-border">
              <input
                className="w-full bg-game-dark border border-game-border rounded px-2 py-1 text-xs text-game-text placeholder:text-game-muted focus:outline-none focus:border-game-primary"
                placeholder="filter sounds…"
                value={search}
                onChange={e => setSearch(e.target.value)}
              />
            </div>

            {/* Drop zone */}
            <div
              className={`mx-3 my-2 border-2 border-dashed rounded p-3 text-center text-xs transition-colors ${dragOver ? 'border-game-primary text-game-primary bg-game-primary/5' : 'border-game-border text-game-muted'}`}
              onDragOver={e => { e.preventDefault(); setDragOver(true); }}
              onDragLeave={() => setDragOver(false)}
              onDrop={e => { e.preventDefault(); setDragOver(false); handleFiles(e.dataTransfer.files); }}
            >
              {uploading ? 'UPLOADING…' : 'DROP WAV / OGG / MP3 HERE'}
            </div>
            {uploadError && <p className="px-3 text-xs text-game-danger">{uploadError}</p>}

            {/* Sound list */}
            <div className="flex-1 overflow-y-auto">
              {loading ? (
                <div className="px-4 py-6 text-xs text-game-muted">Loading…</div>
              ) : filteredSounds.length === 0 ? (
                <div className="px-4 py-6 text-xs text-game-muted">No sounds found.</div>
              ) : filteredSounds.map(s => (
                <div
                  key={s.filename}
                  className={`flex items-center gap-2 px-3 py-2 border-b border-game-border/50 hover:bg-game-bgHover transition-colors group ${playing === s.filename ? 'bg-game-dark' : ''}`}
                >
                  {/* Play button */}
                  <button
                    onClick={() => playSound(s.filename)}
                    className="w-6 h-6 flex-shrink-0 flex items-center justify-center border border-game-border rounded text-xs hover:border-game-primary hover:text-game-primary transition-colors"
                    title={playing === s.filename ? 'Stop' : 'Play'}
                  >
                    {playing === s.filename ? '■' : '▶'}
                  </button>

                  <div className="flex-1 min-w-0">
                    <div className="text-xs text-game-text truncate">{s.filename}</div>
                    <div className="text-xs text-game-muted">{formatSize(s.size)}</div>
                  </div>

                  <button
                    onClick={() => handleDelete(s.filename)}
                    className="opacity-0 group-hover:opacity-100 text-xs text-game-muted hover:text-game-danger transition-all"
                    title="Delete"
                  >
                    ✕
                  </button>
                </div>
              ))}
            </div>
          </div>

          {/* Right panel — Event Assignments */}
          <div className="flex-1 flex flex-col overflow-hidden">
            <div className="px-4 py-3 border-b border-game-border flex items-center gap-4">
              <span className="text-xs text-game-textDim tracking-widest">SOUND EVENTS</span>
              <input
                className="ml-auto bg-game-dark border border-game-border rounded px-2 py-1 text-xs text-game-text placeholder:text-game-muted focus:outline-none focus:border-game-primary w-48"
                placeholder="filter events…"
                value={eventSearch}
                onChange={e => setEventSearch(e.target.value)}
              />
            </div>

            <div className="flex-1 overflow-y-auto">
              <table className="w-full text-xs">
                <thead className="sticky top-0 bg-game-bgCard border-b border-game-border">
                  <tr>
                    <th className="text-left px-4 py-2 text-game-textDim font-normal tracking-widest">EVENT</th>
                    <th className="text-left px-4 py-2 text-game-textDim font-normal tracking-widest">ASSIGNED SOUND</th>
                    <th className="text-left px-4 py-2 text-game-textDim font-normal tracking-widest w-12">PREVIEW</th>
                  </tr>
                </thead>
                <tbody>
                  {filteredEvents.map(event => {
                    const assigned = events[event] ?? '';
                    const isPatching = patchingEvent === event;
                    return (
                      <tr
                        key={event}
                        className="border-b border-game-border/50 hover:bg-game-bgHover transition-colors"
                        onDragOver={e => e.preventDefault()}
                        onDrop={e => {
                          e.preventDefault();
                          const fn = e.dataTransfer.getData('text/plain');
                          if (fn) handleEventChange(event, fn);
                        }}
                      >
                        <td className="px-4 py-2 text-game-primary font-mono">{event}</td>
                        <td className="px-4 py-2">
                          <select
                            value={assigned}
                            onChange={e => handleEventChange(event, e.target.value)}
                            disabled={isPatching}
                            className="bg-game-dark border border-game-border rounded px-2 py-0.5 text-xs text-game-text focus:outline-none focus:border-game-primary w-full max-w-xs disabled:opacity-50"
                          >
                            <option value="">— unassigned —</option>
                            {sounds.map(s => (
                              <option key={s.filename} value={s.filename}>{s.filename}</option>
                            ))}
                          </select>
                        </td>
                        <td className="px-4 py-2">
                          {assigned && (
                            <button
                              onClick={() => playSound(assigned)}
                              className="w-6 h-6 flex items-center justify-center border border-game-border rounded text-xs hover:border-game-primary hover:text-game-primary transition-colors"
                              title={`Preview ${assigned}`}
                            >
                              {playing === assigned ? '■' : '▶'}
                            </button>
                          )}
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>

              {/* Export */}
              <div className="px-4 py-4 border-t border-game-border">
                <button
                  onClick={() => {
                    const blob = new Blob([JSON.stringify(events, null, 2)], { type: 'application/json' });
                    const a = document.createElement('a');
                    a.href = URL.createObjectURL(blob);
                    a.download = 'sound-events.json';
                    a.click();
                  }}
                  className="px-3 py-1.5 text-xs border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-primary transition-colors"
                >
                  ↓ EXPORT sound-events.json
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
