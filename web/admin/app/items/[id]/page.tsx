'use client';
import { useRef, useEffect, useState } from 'react';
import Link from 'next/link';
import { useParams, useRouter } from 'next/navigation';
import { useAuth } from '../../../lib/auth';
import Sidebar from '../../../components/Sidebar';
import { useWsConnected } from '../../../lib/socket';
import * as gasStore from '../../../lib/gas-store';

export interface ItemDef {
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
  agencyRestriction: number;
  spawnAmmo?: number;
  pickupAmmo?: number;
  maxAmmo?: number;
  spawnInventoryCount?: number;
  healAmount?: number;
  poisonDose?: number;
  [key: string]: unknown;
}

const AGENCY_NAMES: [number, string][] = [
  [-1, 'All Agencies'],
  [0,  'Noxis'],
  [1,  'Lazarus'],
  [2,  'Caliber'],
  [3,  'Static'],
  [4,  'Blackrose'],
];

const STATS: { key: string; label: string }[] = [
  { key: 'spawnAmmo',           label: 'Spawn Ammo' },
  { key: 'pickupAmmo',          label: 'Pickup Ammo' },
  { key: 'maxAmmo',             label: 'Max Ammo' },
  { key: 'spawnInventoryCount', label: 'Spawn Inventory Count' },
  { key: 'healAmount',          label: 'Heal Amount' },
  { key: 'poisonDose',          label: 'Poison Dose' },
];

const INPUT = 'bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 w-full focus:border-[#00a328] outline-none';
const LABEL = 'text-[9px] font-mono text-[#4a7a4a] tracking-widest';

