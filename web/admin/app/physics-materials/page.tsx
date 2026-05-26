'use client';
import { useState, useCallback, useEffect, useRef } from 'react';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import * as store from '../../lib/physics-materials-store';

interface PhysicsMaterialEntry {
  id: string;
  friction: number;
  speedMult: number;
  footstepL: string;
  footstepR: string;
  footstepCrouchL: string;
  footstepCrouchR: string;
  footstepStairL: string;
  footstepStairR: string;
}

const SOUND_FIELDS: (keyof PhysicsMaterialEntry)[] = [
  'footstepL', 'footstepR',
  'footstepCrouchL', 'footstepCrouchR',
  'footstepStairL', 'footstepStairR',
];

const SOUND_LABELS: Record<string, string> = {
  footstepL: 'Walk L', footstepR: 'Walk R',
  footstepCrouchL: 'Crouch L', footstepCrouchR: 'Crouch R',
  footstepStairL: 'Stair L', footstepStairR: 'Stair R',
};

function parseMaterial(text: string): PhysicsMaterialEntry {
  const raw = text.trimStart().replace(/^\uFEFF/, '');
  return JSON.parse(raw) as PhysicsMaterialEntry;
}

function serializeMaterial(mat: PhysicsMaterialEntry): string {
  return JSON.stringify(mat, null, 2) + '\n';
}

