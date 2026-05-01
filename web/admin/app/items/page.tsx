'use client';
import React, { useRef, useState, useEffect } from 'react';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { useWsConnected } from '../../lib/socket';
import * as gasStore from '../../lib/gas-store';

// ── Types ─────────────────────────────────────────────────────────────────────

interface ItemDef {
  id: string;
  enumId: number;
  name: string;
  description: string;
  price: number;
  repairPrice: number;
  spriteBank: number;
  spriteIndex: number;
  techChoice: number;
  techSlots: number;
  agencyRestriction: number;  // -1=all, 0=Noxis, 1=Lazarus, 2=Caliber, 3=Static, 4=Blackrose
  spawnAmmo?: number;
  pickupAmmo?: number;
  maxAmmo?: number;
  spawnInventoryCount?: number;
  healAmount?: number;
  poisonDose?: number;
  [key: string]: unknown;
}

interface FolderState {
  items: ItemDef[];
  raw: Record<string, unknown>;
  folderName: string;
}

// ── Constants ─────────────────────────────────────────────────────────────────

const AGENCY_NAMES: Record<number, string> = {
  [-1]: 'All Agencies',
  0: 'Noxis',
  1: 'Lazarus',
  2: 'Caliber',
  3: 'Static',
  4: 'Blackrose',
};

const NUMERIC_FIELDS: { key: string; label: string }[] = [
  { key: 'price',               label: 'Price (₢)' },
  { key: 'repairPrice',         label: 'Repair Price (₢)' },
  { key: 'techSlots',           label: 'Tech Slots' },
  { key: 'spawnAmmo',           label: 'Spawn Ammo' },
  { key: 'pickupAmmo',          label: 'Pickup Ammo' },
  { key: 'maxAmmo',             label: 'Max Ammo' },
  { key: 'spawnInventoryCount', label: 'Spawn Inventory Count' },
  { key: 'healAmount',          label: 'Heal Amount' },
  { key: 'poisonDose',          label: 'Poison Dose' },
];

// ── Component ─────────────────────────────────────────────────────────────────