export default function ItemDetailPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const { id } = useParams() as { id: string };
  const router = useRouter();
  const folderInputRef = useRef<HTMLInputElement>(null);

  const [items, setItems]     = useState<ItemDef[]>([]);
  const [rawAll, setRawAll]   = useState<Record<string, unknown>>({});
  const [folderName, setFolderName] = useState<string | null>(null);
  const [dirty, setDirty]     = useState(false);
  const [saveMsg, setSaveMsg] = useState('');
  const [error, setError]     = useState('');

  // Hydrate from GAS store on mount / when id changes
  useEffect(() => {
    const text = gasStore.getFile('items');
    if (!text) return;
    try {
      const parsed = JSON.parse(text) as Record<string, unknown>;
      setRawAll(parsed);
      setItems((parsed.items as ItemDef[]) ?? []);
      setFolderName(gasStore.getFolderName());
      setDirty(false);
      setSaveMsg('');
    } catch { /* ignore */ }
  }, [id]);

  async function handleFolderPicked(e: React.ChangeEvent<HTMLInputElement>) {
    const files = Array.from(e.target.files ?? []);
    e.target.value = '';
    if (!files.length) return;
    const name = files[0]?.webkitRelativePath?.split('/')[0] ?? 'gas';
    const iFile = files.find(f => f.name === 'items.json');
    if (!iFile) { setError('items.json not found.'); return; }
    const text = await iFile.text();
    try {
      const parsed = JSON.parse(text) as Record<string, unknown>;
      const list = (parsed.items as ItemDef[]) ?? [];
      setRawAll(parsed);
      setItems(list);
      setFolderName(name);
      setDirty(false);
      setError('');
      const existing = gasStore.getAllFiles();
      gasStore.loadFolder(name, { ...existing, items: text });
      // Navigate to first item if current id not in new set
      if (!list.find(i => i.id === id) && list[0]) {
        router.replace(`/items/${list[0].id}`);
      }
    } catch (err) {
      setError(String(err));
    }
  }

  function closeFolder() {
    gasStore.clear();
    setItems([]);
    setRawAll({});
    setFolderName(null);
    router.push('/items');
  }

  const item = items.find(i => i.id === id) ?? null;

  function patch(update: Partial<ItemDef>) {
    if (!item) return;
    const updated = { ...item, ...update };
    const newItems = items.map(i => i.id === id ? updated : i);
    const newRaw = { ...rawAll, items: newItems };
    setItems(newItems);
    setRawAll(newRaw);
    gasStore.setFile('items', JSON.stringify(newRaw, null, 2));
    setDirty(true);
    setSaveMsg('');
  }

  function patchNum(key: string, val: string) {
    const n = val === '' ? undefined : Number(val);
    if (n !== undefined && isNaN(n)) return;
    patch({ [key]: n } as Partial<ItemDef>);
  }

  function handleSave() {
    if (!item) return;
    const text = JSON.stringify(rawAll, null, 2);
    const blob = new Blob([text], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'items.json';
    a.click();
    URL.revokeObjectURL(a.href);
    setDirty(false);
    setSaveMsg('Downloaded items.json');
  }

  // No folder: show picker
  if (!folderName) {
    return (
      <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
        <Sidebar wsConnected={wsConnected} />
        <input ref={folderInputRef} type="file" className="hidden"
          /* @ts-expect-error webkitdirectory not in TS types */
          webkitdirectory="" multiple onChange={handleFolderPicked} />
        <div className="flex-1 flex items-center justify-center">
          <div className="flex flex-col items-center gap-4 text-center max-w-xs">
            <div className="text-4xl">⊟</div>
            <p className="text-xs text-[#4a7a4a] font-mono">
              Open your GAS folder to load items.
            </p>
            {error && <p className="text-xs text-red-400 font-mono">{error}</p>}
            <button
              onClick={() => folderInputRef.current?.click()}
              className="px-6 py-2 border border-[#00a328] text-[#00a328] font-mono text-xs tracking-widest hover:bg-[#00a328]/10 transition-colors"
            >
              [ OPEN FOLDER ]
            </button>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
      <Sidebar wsConnected={wsConnected} />
      <input ref={folderInputRef} type="file" className="hidden"
        /* @ts-expect-error webkitdirectory not in TS types */
        webkitdirectory="" multiple onChange={handleFolderPicked} />

      <div className="flex flex-col flex-1 min-w-0">
        {/* Header */}
        <div className="flex items-center gap-3 px-4 py-2 border-b border-[#1a2e1a] shrink-0">
          <span className="text-xs font-mono text-[#00a328] tracking-widest">⊟ ITEM TOOL</span>
          <span className="text-xs font-mono text-[#4a7a4a]">[ {folderName} ]</span>
          <div className="flex-1" />
          {error  && <span className="text-xs font-mono text-red-400 max-w-xs truncate">{error}</span>}
          {dirty  && <span className="text-[10px] font-mono text-[#f59e0b]">● unsaved</span>}
          {saveMsg && <span className="text-[10px] font-mono text-[#00a328]">{saveMsg}</span>}
          <button onClick={() => folderInputRef.current?.click()}
            className="px-2 py-1 text-xs font-mono text-[#4a7a4a] hover:text-[#00a328] border border-[#1a2e1a] hover:border-[#00a328] transition-colors">
            ↺ CHANGE
          </button>
          <button onClick={closeFolder}
            className="px-2 py-1 text-xs font-mono text-[#4a7a4a] hover:text-red-400 border border-[#1a2e1a] hover:border-red-400 transition-colors">
            ✕ CLOSE
          </button>
        </div>

        <div className="flex flex-1 min-h-0">
          {/* ── Left: item list ── */}
          <div className="flex flex-col border-r border-[#1a2e1a] overflow-hidden" style={{ width: 200, minWidth: 200 }}>
            <div className="px-3 py-2 border-b border-[#1a2e1a] shrink-0">
              <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">ITEMS ({items.length})</span>
            </div>
            <div className="flex-1 overflow-y-auto">
              {items.map(i => (
                <Link key={i.id} href={`/items/${i.id}`}
                  className={`flex flex-col px-3 py-2 border-b border-[#1a2e1a] transition-colors ${
                    i.id === id
                      ? 'bg-[#00a328] text-black'
                      : 'hover:bg-[#0a180a] text-[#d1fad7]'
                  }`}>
                  <span className="text-xs font-mono truncate">{i.name || i.id}</span>
                  <span className={`text-[10px] font-mono ${i.id === id ? 'text-black/60' : 'text-[#4a7a4a]'}`}>
                    {i.id} · ₢{i.price}
                  </span>
                </Link>
              ))}
            </div>
          </div>

          {/* ── Right: property panel ── */}
          {item ? (
            <div className="flex-1 overflow-y-auto p-5 space-y-4">

              {/* Save */}
              <div className="flex items-center gap-3">
                <button onClick={handleSave} disabled={!dirty}
                  className={`px-3 py-1 text-xs font-mono border transition-colors ${
                    dirty
                      ? 'border-[#00a328] text-[#00a328] hover:bg-[#00a328]/10'
                      : 'border-[#1a2e1a] text-[#4a7a4a] cursor-not-allowed'
                  }`}>
                  ↓ DOWNLOAD items.json
                </button>
              </div>

              {/* Identity */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">IDENTITY</span>
                <div className="grid grid-cols-2 gap-3">
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>ID</span>
                    <input className={INPUT} value={item.id}
                      onChange={e => patch({ id: e.target.value })} />
                  </label>
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>ENUM ID</span>
                    <input type="number" className={INPUT} value={item.enumId ?? ''}
                      onChange={e => patchNum('enumId', e.target.value)} />
                  </label>
                  <label className="col-span-2 flex flex-col gap-1">
                    <span className={LABEL}>NAME</span>
                    <input className={INPUT} value={item.name}
                      onChange={e => patch({ name: e.target.value })} />
                  </label>
                  <label className="col-span-2 flex flex-col gap-1">
                    <span className={LABEL}>DESCRIPTION</span>
                    <textarea rows={6} className={`${INPUT} resize-y`} value={item.description}
                      onChange={e => patch({ description: e.target.value })} />
                  </label>
                </div>
              </section>

              {/* Sprite */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SPRITE</span>
                <div className="flex items-start gap-4">
                  {/* Preview */}
                  {item.spriteBank != null && item.spriteBank !== 0xFF ? (
                    // eslint-disable-next-line @next/next/no-img-element
                    <img
                      src={`/api/sprites/${item.spriteBank}/${item.spriteIndex ?? 0}`}
                      alt={`bank ${item.spriteBank} frame ${item.spriteIndex}`}
                      width={64} height={64}
                      className="border border-[#1a2e1a] bg-[#080f08] shrink-0"
                      style={{ imageRendering: 'pixelated', objectFit: 'contain' }}
                    />
                  ) : (
                    <div className="w-16 h-16 border border-[#1a2e1a] bg-[#080f08] flex items-center justify-center text-[10px] font-mono text-[#2a4a2a] shrink-0">—</div>
                  )}
                  <div className="grid grid-cols-2 gap-3 flex-1">
                    <label className="flex flex-col gap-1">
                      <span className={LABEL}>BANK</span>
                      <input type="number" className={INPUT} value={item.spriteBank ?? ''}
                        onChange={e => patchNum('spriteBank', e.target.value)} />
                    </label>
                    <label className="flex flex-col gap-1">
                      <span className={LABEL}>INDEX</span>
                      <input type="number" className={INPUT} value={item.spriteIndex ?? ''}
                        onChange={e => patchNum('spriteIndex', e.target.value)} />
                    </label>
                  </div>
                </div>
              </section>

              {/* Purchase */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">PURCHASE</span>
                <div className="grid grid-cols-2 gap-3">
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>PRICE (₢)</span>
                    <input type="number" className={INPUT} value={item.price ?? ''}
                      onChange={e => patchNum('price', e.target.value)} />
                  </label>
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>REPAIR PRICE (₢)</span>
                    <input type="number" className={INPUT} value={item.repairPrice ?? ''}
                      onChange={e => patchNum('repairPrice', e.target.value)} />
                  </label>
                </div>
              </section>

              {/* Tech tree */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">TECH TREE</span>
                <div className="grid grid-cols-2 gap-3">
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>TECH CHOICE (BITMASK)</span>
                    <input type="number" className={INPUT} value={item.techChoice ?? ''}
                      onChange={e => patchNum('techChoice', e.target.value)} />
                  </label>
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>TECH SLOTS</span>
                    <input type="number" className={INPUT} value={item.techSlots ?? ''}
                      onChange={e => patchNum('techSlots', e.target.value)} />
                  </label>
                  <label className="col-span-2 flex flex-col gap-1">
                    <span className={LABEL}>AGENCY RESTRICTION</span>
                    <select className={INPUT} value={item.agencyRestriction}
                      onChange={e => patch({ agencyRestriction: Number(e.target.value) })}>
                      {AGENCY_NAMES.map(([val, name]) => (
                        <option key={val} value={val}>{name}</option>
                      ))}
                    </select>
                  </label>
                </div>
              </section>

              {/* Stats */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">STATS &amp; EFFECTS</span>
                <div className="grid grid-cols-2 gap-3">
                  {STATS.map(({ key, label }) => (
                    <label key={key} className="flex flex-col gap-1">
                      <span className={LABEL}>{label.toUpperCase()}</span>
                      <input type="number" className={INPUT}
                        value={(item[key] as number | undefined) ?? ''}
                        onChange={e => patchNum(key, e.target.value)}
                        placeholder="—" />
                    </label>
                  ))}
                </div>
              </section>

            </div>
          ) : (
            <div className="flex-1 flex items-center justify-center">
              <p className="text-[#4a7a4a] font-mono text-xs">SELECT AN ITEM</p>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