export default function PhysicsMaterialsPage() {
  useAuth();

  const folderInputRef = useRef<HTMLInputElement>(null);

  // materials keyed by id
  const [materials, setMaterials]     = useState<Record<string, PhysicsMaterialEntry>>({});
  const [savedTexts, setSavedTexts]   = useState<Record<string, string>>({});
  const [folderName, setFolderName]   = useState<string | null>(null);
  const [selected, setSelected]       = useState<string | null>(null);
  const [saveMsg, setSaveMsg]         = useState('');
  const [saveErr, setSaveErr]         = useState('');
  const [sounds, setSounds]           = useState<string[]>([]);

  // Fetch available sound cues for dropdowns
  useEffect(() => {
    const token = localStorage.getItem('zs_token');
    fetch('/api/sound-cues', { headers: token ? { authorization: `Bearer ${token}` } : {} })
      .then(r => r.ok ? r.json() : [])
      .then((list: { id: string }[]) => setSounds(list.map(s => s.id).sort()))
      .catch(() => {});
  }, []);

  // Hydrate from store on mount
  useEffect(() => {
    if (store.isLoaded()) {
      const files = store.getAllFiles();
      const mats: Record<string, PhysicsMaterialEntry> = {};
      const saved: Record<string, string> = {};
      for (const [id, text] of Object.entries(files)) {
        try { mats[id] = parseMaterial(text); saved[id] = text; } catch { /* skip */ }
      }
      if (Object.keys(mats).length > 0) {
        setMaterials(mats);
        setSavedTexts(saved);
        setFolderName(store.getFolderName());
        setSelected(Object.keys(mats).sort()[0]);
      }
    }
  }, []);

  const isLoaded = Object.keys(materials).length > 0;
  const mat = selected ? materials[selected] ?? null : null;
  const matText = mat ? serializeMaterial(mat) : null;
  const isDirty = selected ? matText !== savedTexts[selected] : false;
  const anyDirty = Object.keys(materials).some(id => serializeMaterial(materials[id]) !== savedTexts[id]);

  async function handleFolderPicked(e: React.ChangeEvent<HTMLInputElement>) {
    const picked = e.target.files;
    if (!picked || picked.length === 0) return;
    const first = picked[0] as { webkitRelativePath?: string } & File;
    const name = first?.webkitRelativePath?.split('/')[0] ?? 'physics_materials';
    const mats: Record<string, PhysicsMaterialEntry> = {};
    const saved: Record<string, string> = {};
    await Promise.all(
      Array.from(picked)
        .filter(f => f.name.endsWith('.json'))
        .map(f => f.text().then(text => {
          try {
            const m = parseMaterial(text);
            if (m.id) { mats[m.id] = m; saved[m.id] = text; }
          } catch { /* skip malformed */ }
        }))
    );
    e.target.value = '';
    if (Object.keys(mats).length === 0) { setSaveErr('No valid material files found.'); return; }
    setMaterials(mats);
    setSavedTexts(saved);
    setFolderName(name);
    setSaveErr('');
    setSaveMsg('');
    store.loadFolder(name, saved);
    setSelected(Object.keys(mats).sort()[0]);
  }

  function handleClose() {
    store.clear();
    setMaterials({});
    setSavedTexts({});
    setFolderName(null);
    setSelected(null);
    setSaveMsg('');
    setSaveErr('');
  }

  const handleSave = useCallback(async () => {
    if (!mat || !selected) return;
    const text = serializeMaterial(mat);
    const name = `${selected}.json`;
    if (typeof window !== 'undefined' && 'showSaveFilePicker' in window) {
      try {
        const handle = await (window as unknown as {
          showSaveFilePicker: (o: unknown) => Promise<FileSystemFileHandle>
        }).showSaveFilePicker({
          suggestedName: name,
          types: [{ description: 'JSON', accept: { 'application/json': ['.json'] } }],
        });
        const writable = await handle.createWritable();
        await writable.write(text);
        await writable.close();
      } catch { return; }
    } else {
      const blob = new Blob([text], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url; a.download = name; a.click();
      URL.revokeObjectURL(url);
    }
    setSavedTexts(prev => ({ ...prev, [selected]: text }));
    store.setFile(selected, text);
    setSaveMsg('✓ Saved');
    setTimeout(() => setSaveMsg(''), 2500);
  }, [mat, selected]);

  // Ctrl+S
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === 's') { e.preventDefault(); handleSave(); }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [handleSave]);

  function patchMaterial(id: string, patch: Partial<PhysicsMaterialEntry>) {
    setMaterials(prev => {
      const updated = { ...prev, [id]: { ...prev[id], ...patch } };
      store.setFile(id, serializeMaterial(updated[id]));
      return updated;
    });
    setSaveErr('');
    setSaveMsg('');
  }

  const sortedIds = Object.keys(materials).sort();

  return (
    <div className="flex h-screen bg-game-bg text-game-text font-mono overflow-hidden">
      <Sidebar />
      <div className="flex flex-col flex-1 overflow-hidden">
        <input ref={folderInputRef} type="file" className="hidden"
          // @ts-expect-error webkitdirectory is non-standard
          webkitdirectory=""
          onChange={handleFolderPicked}
        />

        {/* Header */}
        <div className="flex items-center justify-between px-6 py-3 border-b border-game-border bg-game-bgCard shrink-0">
          <div className="flex items-center gap-3">
            <span className="text-game-textDim tracking-widest text-sm">⬡ PHYSICS MATERIALS</span>
            {folderName && <span className="text-game-textDim text-xs opacity-60">{folderName}/</span>}
            {anyDirty && <span className="text-game-warning text-xs">● unsaved</span>}
          </div>
          <div className="flex items-center gap-2">
            {saveErr && <span className="text-game-danger text-xs">{saveErr}</span>}
            {saveMsg && <span className="text-game-textDim text-xs">{saveMsg}</span>}
            {!isLoaded ? (
              <button onClick={() => folderInputRef.current?.click()}
                className="px-3 py-1 text-xs border border-game-border text-game-textDim hover:border-game-text hover:text-game-text transition-colors rounded">
                OPEN FOLDER
              </button>
            ) : (
              <>
                <button onClick={handleClose}
                  className="px-3 py-1 text-xs border border-game-border text-game-textDim hover:border-game-text hover:text-game-text transition-colors rounded">
                  ✕ CLOSE
                </button>
                <button onClick={handleSave} disabled={!isDirty}
                  className="px-3 py-1 text-xs border border-game-border text-game-textDim hover:border-game-text hover:text-game-text transition-colors rounded disabled:opacity-40 disabled:cursor-not-allowed">
                  {isDirty ? '● SAVE' : 'SAVE'}
                </button>
              </>
            )}
          </div>
        </div>

        {!isLoaded ? (
          <div className="flex flex-col items-center justify-center flex-1 gap-4 text-game-textDim">
            <p className="text-3xl opacity-20">⬡</p>
            <p className="text-sm">Open the <span className="text-game-text">physics_materials/</span> folder from your GAS directory</p>
            <button onClick={() => folderInputRef.current?.click()}
              className="px-4 py-2 text-xs border border-game-border text-game-textDim hover:border-game-text hover:text-game-text transition-colors rounded">
              OPEN FOLDER
            </button>
          </div>
        ) : (
          <div className="flex flex-1 overflow-hidden">
            {/* Material list */}
            <div className="w-52 border-r border-game-border overflow-y-auto shrink-0 bg-game-bgCard">
              {sortedIds.map(id => {
                const m = materials[id];
                const dirty = serializeMaterial(m) !== savedTexts[id];
                return (
                  <button key={id} onClick={() => setSelected(id)}
                    className={`w-full text-left px-4 py-2 text-xs border-b border-game-border transition-colors ${
                      selected === id
                        ? 'bg-white/5 text-game-text border-l-2 border-l-game-textDim'
                        : 'text-game-textDim hover:text-game-text hover:bg-white/[0.03]'
                    }`}>
                    <div className="font-medium flex items-center gap-1">
                      {dirty && <span className="text-game-warning">●</span>}{id}
                    </div>
                    <div className="text-[10px] opacity-50 mt-0.5">
                      f:{m.friction ?? 1.0} s:{m.speedMult ?? 1.0}
                    </div>
                  </button>
                );
              })}
            </div>

            {/* Detail panel */}
            <div className="flex-1 overflow-y-auto p-6">
              {!mat ? (
                <p className="text-game-textDim text-sm">Select a material</p>
              ) : (
                <div className="max-w-lg space-y-6">
                  <h2 className="text-game-text tracking-widest text-sm">{mat.id}</h2>

                  {/* Physics */}
                  <section>
                    <h3 className="text-game-textDim text-[10px] tracking-widest mb-3 uppercase">Physics</h3>
                    <div className="grid grid-cols-2 gap-4">
                      <label className="flex flex-col gap-1">
                        <span className="text-[10px] text-game-textDim">FRICTION</span>
                        <span className="text-[9px] text-game-textDim opacity-60">decel multiplier — 1.0 normal · &lt;1 slippery · &gt;1 sticky</span>
                        <input type="number" step="0.05" min="0.05" max="5"
                          value={mat.friction ?? 1.0}
                          onChange={e => patchMaterial(mat.id, { friction: parseFloat(e.target.value) || 1.0 })}
                          className="bg-game-bg border border-game-border text-game-text text-xs font-mono rounded px-2 py-1 focus:outline-none focus:border-game-textDim"
                        />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className="text-[10px] text-game-textDim">SPEED MULT</span>
                        <span className="text-[9px] text-game-textDim opacity-60">run speed multiplier — 1.0 normal · &lt;1 slower · &gt;1 faster</span>
                        <input type="number" step="0.05" min="0.1" max="3"
                          value={mat.speedMult ?? 1.0}
                          onChange={e => patchMaterial(mat.id, { speedMult: parseFloat(e.target.value) || 1.0 })}
                          className="bg-game-bg border border-game-border text-game-text text-xs font-mono rounded px-2 py-1 focus:outline-none focus:border-game-textDim"
                        />
                      </label>
                    </div>
                  </section>

                  {/* Sounds */}
                  <section>
                    <h3 className="text-game-textDim text-[10px] tracking-widest mb-3 uppercase">Footstep Sounds</h3>
                    <p className="text-[9px] text-game-textDim opacity-60 mb-3">
                      Crouch/Stair fields fall back to Walk L/R if left empty.
                    </p>
                    <div className="grid grid-cols-2 gap-3">
                      {SOUND_FIELDS.map(field => {
                        const val = (mat[field] as string) ?? '';
                        const isFallback = field.includes('Crouch') || field.includes('Stair');
                        return (
                          <label key={field} className="flex flex-col gap-1">
                            <span className="text-[10px] text-game-textDim">{SOUND_LABELS[field]}</span>
                            <select
                              value={val}
                              onChange={e => patchMaterial(mat.id, { [field]: e.target.value })}
                              className="bg-game-bg border border-game-border text-game-text text-xs font-mono rounded px-2 py-1 focus:outline-none focus:border-game-textDim"
                            >
                              <option value="">{isFallback ? '← falls back to walk' : '— none —'}</option>
                              {sounds.map(s => <option key={s} value={s}>{s}</option>)}
                            </select>
                          </label>
                        );
                      })}
                    </div>
                  </section>
                </div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
