'use client';
import { useEffect, useState } from 'react';
import { useParams } from 'next/navigation';
import dynamic from 'next/dynamic';
import { useAuth } from '../../../lib/auth';
import { useWsConnected } from '../../../lib/socket';
import Sidebar from '../../../components/Sidebar';
import { getBehaviorTree, saveBehaviorTree, type BehaviorTree } from '../../../lib/api';

// ReactFlow must be client-only
const BehaviorTreeEditor = dynamic(() => import('./BehaviorTreeEditor'), { ssr: false });

export default function BehaviorTreePage() {
  useAuth();
  const wsConnected = useWsConnected();
  const { id } = useParams<{ id: string }>();
  const [bt, setBt] = useState<BehaviorTree | null>(null);
  const [dirty, setDirty] = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => {
    if (!id) return;
    getBehaviorTree(id)
      .then(data => setBt(data))
      .catch(e => setError(e instanceof Error ? e.message : String(e)));
  }, [id]);

  function handleChange(updated: BehaviorTree) {
    setBt(updated);
    setDirty(true);
  }

  async function handleSave() {
    if (!bt || !id) return;
    setSaving(true);
    try {
      await saveBehaviorTree(id, bt);
      setDirty(false);
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setSaving(false);
    }
  }

  function handleDownload() {
    if (!bt || !id) return;
    const json = JSON.stringify(bt, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${id}.json`;
    a.click();
    URL.revokeObjectURL(url);
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
          <div style={{ flex: 1 }} />
          {error && <span style={{ color: '#f87171', fontSize: 11 }}>{error}</span>}
          {dirty && <span style={{ color: '#f59e0b', fontSize: 11 }}>● unsaved</span>}
          <button
            onClick={handleDownload}
            disabled={!bt}
            title="Download JSON to commit to git (shared/assets/behaviortrees/)"
            style={{
              padding: '6px 14px', background: 'transparent', border: '1px solid #4a5568',
              color: bt ? '#a0aec0' : '#4a5568', fontFamily: 'monospace', fontSize: 12, fontWeight: 700,
              letterSpacing: 2, cursor: bt ? 'pointer' : 'default', transition: 'all 0.15s',
            }}
          >
            ↓ DOWNLOAD
          </button>
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
