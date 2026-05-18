'use client';
import { useEffect, useState } from 'react';
import { useParams, useRouter } from 'next/navigation';
import dynamic from 'next/dynamic';
import { useAuth } from '../../../lib/auth';
import { useWsConnected } from '../../../lib/socket';
import Sidebar from '../../../components/Sidebar';
import { type BehaviorTree } from '../../../lib/api';
import { getFolderName, readFromStore, writeToStore, downloadJson } from '../../../lib/folder-store';

// ReactFlow must be client-only
const BehaviorTreeEditor = dynamic(() => import('./BehaviorTreeEditor'), { ssr: false });

export default function BehaviorTreePage() {
  useAuth();
  const wsConnected = useWsConnected();
  const { id } = useParams<{ id: string }>();
  const router = useRouter();
  const [bt, setBt] = useState<BehaviorTree | null>(null);
  const [dirty, setDirty] = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState('');
  const folderName = getFolderName();

  useEffect(() => {
    if (!id) return;
    const data = readFromStore(id);
    if (data) {
      setBt(data);
    } else {
      setError('Tree not found in loaded folder. Go back and open the behaviortrees folder first.');
    }
  }, [id]);

  function handleChange(updated: BehaviorTree) {
    setBt(updated);
    setDirty(true);
  }

  async function handleSave() {
    if (!bt || !id) return;
    setSaving(true);
    try {
      writeToStore(id, bt);
      await downloadJson(id, bt);
      setDirty(false);
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setSaving(false);
    }
  }

  function handleNewTree() {
    const name = window.prompt('New behavior tree name (no spaces, no .json):');
    if (!name || !name.trim()) return;
    const newId = name.trim().replace(/[^a-z0-9_-]/gi, '_').toLowerCase();
    const rootId = 'root_selector';
    const newBt: BehaviorTree = {
      version: 1,
      id: newId,
      rootId,
      nodes: {
        [rootId]: { type: 'Selector', label: 'Root', children: [], props: {} },
      },
      blackboard: [],
      positions: { [rootId]: { x: 400, y: 200 } },
    };
    writeToStore(newId, newBt);
    router.push(`/behavior-trees/${newId}`);
  }

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar wsConnected={wsConnected} />
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {/* Toolbar */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 12, padding: '10px 16px', borderBottom: '1px solid #2d3748', background: '#0d1117', flexShrink: 0 }}>
          <h1 style={{ color: '#22c55e', fontFamily: 'monospace', fontSize: 14, letterSpacing: 3, fontWeight: 700, margin: 0 }}>
            BEHAVIOR TREE: {id}
          </h1>
          {folderName && (
            <span style={{ fontFamily: 'monospace', fontSize: 10, color: '#22c55e', border: '1px solid #22c55e44', padding: '2px 8px', letterSpacing: 1 }}>
              📁 {folderName}
            </span>
          )}
          <div style={{ flex: 1 }} />
          {error && <span style={{ color: '#f87171', fontSize: 11 }}>{error}</span>}
          {dirty && <span style={{ color: '#f59e0b', fontSize: 11 }}>● unsaved</span>}
          {folderName && (
            <button
              onClick={handleNewTree}
              style={{
                padding: '6px 14px', background: '#161b22', border: '1px solid #4a5568',
                color: '#a0aec0', fontFamily: 'monospace', fontSize: 12,
                letterSpacing: 2, cursor: 'pointer',
              }}
            >
              + NEW TREE
            </button>
          )}
          <button
            onClick={handleSave}
            disabled={saving || !dirty}
            style={{
              padding: '6px 14px', background: dirty ? '#22c55e' : '#1a2a1a', border: '1px solid #22c55e',
              color: dirty ? '#000' : '#4a5568', fontFamily: 'monospace', fontSize: 12, fontWeight: 700,
              letterSpacing: 2, cursor: dirty ? 'pointer' : 'default', transition: 'all 0.15s',
            }}
          >
            {saving ? 'SAVING…' : 'SAVE'}
          </button>
        </div>

        {/* Editor canvas */}
        <div style={{ flex: 1, overflow: 'hidden' }}>
          {!bt ? (
            <div style={{ padding: 32, color: '#718096', fontFamily: 'monospace', fontSize: 13 }}>
              {error ? `Error: ${error}` : 'Loading…'}
            </div>
          ) : (
            <BehaviorTreeEditor bt={bt} onChange={handleChange} />
          )}
        </div>
      </div>
    </div>
  );
}

// ReactFlow must be client-only
