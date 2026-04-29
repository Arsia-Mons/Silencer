'use client';
/**
 * Actor editor — tabbed interface for:
 * C3: Animation sequence builder + timeline
 * C4: Live preview canvas (60fps playback)
 * C5: Hitbox editor (per-frame AABB drawing)
 * C7: Actor properties + export/save
 */
import { Suspense, useEffect, useState } from 'react';
import Link from 'next/link';
import { useParams, useRouter, useSearchParams } from 'next/navigation';
import { useAuth } from '../../../lib/auth';
import { useWsConnected } from '../../../lib/socket';
import Sidebar from '../../../components/Sidebar';
import { type ActorDef } from '../../../lib/api';
import {
  readFromStore, writeToStore, downloadJson, getFolderName,
} from '../../../lib/actor-store';
import AnimationTab from './AnimationTab';
import HitboxTab from './HitboxTab';
import PropsTab from './PropsTab';

type Tab = 'animation' | 'hitbox' | 'props';
const VALID_TABS: Tab[] = ['animation', 'hitbox', 'props'];

// useSearchParams() requires Suspense in Next.js 14 production builds.
// Inner component reads search params; outer wraps it in Suspense.
function ActorEditorInner() {
  useAuth();
  const wsConnected = useWsConnected();
  const { id } = useParams() as { id: string };
  const router = useRouter();
  const searchParams = useSearchParams();
  const [def, setDef]       = useState<ActorDef | null>(null);
  const [dirty, setDirty]   = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError]   = useState('');
  const folderName = getFolderName();

  const rawTab = searchParams.get('tab') as Tab | null;
  const tab: Tab = rawTab && VALID_TABS.includes(rawTab) ? rawTab : 'animation';

  function setTab(t: Tab) {
    const params = new URLSearchParams(searchParams.toString());
    params.set('tab', t);
    router.replace(`?${params.toString()}`);
  }

  useEffect(() => {
    const stored = readFromStore(id);
    if (stored) {
      setDef(stored);
    } else {
      setError('Actor not found in loaded folder. Go back and open the actordefs folder first.');
    }
  }, [id]);

  function updateDef(patch: Partial<ActorDef>) {
    setDef(prev => prev ? { ...prev, ...patch } : prev);
    setDirty(true);
  }

  async function handleSave() {
    if (!def) return;
    setSaving(true);
    try {
      writeToStore(id, def);
      await downloadJson(id, def);
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
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 flex flex-col min-h-0">
        {/* Top bar */}
        <div className="flex items-center gap-4 px-8 py-4 border-b border-game-border">
          <Link href="/actors" className="text-game-textDim hover:text-game-text text-sm">← ACTORS</Link>
          <h1 className="text-xl font-bold tracking-widest text-game-primary font-mono flex-1">{id}</h1>
          {folderName && (
            <span className="text-xs text-game-warning tracking-wider border border-game-warning/40 px-2 py-1">
              📁 {folderName}
            </span>
          )}
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

        <div className="flex flex-1 min-h-0 overflow-auto">
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

export default function ActorEditorPage() {
  return (
    <Suspense>
      <ActorEditorInner />
    </Suspense>
  );
}
