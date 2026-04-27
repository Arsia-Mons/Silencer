'use client';
import { useEffect, useState } from 'react';
import Link from 'next/link';
import { useRouter } from 'next/navigation';
import { useAuth } from '../../lib/auth';
import { useWsConnected } from '../../lib/socket';
import Sidebar from '../../components/Sidebar';
import { listBehaviorTrees, deleteBehaviorTree, saveBehaviorTree, type BehaviorTree } from '../../lib/api';
import {
  openFolder, clearFolder, getFolderHandle,
  listJsonFiles, writeJson, deleteJson,
} from '../../lib/folder-store';

export default function BehaviorTreesPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const router = useRouter();
  const [trees, setTrees] = useState<string[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [newId, setNewId] = useState('');
  const [creating, setCreating] = useState(false);
  const [folderName, setFolderName] = useState<string | null>(null);

  async function load(dir?: FileSystemDirectoryHandle | null) {
    setLoading(true);
    const h = dir !== undefined ? dir : getFolderHandle();
    try {
      setTrees(h ? await listJsonFiles(h) : await listBehaviorTrees());
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => { load(); }, []);

  async function handleOpenFolder() {
    const h = await openFolder();
    if (!h) return;
    setFolderName(h.name);
    await load(h);
  }

  function handleCloseFolder() {
    clearFolder();
    setFolderName(null);
    load(null);
  }

  async function handleCreate() {
    const id = newId.trim().toLowerCase().replace(/\s+/g, '-');
    if (!id || !/^[a-z0-9-]+$/.test(id)) return;
    setCreating(true);
    const emptyTree: BehaviorTree = {
      version: 1, id,
      blackboard: [],
      rootId: 'root',
      nodes: { root: { type: 'Selector', label: 'Root', children: [], props: {} } },
      positions: {},
    };
    try {
      const h = getFolderHandle();
      if (h) {
        await writeJson(h, id, emptyTree);
      } else {
        await saveBehaviorTree(id, emptyTree);
      }
      router.push(`/behavior-trees/${id}`);
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
      setCreating(false);
    }
  }

  async function handleDelete(id: string) {
    if (!confirm(`Delete behavior tree "${id}"?`)) return;
    try {
      const h = getFolderHandle();
      if (h) {
        await deleteJson(h, id);
      } else {
        await deleteBehaviorTree(id);
      }
      setTrees(prev => prev.filter(t => t !== id));
    } catch (e: unknown) { setError(e instanceof Error ? e.message : String(e)); }
  }

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-8">
        <div className="mb-6 flex items-start justify-between">
          <div>
            <h1 className="text-2xl font-bold tracking-widest text-game-primary">BEHAVIOR TREES</h1>
            <p className="text-game-textDim text-sm mt-1">Visual AI behavior tree editor — define NPC decision logic</p>
          </div>
          <div className="flex items-center gap-3 mt-1">
            {folderName ? (
              <>
                <span className="font-mono text-xs text-game-primary border border-game-primary/40 px-2 py-1">
                  📁 {folderName}
                </span>
                <button
                  onClick={handleCloseFolder}
                  className="text-xs text-game-textDim border border-game-border px-2 py-1 hover:text-game-text hover:border-game-text transition-colors"
                >
                  × CLOSE FOLDER
                </button>
              </>
            ) : (
              <button
                onClick={handleOpenFolder}
                title="Open local shared/assets/behaviortrees/ folder — edits write directly to disk"
                className="text-xs font-bold tracking-wider border border-game-border px-3 py-1.5 text-game-textDim hover:text-game-primary hover:border-game-primary transition-colors"
              >
                📁 OPEN FOLDER
              </button>
            )}
          </div>
        </div>

        <div className="bg-game-bgCard border border-game-border p-4 mb-6 flex gap-3 items-center">
          <input
            className="flex-1 bg-game-bg border border-game-border px-3 py-2 text-sm text-game-text placeholder-game-textDim focus:outline-none focus:border-game-primary"
            placeholder="new-tree-id (e.g. guard-patrol)"
            value={newId}
            onChange={e => setNewId(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && handleCreate()}
          />
          <button
            onClick={handleCreate}
            disabled={creating || !newId.trim()}
            className="px-4 py-2 bg-game-primary text-black text-sm font-bold tracking-wider disabled:opacity-40"
          >
            {creating ? 'CREATING…' : '+ NEW TREE'}
          </button>
        </div>

        {error && <div className="text-game-danger text-sm mb-4">{error}</div>}

        {loading ? (
          <div className="text-game-textDim text-sm">Loading…</div>
        ) : trees.length === 0 ? (
          <div className="text-game-textDim text-sm border border-game-border p-8 text-center">
            No behavior trees found. {folderName ? 'No .json files in this folder.' : 'Create one above.'}
          </div>
        ) : (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {trees.map(id => (
              <div key={id} className="bg-game-bgCard border border-game-border p-4 flex items-center justify-between group hover:border-game-primary transition-colors">
                <Link href={`/behavior-trees/${id}`} className="flex-1 font-mono text-game-text hover:text-game-primary tracking-wider">
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
