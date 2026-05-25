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

interface PhysicsMaterialsFile {
  physicsMaterials: PhysicsMaterialEntry[];
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

function extractFirstJson(text: string): string {
  let depth = 0;
  let inString = false;
  let escape = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (escape) { escape = false; continue; }
    if (c === '\\' && inString) { escape = true; continue; }
    if (c === '"') { inString = !inString; continue; }
    if (inString) continue;
    if (c === '{') depth++;
    else if (c === '}') { depth--; if (depth === 0) return text.slice(0, i + 1); }
  }
  return text;
}

function parse(text: string): PhysicsMaterialsFile {
  return JSON.parse(extractFirstJson(text.trimStart().replace(/^\uFEFF/, ''))) as PhysicsMaterialsFile;
}

function serialize(data: PhysicsMaterialsFile): string {
  return JSON.stringify(data, null, 2) + '\n';
}

export default function PhysicsMaterialsPage() {
  useAuth();

  const fileInputRef = useRef<HTMLInputElement>(null);

  const [data, setData]         = useState<PhysicsMaterialsFile | null>(null);
  const [savedText, setSavedText] = useState<string | null>(null);
  const [fileName, setFileName]   = useState<string | null>(null);
  const [saveMsg, setSaveMsg]     = useState('');
  const [saveErr, setSaveErr]     = useState('');
  const [selected, setSelected]   = useState<string | null>(null);

  // Hydrate from store on mount
  useEffect(() => {
    if (store.isLoaded()) {
      const text = store.getText()!;
      try {
        setData(parse(text));
        setSavedText(text);
        setFileName(store.getFileName());
      } catch { /* stale store */ }
    }
  }, []);

  const currentText = data ? serialize(data) : null;
  const isDirty = currentText !== savedText;

  function handleFilePicked(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    file.text().then(text => {
      try {
        const parsed = parse(text);
        setData(parsed);
        setSavedText(text);
        setFileName(file.name);
        setSaveErr('');
        setSaveMsg('');
        store.load(file.name, text);
        setSelected(parsed.physicsMaterials[0]?.id ?? null);
      } catch (err) {
        setSaveErr(`Invalid JSON: ${err instanceof Error ? err.message : String(err)}`);
      }
      e.target.value = '';
    }).catch(() => setSaveErr('Failed to read file.'));
  }

  function handleClose() {
    store.clear();
    setData(null);
    setSavedText(null);
    setFileName(null);
    setSaveMsg('');
    setSaveErr('');
    setSelected(null);
  }

  const handleSave = useCallback(async () => {
    if (!data) return;
    const text = serialize(data);
    const name = fileName ?? 'physics_materials.json';
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
    setSavedText(text);
    store.setText(text);
    setSaveMsg('✓ Saved');
    setTimeout(() => setSaveMsg(''), 2500);
  }, [data, fileName]);

  // Ctrl+S
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === 's') { e.preventDefault(); handleSave(); }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [handleSave]);

  function patchMaterial(id: string, patch: Partial<PhysicsMaterialEntry>) {
    setData(prev => {
      if (!prev) return prev;
      const updated = {
        ...prev,
        physicsMaterials: prev.physicsMaterials.map(m => m.id === id ? { ...m, ...patch } : m),
      };
      store.setText(serialize(updated));
      return updated;
    });
    setSaveErr('');
    setSaveMsg('');
  }

  const mat = data?.physicsMaterials.find(m => m.id === selected) ?? null;

  return (
    <div className="flex h-screen bg-game-bg text-game-text font-mono overflow-hidden">
      <Sidebar />
      <div className="flex flex-col flex-1 overflow-hidden">
      {/* Single file input — always mounted so ref is stable regardless of which button triggers it */}
      <input ref={fileInputRef} type="file" accept=".json" className="hidden" onChange={handleFilePicked} />
        {/* Header */}
        <div className="flex items-center justify-between px-6 py-3 border-b border-game-border bg-game-bgCard shrink-0">
          <div className="flex items-center gap-3">
            <span className="text-game-textDim tracking-widest text-sm">⬡ PHYSICS MATERIALS</span>
            {fileName && (
              <span className="text-game-textDim text-xs opacity-60">{fileName}</span>
            )}
            {isDirty && <span className="text-game-warning text-xs">● unsaved</span>}
          </div>
          <div className="flex items-center gap-2">
            {saveErr && <span className="text-game-danger text-xs">{saveErr}</span>}
            {saveMsg && <span className="text-game-textDim text-xs">{saveMsg}</span>}
            {!data ? (
              <button
                onClick={() => fileInputRef.current?.click()}
                className="px-3 py-1 text-xs border border-game-border text-game-textDim hover:border-game-text hover:text-game-text transition-colors rounded"
              >
                OPEN FILE
              </button>
            ) : (
              <>
                <button
                  onClick={handleClose}
                  className="px-3 py-1 text-xs border border-game-border text-game-textDim hover:border-game-text hover:text-game-text transition-colors rounded"
                >
                  ✕ CLOSE
                </button>
                <button
                  onClick={handleSave}
                  disabled={!isDirty}
                  className="px-3 py-1 text-xs border border-game-border text-game-textDim hover:border-game-text hover:text-game-text transition-colors rounded disabled:opacity-40 disabled:cursor-not-allowed"
                >
                  {isDirty ? '● SAVE' : 'SAVE'}
                </button>
              </>
            )}
          </div>
        </div>

        {!data ? (
          <div className="flex flex-col items-center justify-center flex-1 gap-4 text-game-textDim">
            <p className="text-3xl opacity-20">⬡</p>
            <p className="text-sm">Open <span className="text-game-text">physics_materials.json</span> from your GAS folder</p>
            <button
              onClick={() => fileInputRef.current?.click()}
              className="px-4 py-2 text-xs border border-game-border text-game-textDim hover:border-game-text hover:text-game-text transition-colors rounded"
            >
              OPEN FILE
            </button>
          </div>
        ) : (
          <div className="flex flex-1 overflow-hidden">
            {/* Material list */}
            <div className="w-52 border-r border-game-border overflow-y-auto shrink-0 bg-game-bgCard">
              {data.physicsMaterials.map(m => (
                <button
                  key={m.id}
                  onClick={() => setSelected(m.id)}
                  className={`w-full text-left px-4 py-2 text-xs border-b border-game-border transition-colors ${
                    selected === m.id
                      ? 'bg-white/5 text-game-text border-l-2 border-l-game-textDim'
                      : 'text-game-textDim hover:text-game-text hover:bg-white/[0.03]'
                  }`}
                >
                  <div className="font-medium">{m.id}</div>
                  <div className="text-[10px] opacity-50 mt-0.5">
                    f:{m.friction ?? 1.0} s:{m.speedMult ?? 1.0}
                  </div>
                </button>
              ))}
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
                        <input
                          type="number"
                          step="0.05"
                          min="0.05"
                          max="5"
                          value={mat.friction ?? 1.0}
                          onChange={e => patchMaterial(mat.id, { friction: parseFloat(e.target.value) || 1.0 })}
                          className="bg-game-dark border border-game-border text-game-text text-xs font-mono rounded px-2 py-1 focus:outline-none focus:border-game-textDim"
                        />
                      </label>
                      <label className="flex flex-col gap-1">
                        <span className="text-[10px] text-game-textDim">SPEED MULT</span>
                        <span className="text-[9px] text-game-textDim opacity-60">run speed multiplier — 1.0 normal · &lt;1 slower · &gt;1 faster</span>
                        <input
                          type="number"
                          step="0.05"
                          min="0.1"
                          max="3"
                          value={mat.speedMult ?? 1.0}
                          onChange={e => patchMaterial(mat.id, { speedMult: parseFloat(e.target.value) || 1.0 })}
                          className="bg-game-dark border border-game-border text-game-text text-xs font-mono rounded px-2 py-1 focus:outline-none focus:border-game-textDim"
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
                      {SOUND_FIELDS.map(field => (
                        <label key={field} className="flex flex-col gap-1">
                          <span className="text-[10px] text-game-textDim">{SOUND_LABELS[field]}</span>
                          <input
                            type="text"
                            value={(mat[field] as string) ?? ''}
                            onChange={e => patchMaterial(mat.id, { [field]: e.target.value })}
                            placeholder={field.includes('Crouch') || field.includes('Stair') ? '← falls back to walk' : ''}
                            className="bg-game-dark border border-game-border text-game-text text-xs font-mono rounded px-2 py-1 focus:outline-none focus:border-game-textDim placeholder:text-game-border"
                          />
                        </label>
                      ))}
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
