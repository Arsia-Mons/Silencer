'use client';
import { useEffect, useState } from 'react';
import Link from 'next/link';
import { useRouter } from 'next/navigation';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { listActors, deleteActor } from '../../lib/api';

export default function ActorsPage() {
  useAuth();
  const router = useRouter();
  const [actors, setActors] = useState<string[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [newId, setNewId] = useState('');
  const [creating, setCreating] = useState(false);

  async function load() {
    try {
      setActors(await listActors());
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => { load(); }, []);

  async function handleCreate() {
    const id = newId.trim().toLowerCase().replace(/\s+/g, '-');
    if (!id || !/^[a-z0-9-]+$/.test(id)) return;
    setCreating(true);
    try {
      const { saveActor } = await import('../../lib/api');
      await saveActor(id, { id, sequences: {} });
      router.push(`/actors/${id}`);
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
      setCreating(false);
    }
  }

  async function handleDelete(id: string) {
    if (!confirm(`Delete actor "${id}"? This cannot be undone.`)) return;
    try {
      await deleteActor(id);
      setActors(prev => prev.filter(a => a !== id));
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar />
      <main className="flex-1 p-8">
        <div className="flex items-center justify-between mb-6">
          <div>
            <h1 className="text-2xl font-bold tracking-widest text-game-primary">ACTOR EDITOR</h1>
            <p className="text-game-textDim text-sm mt-1">Manage actor definitions — animations, hitboxes, properties</p>
          </div>
          <Link href="/actors/sprites" className="px-4 py-2 border border-game-border text-game-textDim hover:text-game-text text-sm tracking-wider">
            [ SPRITE BROWSER ]
          </Link>
        </div>

        {error && <div className="mb-4 p-3 bg-game-danger/20 border border-game-danger text-game-danger text-sm">{error}</div>}

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

        {loading ? (
          <div className="text-game-textDim text-sm">Loading…</div>
        ) : actors.length === 0 ? (
          <div className="text-game-textDim text-sm border border-game-border p-8 text-center">
            No actor definitions found. Create one above or place <code>.json</code> files in <code>shared/assets/actordefs/</code>.
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
