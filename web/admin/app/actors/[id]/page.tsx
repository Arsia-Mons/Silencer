'use client';
/**
 * Actor editor — tabbed interface for:
 * C3: Animation sequence builder + timeline
 * C4: Live preview canvas (60fps playback)
 * C5: Hitbox editor (per-frame AABB drawing)
 * C7: Actor properties + export/save
 */
import { useEffect, useRef, useState, useCallback } from 'react';
import Link from 'next/link';
import { useParams } from 'next/navigation';
import { useAuth } from '../../../lib/auth';
import Sidebar from '../../../components/Sidebar';
import { getActor, saveActor, type ActorDef } from '../../../lib/api';
import AnimationTab from './AnimationTab';
import HitboxTab from './HitboxTab';
import PropsTab from './PropsTab';

type Tab = 'animation' | 'hitbox' | 'props';

export default function ActorEditorPage() {
  useAuth();
  const { id } = useParams() as { id: string };
  const [def, setDef]       = useState<ActorDef | null>(null);
  const [dirty, setDirty]   = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError]   = useState('');
  const [tab, setTab]       = useState<Tab>('animation');

  useEffect(() => {
    getActor(id)
      .then(d => setDef(d))
      .catch(e => setError(e.message));
  }, [id]);

  function updateDef(patch: Partial<ActorDef>) {
    setDef(prev => prev ? { ...prev, ...patch } : prev);
    setDirty(true);
  }

  async function handleSave() {
    if (!def) return;
    setSaving(true);
    try {
      await saveActor(id, def);
      setDirty(false);
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setSaving(false);
    }
  }

  const TABS: { key: Tab; label: string }[] = [
    { key: 'animation', label: '[ ANIMATION ]' },
    { key: 'hitbox',    label: '[ HITBOXES ]' },
    { key: 'props',     label: '[ PROPERTIES ]' },
  ];

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar wsConnected={false} />
      <main className="flex-1 flex flex-col min-h-0">
        {/* Top bar */}
        <div className="flex items-center gap-4 px-8 py-4 border-b border-game-border">
          <Link href="/actors" className="text-game-textDim hover:text-game-text text-sm">← ACTORS</Link>
          <h1 className="text-xl font-bold tracking-widest text-game-primary font-mono flex-1">{id}</h1>
          {dirty && <span className="text-game-warning text-xs tracking-widest">UNSAVED CHANGES</span>}
          {error && <span className="text-game-danger text-xs">{error}</span>}
          <button
            onClick={handleSave}
            disabled={saving || !dirty}
            className="px-4 py-2 bg-game-primary text-black text-sm font-bold tracking-wider disabled:opacity-40"
          >
            {saving ? 'SAVING…' : 'SAVE'}
          </button>
        </div>

        {/* Tab bar */}
        <div className="flex border-b border-game-border px-8">
          {TABS.map(t => (
            <button
              key={t.key}
              onClick={() => setTab(t.key)}
              className={`px-4 py-3 text-xs tracking-widest border-b-2 transition-colors ${
                tab === t.key
                  ? 'border-game-primary text-game-primary'
                  : 'border-transparent text-game-textDim hover:text-game-text'
              }`}
            >
              {t.label}
            </button>
          ))}
        </div>

        {/* Tab content */}
        <div className="flex-1 min-h-0 overflow-auto">
          {!def ? (
            <div className="p-8 text-game-textDim text-sm">Loading…</div>
          ) : tab === 'animation' ? (
            <AnimationTab actorId={id} def={def} onChange={updateDef} />
          ) : tab === 'hitbox' ? (
            <HitboxTab actorId={id} def={def} onChange={updateDef} />
          ) : (
            <PropsTab def={def} onChange={updateDef} />
          )}
        </div>
      </main>
    </div>
  );
}