export default function ItemsPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const folderInputRef = useRef<HTMLInputElement>(null);
  const [folder, setFolder] = useState<FolderState | null>(null);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [dirty, setDirty] = useState(false);
  const [error, setError] = useState('');
  const [saveMsg, setSaveMsg] = useState('');

  // ── Load from GAS store on mount ─────────────────────────────────────────────
  useEffect(() => {
    if (!gasStore.isLoaded()) return;
    const raw = gasStore.getFile('items');
    if (!raw) return;
    try {
      const parsed = JSON.parse(raw) as Record<string, unknown>;
      const items = (parsed.items as ItemDef[]) ?? [];
      setFolder({ items, raw: parsed, folderName: gasStore.getFolderName() ?? 'gas' });
      setSelectedId(items[0]?.id ?? null);
    } catch { /* ignore */ }
  }, []);

  // ── Folder pick ───────────────────────────────────────────────────────────────
  async function handleFolderPick(e: React.ChangeEvent<HTMLInputElement>) {
    const files = Array.from(e.target.files ?? []);
    const folderName = files[0]?.webkitRelativePath.split('/')[0] ?? 'gas';
    const iFile = files.find(f => f.name === 'items.json');
    if (!iFile) { setError('items.json not found in selected folder.'); return; }
    const itemsText = await iFile.text();
    try {
      const parsed = JSON.parse(itemsText) as Record<string, unknown>;
      const items = (parsed.items as ItemDef[]) ?? [];
      setFolder({ items, raw: parsed, folderName });
      setSelectedId(items[0]?.id ?? null);
      setDirty(false);
      setError('');
      const existingFiles = gasStore.getAllFiles();
      gasStore.loadFolder(folderName, { ...existingFiles, items: itemsText });
    } catch (err) {
      setError(String(err));
    }
  }

  function closeFolder() {
    gasStore.clear();
    setFolder(null);
    setSelectedId(null);
    setDirty(false);
  }

  // ── Selected item ─────────────────────────────────────────────────────────────
  const item = folder?.items.find(w => w.id === selectedId) ?? null;

  function updateItem(patch: Partial<ItemDef>) {
    if (!folder || !item) return;
    const updated = folder.items.map(i => i.id === item.id ? { ...i, ...patch } : i);
    const newRaw = { ...folder.raw, items: updated };
    setFolder({ ...folder, items: updated, raw: newRaw });
    gasStore.setFile('items', JSON.stringify(newRaw, null, 2));
    setDirty(true);
    setSaveMsg('');
  }

  function updateNumeric(key: string, val: string) {
    const n = val === '' ? undefined : Number(val);
    if (n !== undefined && isNaN(n)) return;
    updateItem({ [key]: n === undefined ? undefined : n } as Partial<ItemDef>);
  }

  // ── Save ──────────────────────────────────────────────────────────────────────
  async function handleSave() {
    if (!folder) return;
    const text = JSON.stringify(folder.raw, null, 2);
    const blob = new Blob([text], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'items.json';
    a.click();
    URL.revokeObjectURL(a.href);
    setDirty(false);
    setSaveMsg('Downloaded items.json');
  }

  // ── Render ────────────────────────────────────────────────────────────────────
  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 flex flex-col overflow-hidden">

        {/* Header */}
        <div className="px-6 py-4 border-b border-game-border flex items-center gap-4">
          <h1 className="text-game-primary font-mono text-xl tracking-widest">🎒 ITEMS</h1>
          {folder && (
            <>
              <span className="text-game-textDim font-mono text-xs">{folder.folderName}/items.json</span>
              <button onClick={closeFolder} className="ml-auto text-game-muted hover:text-game-text font-mono text-xs">✕ CLOSE</button>
            </>
          )}
        </div>

        {!folder ? (
          /* ── Folder picker ── */
          <div className="flex-1 flex flex-col items-center justify-center gap-4 p-8">
            <p className="text-game-textDim font-mono text-sm text-center max-w-sm">
              Open your GAS folder to edit items. The folder should contain <span className="text-game-primary">items.json</span>.
            </p>
            <button
              className="px-6 py-3 border border-game-primary text-game-primary font-mono text-sm tracking-widest hover:bg-game-primary hover:text-black transition-colors"
              onClick={() => folderInputRef.current?.click()}
            >
              📂 OPEN GAS FOLDER
            </button>
            <input ref={folderInputRef} type="file" className="hidden"
              /* @ts-expect-error webkitdirectory not in TS types */
              webkitdirectory="" multiple onChange={handleFolderPick} />
            {error && <p className="text-red-400 font-mono text-xs">{error}</p>}
          </div>
        ) : (
          /* ── Editor ── */
          <div className="flex-1 flex overflow-hidden">

            {/* Item list */}
            <div className="w-56 border-r border-game-border flex flex-col overflow-hidden">
              <div className="px-3 py-2 border-b border-game-border">
                <span className="text-game-textDim font-mono text-xs tracking-widest">ITEMS ({folder.items.length})</span>
              </div>
              <div className="flex-1 overflow-y-auto">
                {folder.items.map(i => (
                  <button
                    key={i.id}
                    onClick={() => setSelectedId(i.id)}
                    className={`w-full text-left px-3 py-2 font-mono text-xs border-b border-game-border transition-colors ${
                      i.id === selectedId
                        ? 'bg-game-primary text-black'
                        : 'text-game-text hover:bg-game-surface'
                    }`}
                  >
                    <div className="truncate">{i.name || i.id}</div>
                    <div className={`text-[10px] ${i.id === selectedId ? 'text-black/70' : 'text-game-muted'}`}>
                      {i.id} · ₢{i.price}
                    </div>
                  </button>
                ))}
              </div>
            </div>

            {/* Property panel */}
            {item ? (
              <div className="flex-1 overflow-y-auto p-6 space-y-6">

                {/* Save bar */}
                <div className="flex items-center gap-3">
                  <button
                    onClick={handleSave}
                    disabled={!dirty}
                    className={`px-4 py-2 font-mono text-xs tracking-widest border transition-colors ${
                      dirty
                        ? 'border-game-primary text-game-primary hover:bg-game-primary hover:text-black'
                        : 'border-game-border text-game-muted cursor-not-allowed'
                    }`}
                  >
                    ↓ DOWNLOAD items.json
                  </button>
                  {saveMsg && <span className="text-green-400 font-mono text-xs">{saveMsg}</span>}
                  {error && <span className="text-red-400 font-mono text-xs">{error}</span>}
                </div>

                {/* Identity */}
                <section>
                  <h2 className="text-game-primary font-mono text-xs tracking-widest mb-3 border-b border-game-border pb-1">IDENTITY</h2>
                  <div className="grid grid-cols-2 gap-3">
                    <label className="flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">ID</span>
                      <input
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                        value={item.id}
                        onChange={e => updateItem({ id: e.target.value })}
                      />
                    </label>
                    <label className="flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">ENUM ID</span>
                      <input type="number"
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                        value={item.enumId ?? ''}
                        onChange={e => updateNumeric('enumId', e.target.value)}
                      />
                    </label>
                    <label className="col-span-2 flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">NAME</span>
                      <input
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                        value={item.name}
                        onChange={e => updateItem({ name: e.target.value })}
                      />
                    </label>
                    <label className="col-span-2 flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">DESCRIPTION</span>
                      <textarea
                        rows={4}
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full resize-y"
                        value={item.description}
                        onChange={e => updateItem({ description: e.target.value })}
                      />
                    </label>
                  </div>
                </section>

                {/* Sprite */}
                <section>
                  <h2 className="text-game-primary font-mono text-xs tracking-widest mb-3 border-b border-game-border pb-1">SPRITE</h2>
                  <div className="grid grid-cols-2 gap-3">
                    <label className="flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">BANK</span>
                      <input type="number"
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                        value={item.spriteBank ?? ''}
                        onChange={e => updateNumeric('spriteBank', e.target.value)}
                      />
                    </label>
                    <label className="flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">INDEX</span>
                      <input type="number"
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                        value={item.spriteIndex ?? ''}
                        onChange={e => updateNumeric('spriteIndex', e.target.value)}
                      />
                    </label>
                  </div>
                </section>

                {/* Purchase */}
                <section>
                  <h2 className="text-game-primary font-mono text-xs tracking-widest mb-3 border-b border-game-border pb-1">PURCHASE</h2>
                  <div className="grid grid-cols-2 gap-3">
                    {NUMERIC_FIELDS.slice(0, 2).map(({ key, label }) => (
                      <label key={key} className="flex flex-col gap-1">
                        <span className="text-game-textDim font-mono text-[10px] tracking-widest">{label}</span>
                        <input type="number"
                          className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                          value={(item[key] as number) ?? ''}
                          onChange={e => updateNumeric(key, e.target.value)}
                        />
                      </label>
                    ))}
                  </div>
                </section>

                {/* Tech tree */}
                <section>
                  <h2 className="text-game-primary font-mono text-xs tracking-widest mb-3 border-b border-game-border pb-1">TECH TREE</h2>
                  <div className="grid grid-cols-2 gap-3">
                    <label className="flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">TECH CHOICE (BITMASK)</span>
                      <input type="number"
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                        value={item.techChoice ?? ''}
                        onChange={e => updateNumeric('techChoice', e.target.value)}
                      />
                    </label>
                    <label className="flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">TECH SLOTS</span>
                      <input type="number"
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                        value={item.techSlots ?? ''}
                        onChange={e => updateNumeric('techSlots', e.target.value)}
                      />
                    </label>
                    <label className="col-span-2 flex flex-col gap-1">
                      <span className="text-game-textDim font-mono text-[10px] tracking-widest">AGENCY RESTRICTION</span>
                      <select
                        className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                        value={item.agencyRestriction}
                        onChange={e => updateItem({ agencyRestriction: Number(e.target.value) })}
                      >
                        {Object.entries(AGENCY_NAMES).map(([val, name]) => (
                          <option key={val} value={val}>{name}</option>
                        ))}
                      </select>
                    </label>
                  </div>
                </section>

                {/* Stats */}
                <section>
                  <h2 className="text-game-primary font-mono text-xs tracking-widest mb-3 border-b border-game-border pb-1">STATS &amp; EFFECTS</h2>
                  <div className="grid grid-cols-2 gap-3">
                    {NUMERIC_FIELDS.slice(2).map(({ key, label }) => (
                      <label key={key} className="flex flex-col gap-1">
                        <span className="text-game-textDim font-mono text-[10px] tracking-widest">{label}</span>
                        <input type="number"
                          className="bg-game-surface border border-game-border text-game-text font-mono text-xs px-2 py-1 w-full"
                          value={(item[key] as number | undefined) ?? ''}
                          onChange={e => updateNumeric(key, e.target.value)}
                          placeholder="—"
                        />
                      </label>
                    ))}
                  </div>
                </section>

              </div>
            ) : (
              <div className="flex-1 flex items-center justify-center text-game-muted font-mono text-xs">
                SELECT AN ITEM
              </div>
            )}
          </div>
        )}
      </main>
    </div>
  );
}
