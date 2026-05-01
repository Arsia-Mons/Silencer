'use client';
import { useEffect, useState } from 'react';
import Link from 'next/link';
import { useParams } from 'next/navigation';
import { useAuth } from '../../../lib/auth';
import Sidebar from '../../../components/Sidebar';
import { useWsConnected } from '../../../lib/socket';
import * as gasStore from '../../../lib/gas-store';
import type { ItemDef } from '../page';

const AGENCY_NAMES: [number, string][] = [
  [-1, 'All Agencies'],
  [0,  'Noxis'],
  [1,  'Lazarus'],
  [2,  'Caliber'],
  [3,  'Static'],
  [4,  'Blackrose'],
];

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

const INPUT = 'bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 w-full focus:border-[#00a328] outline-none';

export default function ItemDetailPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const { id } = useParams() as { id: string };
  const [item, setItem]       = useState<ItemDef | null>(null);
  const [rawAll, setRawAll]   = useState<Record<string, unknown>>({});
  const [dirty, setDirty]     = useState(false);
  const [saveMsg, setSaveMsg] = useState('');
  const [error, setError]     = useState('');

  useEffect(() => {
    const text = gasStore.getFile('items');
    if (!text) {
      setError('No folder loaded — go back to /items and open your GAS folder first.');
      return;
    }
    try {
      const parsed = JSON.parse(text) as Record<string, unknown>;
      setRawAll(parsed);
      const found = (parsed.items as ItemDef[])?.find(i => i.id === id);
      if (!found) { setError(`Item "${id}" not found.`); return; }
      setItem(found);
    } catch (e) {
      setError(String(e));
    }
  }, [id]);

  function patch(update: Partial<ItemDef>) {
    if (!item) return;
    const updated = { ...item, ...update };
    setItem(updated);
    const items = (rawAll.items as ItemDef[]).map(i => i.id === id ? updated : i);
    const newRaw = { ...rawAll, items };
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

  return (
    <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
      <Sidebar wsConnected={wsConnected} />

      <div className="flex flex-col flex-1 min-w-0">
        {/* Header */}
        <div className="flex items-center gap-3 px-4 py-2 border-b border-[#1a2e1a] shrink-0">
          <Link href="/items" className="text-xs font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors">
            ← ITEMS
          </Link>
          <span className="text-[#1a2e1a]">/</span>
          <span className="text-xs font-mono text-[#d1fad7]">{item?.name || id}</span>
          <span className="text-[10px] font-mono text-[#4a7a4a]">({id})</span>
          <div className="flex-1" />
          {dirty && <span className="text-[10px] font-mono text-[#f59e0b]">● unsaved</span>}
          {saveMsg && <span className="text-[10px] font-mono text-[#00a328]">{saveMsg}</span>}
          {error && <span className="text-xs font-mono text-red-400 max-w-xs truncate">{error}</span>}
          <button
            onClick={handleSave}
            disabled={!dirty}
            className={`px-3 py-1 text-xs font-mono border transition-colors ${
              dirty
                ? 'border-[#00a328] text-[#00a328] hover:bg-[#00a328]/10'
                : 'border-[#1a2e1a] text-[#4a7a4a] cursor-not-allowed'
            }`}
          >
            ↓ DOWNLOAD items.json
          </button>
        </div>

        {!item ? (
          <div className="flex-1 flex items-center justify-center">
            {error
              ? <div className="text-center space-y-3">
                  <p className="text-red-400 font-mono text-xs max-w-sm">{error}</p>
                  <Link href="/items" className="text-[#00a328] font-mono text-xs hover:underline">
                    ← Back to Items
                  </Link>
                </div>
              : <p className="text-[#4a7a4a] font-mono text-xs">Loading…</p>
            }
          </div>
        ) : (
          <div className="flex-1 overflow-y-auto p-6 max-w-2xl space-y-6">

            {/* Identity */}
            <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
              <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">IDENTITY</span>
              <div className="grid grid-cols-2 gap-3">
                <label className="flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">ID</span>
                  <input className={INPUT} value={item.id}
                    onChange={e => patch({ id: e.target.value })} />
                </label>
                <label className="flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">ENUM ID</span>
                  <input type="number" className={INPUT} value={item.enumId ?? ''}
                    onChange={e => patchNum('enumId', e.target.value)} />
                </label>
                <label className="col-span-2 flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">NAME</span>
                  <input className={INPUT} value={item.name}
                    onChange={e => patch({ name: e.target.value })} />
                </label>
                <label className="col-span-2 flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">DESCRIPTION</span>
                  <textarea rows={4} className={`${INPUT} resize-y`} value={item.description}
                    onChange={e => patch({ description: e.target.value })} />
                </label>
              </div>
            </section>

            {/* Sprite */}
            <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
              <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SPRITE</span>
              <div className="grid grid-cols-2 gap-3">
                <label className="flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">BANK</span>
                  <input type="number" className={INPUT} value={item.spriteBank ?? ''}
                    onChange={e => patchNum('spriteBank', e.target.value)} />
                </label>
                <label className="flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">INDEX</span>
                  <input type="number" className={INPUT} value={item.spriteIndex ?? ''}
                    onChange={e => patchNum('spriteIndex', e.target.value)} />
                </label>
              </div>
            </section>

            {/* Purchase */}
            <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
              <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">PURCHASE</span>
              <div className="grid grid-cols-2 gap-3">
                {NUMERIC_FIELDS.slice(0, 2).map(({ key, label }) => (
                  <label key={key} className="flex flex-col gap-1">
                    <span className="text-[9px] font-mono text-[#4a7a4a]">{label.toUpperCase()}</span>
                    <input type="number" className={INPUT} value={(item[key] as number) ?? ''}
                      onChange={e => patchNum(key, e.target.value)} />
                  </label>
                ))}
              </div>
            </section>

            {/* Tech tree */}
            <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
              <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">TECH TREE</span>
              <div className="grid grid-cols-2 gap-3">
                <label className="flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">TECH CHOICE (BITMASK)</span>
                  <input type="number" className={INPUT} value={item.techChoice ?? ''}
                    onChange={e => patchNum('techChoice', e.target.value)} />
                </label>
                <label className="flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">TECH SLOTS</span>
                  <input type="number" className={INPUT} value={item.techSlots ?? ''}
                    onChange={e => patchNum('techSlots', e.target.value)} />
                </label>
                <label className="col-span-2 flex flex-col gap-1">
                  <span className="text-[9px] font-mono text-[#4a7a4a]">AGENCY RESTRICTION</span>
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
                {NUMERIC_FIELDS.slice(2).map(({ key, label }) => (
                  <label key={key} className="flex flex-col gap-1">
                    <span className="text-[9px] font-mono text-[#4a7a4a]">{label.toUpperCase()}</span>
                    <input type="number" className={INPUT}
                      value={(item[key] as number | undefined) ?? ''}
                      onChange={e => patchNum(key, e.target.value)}
                      placeholder="—" />
                  </label>
                ))}
              </div>
            </section>

          </div>
        )}
      </div>
    </div>
  );
}
