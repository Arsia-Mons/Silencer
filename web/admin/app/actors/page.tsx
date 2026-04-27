'use client';
import { useEffect, useRef, useState } from 'react';
import Link from 'next/link';
import { useRouter } from 'next/navigation';
import { useAuth } from '../../lib/auth';
import { useWsConnected } from '../../lib/socket';
import Sidebar from '../../components/Sidebar';
import { listActors, deleteActor } from '../../lib/api';
import {
  loadFilesIntoStore, clearStore, listIds, deleteFromStore,
  writeToStore, getFolderName, isFolderLoaded,
} from '../../lib/actor-store';

export default function ActorsPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const router = useRouter();
  const folderInputRef = useRef<HTMLInputElement>(null);
  const [actors, setActors] = useState<string[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [newId, setNewId] = useState('');
  const [creating, setCreating] = useState(false);
  const [localFolder, setLocalFolder] = useState<string | null>(null);

  // On mount: if a folder was already loaded (e.g. back-navigation), show it
  useEffect(() => {
    if (isFolderLoaded()) {
      setActors(listIds());
      setLocalFolder(getFolderName());
      setLoading(false);
    } else {
      listActors()
        .then(ids => setActors(ids))
        .catch(e => setError(e instanceof Error ? e.message : String(e)))
        .finally(() => setLoading(false));
    }
  }, []);

  async function handleFolderPicked(e: React.ChangeEvent<HTMLInputElement>) {
    const files = e.target.files;
    if (!files || files.length === 0) return;
    await loadFilesIntoStore(files);
    setLocalFolder(getFolderName());
    setActors(listIds());
    setLoading(false);
    // reset input so re-picking same folder fires onChange again
    e.target.value = '';
  }

  function handleCloseFolder() {
    clearStore();
    setLocalFolder(null);
    setLoading(true);
    listActors()
      .then(ids => setActors(ids))
      .catch(e => setError(e instanceof Error ? e.message : String(e)))
      .finally(() => setLoading(false));
  }

  async function handleCreate() {
    const id = newId.trim().toLowerCase().replace(/\s+/g, '-');
    if (!id || !/^[a-z0-9-]+$/.test(id)) return;
    setCreating(true);
    try {
      if (isFolderLoaded()) {
        writeToStore(id, { id, sequences: {} });
        setActors(listIds());
        router.push(`/actors/${id}`);
      } else {
        const { saveActor } = await import('../../lib/api');
        await saveActor(id, { id, sequences: {} });
        router.push(`/actors/${id}`);
      }
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
      setCreating(false);
    }
  }

  async function handleDelete(id: string) {
    if (!confirm(`Delete actor "${id}"? This cannot be undone.`)) return;
    try {
      if (isFolderLoaded()) {
        deleteFromStore(id);
        setActors(listIds());
      } else {
        await deleteActor(id);
        setActors(prev => prev.filter(a => a !== id));
      }
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar wsConnected={wsConnected} />
      {/* hidden folder input */}
      <input
        ref={folderInputRef}
        type="file"
        // @ts-expect-error non-standard attribute
        webkitdirectory=""
        multiple
        className="hidden"
        onChange={handleFolderPicked}
      />
      <main className="flex-1 p-8">
        <div className="flex items-center justify-between mb-6">
          <div>
            <h1 className="text-2xl font-bold tracking-widest text-game-primary">ACTOR EDITOR</h1>
            <p className="text-game-textDim text-sm mt-1">Manage actor definitions — animations, hitboxes, properties</p>
          </div>
          <div className="flex items-center gap-3">
            {localFolder ? (
              <>
                <span className="text-xs text-game-warning tracking-wider border border-game-warning/40 px-2 py-1">
                  📁 {localFolder}
                </span>
                <button
                  onClick={handleCloseFolder}
                  className="px-3 py-2 border border-game-border text-game-textDim hover:text-game-danger text-sm tracking-wider"
                >
                  ✕ CLOSE
                </button>
              </>
            ) : (
              <button
                onClick={() => folderInputRef.current?.click()}
                className="px-4 py-2 border border-game-border text-game-textDim hover:text-game-text text-sm tracking-wider"
              >
                📁 OPEN FOLDER
              </button>
            )}
            <Link href="/actors/sprites" className="px-4 py-2 border border-game-border text-game-textDim hover:text-game-text text-sm tracking-wider">
              [ SPRITE BROWSER ]
            </Link>
          </div>
        </div>

        {/* Create new actor */}
        <div className="bg-game-bgCard border border-game-border p-4 mb-6 flex gap-3 items-center">
          <input
            className="flex-1 bg-game-bg border border-game-border px-3 py-2 text-sm text-game-text placeholder-game-textDim focus:outline-none focus:border-game-primary"
            placeholder="new-actor-id (e.g. enemy-drone)"
            value={newId}
            onChange={e => setNewId(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && handleCreate()}
          />
          <button
            onClick={handleCreate}
            disabled={creating || !newId.trim()}
            className="px-4 py-2 bg-game-primary text-black text-sm font-bold tracking-wider disabled:opacity-40"
          >
            {creating ? 'CREATING…' : '+ NEW ACTOR'}
          </button>
        </div>

        {error && <div className="text-game-danger text-sm mb-4">{error}</div>}

        {loading ? (
          <div className="text-game-textDim text-sm">Loading…</div>
        ) : actors.length === 0 ? (
          <div className="text-game-textDim text-sm border border-game-border p-8 text-center">
            {localFolder
              ? 'No .json files found in the selected folder.'
              : <>No actor definitions found. Create one above or place <code>.json</code> files in <code>shared/assets/actordefs/</code>.</>
            }
          </div>
        ) : (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {actors.map(id => (
              <div key={id} className="bg-game-bgCard border border-game-border p-4 flex items-center justify-between group hover:border-game-primary transition-colors">
                <Link href={`/actors/${id}`} className="flex-1 font-mono text-game-text hover:text-game-primary tracking-wider">
                  {id}
                </Link>
                <button
                  onClick={() => handleDelete(id)}
                  className="ml-4 text-game-danger opacity-0 group-hover:opacity-100 transition-opacity text-sm px-2 py-1 border border-game-danger hover:bg-game-danger/20"
                >
                  DEL
                </button>
              </div>
            ))}
          </div>
        )}
      </main>
    </div>
  );
}
