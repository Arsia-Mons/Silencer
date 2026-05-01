'use client';
import { useRef, useEffect, useState, useCallback } from 'react';
import Link from 'next/link';
import { useParams, useRouter } from 'next/navigation';
import { useAuth } from '../../../lib/auth';
import Sidebar from '../../../components/Sidebar';
import { useWsConnected } from '../../../lib/socket';
import { apiFetch } from '../../../lib/api';
import { decodeAdpcmWav } from '../../sound-studio/adpcm';
import * as gasStore from '../../../lib/gas-store';
import * as audioStore from '../../../lib/audio-store';

// ── Types ─────────────────────────────────────────────────────────────────────

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
  soundPickup?: string;
  soundUse?: string;
  soundWarn?: string;
  [key: string]: unknown;
}

// ── Constants ─────────────────────────────────────────────────────────────────

const AGENCY_NAMES: [number, string][] = [
  [-1, 'All Agencies'],
  [0, 'Noxis'], [1, 'Lazarus'], [2, 'Caliber'], [3, 'Static'], [4, 'Blackrose'],
];

const TECH_BITS: { bit: number; label: string }[] = [
  { bit: 0,  label: 'Laser' },       { bit: 1,  label: 'Rocket' },
  { bit: 2,  label: 'Flamer' },      { bit: 3,  label: 'Health Pack' },
  { bit: 4,  label: 'EMP Bomb' },    { bit: 5,  label: 'Shaped Bomb' },
  { bit: 6,  label: 'Plasma Bomb' }, { bit: 7,  label: 'Neutron Bomb' },
  { bit: 8,  label: 'Plasma Det.' }, { bit: 9,  label: 'Fixed Cannon' },
  { bit: 10, label: 'Flare' },       { bit: 11, label: 'Base Door' },
  { bit: 12, label: 'Base Defense' },{ bit: 13, label: 'Insider Info' },
  { bit: 14, label: 'Lazarus Tract'},{ bit: 15, label: 'Poison' },
  { bit: 16, label: 'Poison Flare' },{ bit: 17, label: 'Security Pass' },
  { bit: 18, label: 'Camera' },      { bit: 19, label: 'Virus' },
];

const STATS: { key: string; label: string }[] = [
  { key: 'spawnAmmo',           label: 'Spawn Ammo' },
  { key: 'pickupAmmo',          label: 'Pickup Ammo' },
  { key: 'maxAmmo',             label: 'Max Ammo' },
  { key: 'spawnInventoryCount', label: 'Spawn Inventory Count' },
  { key: 'healAmount',          label: 'Heal Amount' },
  { key: 'poisonDose',          label: 'Poison Dose' },
];

const SOUND_FIELDS: { key: string; label: string }[] = [
  { key: 'soundPickup', label: 'Pickup' },
  { key: 'soundUse',    label: 'Use' },
  { key: 'soundWarn',   label: 'Warn' },
];

const INPUT  = 'bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 w-full focus:border-[#00a328] outline-none';
const LABEL  = 'text-[9px] font-mono text-[#4a7a4a] tracking-widest';

// ── Sprite bank cache (module-level, survives navigation) ─────────────────────

interface SpriteBank { bank: number; frames: number }
let _bankCache: SpriteBank[] | null = null;
async function fetchSpriteBanks(): Promise<SpriteBank[]> {
  if (_bankCache) return _bankCache;
  const r = await fetch('/api/sprites');
  _bankCache = await r.json() as SpriteBank[];
  return _bankCache;
}

// ── Sprite picker modal ───────────────────────────────────────────────────────

function SpritePicker({
  currentBank, currentFrame, onPick, onClose,
}: {
  currentBank: number; currentFrame: number;
  onPick: (bank: number, frame: number) => void;
  onClose: () => void;
}) {
  const [banks, setBanks]           = useState<SpriteBank[]>([]);
  const [loadingBanks, setLoadingBanks] = useState(true);
  const [selBank, setSelBank]       = useState<number | null>(null);
  const [frames, setFrames]         = useState<{ frame: number; width: number; height: number }[]>([]);
  const [loadingFrames, setLoadingFrames] = useState(false);

  useEffect(() => {
    fetchSpriteBanks()
      .then(data => { setBanks(data); setLoadingBanks(false); })
      .catch(() => setLoadingBanks(false));
  }, []);

  useEffect(() => {
    if (selBank == null) { setFrames([]); return; }
    setLoadingFrames(true);
    fetch(`/api/sprites/${selBank}/frames`)
      .then(r => r.json())
      .then((data: { frame: number; width: number; height: number }[]) => { setFrames(data); setLoadingFrames(false); })
      .catch(() => setLoadingFrames(false));
  }, [selBank]);

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80"
      onClick={onClose}>
      <div className="bg-[#080f08] border border-[#1a2e1a] rounded p-4 flex flex-col gap-3 w-[680px] max-h-[80vh]"
        onClick={e => e.stopPropagation()}>
        <div className="flex items-center gap-3 shrink-0">
          {selBank != null && (
            <button onClick={() => setSelBank(null)}
              className="text-[10px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors">
              ← BANKS
            </button>
          )}
          <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">
            {selBank == null ? 'SELECT SPRITE BANK' : `BANK ${selBank} — SELECT FRAME`}
          </span>
          <div className="flex-1" />
          <button onClick={onClose}
            className="text-[10px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors">
            ✕ CLOSE
          </button>
        </div>

        <div className="overflow-y-auto">
          {selBank == null ? (
            loadingBanks
              ? <p className="text-[9px] font-mono text-[#2a4a2a]">Loading…</p>
              : <div className="grid grid-cols-8 gap-2">
                  {banks.map(({ bank, frames: fc }) => (
                    <button key={bank} onClick={() => setSelBank(bank)}
                      className={`flex flex-col items-center gap-1 p-1 border rounded transition-colors ${
                        bank === currentBank
                          ? 'border-[#00a328]' : 'border-[#1a2e1a] hover:border-[#00a328]'
                      }`}>
                      {/* eslint-disable-next-line @next/next/no-img-element */}
                      <img src={`/api/sprites/${bank}/0`} width={52} height={52}
                        alt={`bank ${bank}`} className="bg-[#080f08]"
                        style={{ imageRendering: 'pixelated', objectFit: 'contain' }} />
                      <span className="text-[8px] font-mono text-[#d1fad7]">{bank}</span>
                      <span className="text-[7px] font-mono text-[#2a4a2a]">{fc}f</span>
                    </button>
                  ))}
                </div>
          ) : (
            loadingFrames
              ? <p className="text-[9px] font-mono text-[#2a4a2a]">Loading frames…</p>
              : <div className="grid grid-cols-8 gap-2">
                  {frames.map(({ frame }) => (
                    <button key={frame} onClick={() => { onPick(selBank, frame); onClose(); }}
                      className={`flex flex-col items-center gap-1 p-1 border rounded transition-colors ${
                        selBank === currentBank && frame === currentFrame
                          ? 'border-[#00a328]' : 'border-[#1a2e1a] hover:border-[#00a328]'
                      }`}>
                      {/* eslint-disable-next-line @next/next/no-img-element */}
                      <img src={`/api/sprites/${selBank}/${frame}`} width={52} height={52}
                        alt={`frame ${frame}`} className="bg-[#080f08]"
                        style={{ imageRendering: 'pixelated', objectFit: 'contain' }} />
                      <span className="text-[8px] font-mono text-[#d1fad7]">{frame}</span>
                    </button>
                  ))}
                </div>
          )}
        </div>
      </div>
    </div>
  );
}

// ── Main component ────────────────────────────────────────────────────────────

export default function ItemDetailPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const { id } = useParams() as { id: string };
  const router = useRouter();
  const folderInputRef = useRef<HTMLInputElement>(null);

  const [items,      setItems]      = useState<ItemDef[]>([]);
  const [rawAll,     setRawAll]     = useState<Record<string, unknown>>({});
  const [folderName, setFolderName] = useState<string | null>(null);
  const [dirty,      setDirty]      = useState(false);
  const [saveMsg,    setSaveMsg]    = useState('');
  const [error,      setError]      = useState('');
  const [search,     setSearch]     = useState('');
  const [spritePicker, setSpritePicker] = useState(false);
  const [soundList,  setSoundList]  = useState<string[]>([]);
  const [playingSound, setPlayingSound] = useState<string | null>(null);
  const audioCtxRef    = useRef<AudioContext | null>(null);
  const audioSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const selectedRef    = useRef<HTMLAnchorElement>(null);

  // Hydrate from GAS store
  useEffect(() => {
    const text = gasStore.getFile('items');
    if (!text) return;
    try {
      const parsed = JSON.parse(text) as Record<string, unknown>;
      setRawAll(parsed);
      setItems((parsed.items as ItemDef[]) ?? []);
      setFolderName(gasStore.getFolderName());
    } catch { /* ignore */ }
  }, [id]);

  // Load sound list
  useEffect(() => {
    apiFetch('/sounds').then((data: unknown) => {
      setSoundList((data as Array<{ name: string }>).map(s => s.name).sort());
    }).catch(() => {});
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // Scroll selected into view
  useEffect(() => { selectedRef.current?.scrollIntoView({ block: 'nearest' }); }, [id]);

  // Arrow key navigation
  const navigate = useCallback((dir: 1 | -1) => {
    const visible = items.filter(i =>
      !search || i.id.includes(search) || i.name.toLowerCase().includes(search.toLowerCase())
    );
    const idx = visible.findIndex(i => i.id === id);
    if (idx === -1) return;
    const next = visible[idx + dir];
    if (next) router.push(`/items/${next.id}`, { scroll: false });
  }, [items, id, router, search]);

  useEffect(() => {
    function onKey(e: KeyboardEvent) {
      const tag = (e.target as HTMLElement).tagName;
      if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
      if (e.key === 'ArrowUp')   { e.preventDefault(); navigate(-1); }
      if (e.key === 'ArrowDown') { e.preventDefault(); navigate(1);  }
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [navigate]);

  // ── Audio ─────────────────────────────────────────────────────────────────

  function getAudioCtx(): AudioContext {
    if (!audioCtxRef.current || audioCtxRef.current.state === 'closed')
      audioCtxRef.current = new AudioContext();
    return audioCtxRef.current;
  }

  const getToken = () => (typeof window !== 'undefined' ? localStorage.getItem('zs_token') ?? '' : '');

  async function playSound(name: string) {
    if (audioSourceRef.current) {
      try { audioSourceRef.current.stop(); } catch { /* ended */ }
      audioSourceRef.current = null;
    }
    if (playingSound === name) { setPlayingSound(null); return; }
    try {
      let decoded = audioStore.get(name);
      if (!decoded) {
        const r = await fetch(`/api/sounds/${encodeURIComponent(name)}/play`, {
          headers: { Authorization: `Bearer ${getToken()}` },
        });
        if (!r.ok) throw new Error(r.statusText);
        const buf = await r.arrayBuffer();
        const ctx = getAudioCtx();
        if (ctx.state === 'suspended') await ctx.resume();
        decoded = await decodeAdpcmWav(buf, ctx);
        audioStore.set(name, decoded);
      }
      const ctx = getAudioCtx();
      const src = ctx.createBufferSource();
      src.buffer = decoded;
      src.connect(ctx.destination);
      src.start();
      audioSourceRef.current = src;
      setPlayingSound(name);
      src.onended = () => { setPlayingSound(null); audioSourceRef.current = null; };
    } catch { setPlayingSound(null); }
  }

  // ── Folder pick ───────────────────────────────────────────────────────────

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
      setRawAll(parsed); setItems(list); setFolderName(name);
      setDirty(false); setError('');
      gasStore.loadFolder(name, { ...gasStore.getAllFiles(), items: text });
      if (!list.find(i => i.id === id) && list[0]) router.replace(`/items/${list[0].id}`);
    } catch (err) { setError(String(err)); }
  }

  function closeFolder() {
    gasStore.clear(); setItems([]); setRawAll({}); setFolderName(null);
    router.push('/items');
  }

  // ── Mutations ─────────────────────────────────────────────────────────────

  const item = items.find(i => i.id === id) ?? null;

  function commitItems(newItems: ItemDef[]) {
    const newRaw = { ...rawAll, items: newItems };
    setItems(newItems); setRawAll(newRaw);
    gasStore.setFile('items', JSON.stringify(newRaw, null, 2));
    setDirty(true); setSaveMsg('');
  }

  function patch(update: Partial<ItemDef>) {
    if (!item) return;
    commitItems(items.map(i => i.id === id ? { ...i, ...update } : i));
  }

  function patchNum(key: string, val: string) {
    const n = val === '' ? undefined : Number(val);
    if (n !== undefined && isNaN(n)) return;
    patch({ [key]: n } as Partial<ItemDef>);
  }

  function addItem() {
    const newId = `item_${Date.now()}`;
    const blank: ItemDef = {
      id: newId, enumId: 0, name: 'New Item', description: '',
      price: 0, repairPrice: 0, spriteBank: 255, spriteIndex: 0,
      techChoice: 0, techSlots: 1, agencyRestriction: -1,
    };
    const newItems = [...items, blank];
    commitItems(newItems);
    router.push(`/items/${newId}`, { scroll: false });
  }

  function duplicateItem() {
    if (!item) return;
    const newId = `${item.id}_copy`;
    const dup = { ...item, id: newId, name: `${item.name} (copy)` };
    const idx = items.findIndex(i => i.id === id);
    const newItems = [...items.slice(0, idx + 1), dup, ...items.slice(idx + 1)];
    commitItems(newItems);
    router.push(`/items/${newId}`, { scroll: false });
  }

  function deleteItem() {
    if (!item) return;
    if (!confirm(`Delete "${item.name || item.id}"?`)) return;
    const newItems = items.filter(i => i.id !== id);
    commitItems(newItems);
    const nav = newItems[Math.max(0, items.findIndex(i => i.id === id) - 1)];
    router.replace(nav ? `/items/${nav.id}` : '/items', { scroll: false } as Parameters<typeof router.replace>[1]);
  }

  function handleSave() {
    if (!item) return;
    const text = JSON.stringify(rawAll, null, 2);
    const blob = new Blob([text], { type: 'application/json' });
    const a = document.createElement('a'); a.href = URL.createObjectURL(blob);
    a.download = 'items.json'; a.click(); URL.revokeObjectURL(a.href);
    setDirty(false); setSaveMsg('Downloaded items.json');
  }

  // ── Validation ────────────────────────────────────────────────────────────

  const enumIdCounts = items.reduce<Record<number, number>>((acc, i) => {
    acc[i.enumId] = (acc[i.enumId] ?? 0) + 1; return acc;
  }, {});
  const hasDupEnumId = item ? (enumIdCounts[item.enumId] ?? 0) > 1 : false;
  // 255 is the valid sentinel for "no sprite" (abstract items: give0-give3, insiderinfo, etc.)
  const hasUnsetSprite = item ? item.spriteBank == null : false;

  // ── Filtered list ─────────────────────────────────────────────────────────

  const filtered = search
    ? items.filter(i => i.id.includes(search) || i.name.toLowerCase().includes(search.toLowerCase()))
    : items;

  // ── No folder ─────────────────────────────────────────────────────────────

  if (!folderName) {
    return (
      <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
        <Sidebar wsConnected={wsConnected} />
        <input ref={folderInputRef} type="file" className="hidden"
          /* @ts-expect-error webkitdirectory */
          webkitdirectory="" multiple onChange={handleFolderPicked} />
        <div className="flex-1 flex items-center justify-center">
          <div className="flex flex-col items-center gap-4 text-center max-w-xs">
            <div className="text-4xl">⊟</div>
            <p className="text-xs text-[#4a7a4a] font-mono">Open your GAS folder to load items.</p>
            {error && <p className="text-xs text-red-400 font-mono">{error}</p>}
            <button onClick={() => folderInputRef.current?.click()}
              className="px-6 py-2 border border-[#00a328] text-[#00a328] font-mono text-xs tracking-widest hover:bg-[#00a328]/10 transition-colors">
              [ OPEN FOLDER ]
            </button>
          </div>
        </div>
      </div>
    );
  }

  // ── Main layout ───────────────────────────────────────────────────────────

  return (
    <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
      <Sidebar wsConnected={wsConnected} />
      <input ref={folderInputRef} type="file" className="hidden"
        /* @ts-expect-error webkitdirectory */
        webkitdirectory="" multiple onChange={handleFolderPicked} />
      {spritePicker && item && (
        <SpritePicker
          currentBank={item.spriteBank ?? 255}
          currentFrame={item.spriteIndex ?? 0}
          onPick={(bank, frame) => patch({ spriteBank: bank, spriteIndex: frame })}
          onClose={() => setSpritePicker(false)}
        />
      )}

      <div className="flex flex-col flex-1 min-w-0">
        {/* Header */}
        <div className="flex items-center gap-3 px-4 py-2 border-b border-[#1a2e1a] shrink-0">
          <span className="text-xs font-mono text-[#00a328] tracking-widest">⊟ ITEM TOOL</span>
          <span className="text-xs font-mono text-[#4a7a4a]">[ {folderName} ]</span>
          <div className="flex-1" />
          {error   && <span className="text-xs font-mono text-red-400 max-w-xs truncate">{error}</span>}
          {dirty   && <span className="text-[10px] font-mono text-[#f59e0b]">● unsaved</span>}
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
          <div className="flex flex-col border-r border-[#1a2e1a] shrink-0" style={{ width: 210 }}>
            {/* Search */}
            <div className="px-2 py-2 border-b border-[#1a2e1a] flex gap-1">
              <input
                className="flex-1 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 focus:border-[#00a328] outline-none"
                placeholder="filter…"
                value={search}
                onChange={e => setSearch(e.target.value)}
              />
              <button onClick={addItem} title="Add new item"
                className="px-2 py-1 text-xs font-mono text-[#4a7a4a] hover:text-[#00a328] border border-[#1a2e1a] hover:border-[#00a328] transition-colors">
                +
              </button>
            </div>
            <div className="px-3 py-1 border-b border-[#1a2e1a] shrink-0">
              <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">
                ITEMS ({filtered.length}{search ? `/${items.length}` : ''})
              </span>
            </div>
            <div className="flex-1 overflow-y-auto">
              {filtered.map(i => {
                const dupEnum = (enumIdCounts[i.enumId] ?? 0) > 1;
                const unsetSp = i.spriteBank == null;
                return (
                  <Link key={i.id} href={`/items/${i.id}`} scroll={false}
                    ref={i.id === id ? selectedRef : null}
                    className={`flex items-center gap-1 px-3 py-2 border-b border-[#1a2e1a] transition-colors ${
                      i.id === id ? 'bg-[#00a328] text-black' : 'hover:bg-[#0a180a] text-[#d1fad7]'
                    }`}>
                    <div className="flex-1 min-w-0">
                      <div className="text-xs font-mono truncate">{i.name || i.id}</div>
                      <div className={`text-[10px] font-mono ${i.id === id ? 'text-black/60' : 'text-[#4a7a4a]'}`}>
                        {i.id} · ₢{i.price}
                      </div>
                    </div>
                    {(dupEnum || unsetSp) && (
                      <span title={[dupEnum && 'duplicate enumId', unsetSp && 'sprite unset'].filter(Boolean).join(', ')}
                        className={`text-[10px] shrink-0 ${i.id === id ? 'text-black/70' : 'text-[#f59e0b]'}`}>
                        ⚠
                      </span>
                    )}
                  </Link>
                );
              })}
            </div>
          </div>

          {/* ── Right: property panel ── */}
          {item ? (
            <div className="flex-1 overflow-y-auto p-5 space-y-4">

              {/* Actions */}
              <div className="flex items-center gap-2 flex-wrap">
                <button onClick={handleSave} disabled={!dirty}
                  className={`px-3 py-1 text-xs font-mono border transition-colors ${dirty
                    ? 'border-[#00a328] text-[#00a328] hover:bg-[#00a328]/10'
                    : 'border-[#1a2e1a] text-[#4a7a4a] cursor-not-allowed'}`}>
                  ↓ DOWNLOAD items.json
                </button>
                <button onClick={duplicateItem}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:text-[#00a328] hover:border-[#00a328] transition-colors">
                  ⊕ DUPLICATE
                </button>
                <button onClick={deleteItem}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:text-red-400 hover:border-red-400 transition-colors">
                  ✕ DELETE
                </button>
              </div>

              {/* Validation warnings */}
              {hasDupEnumId && (
                <div className="border border-[#f59e0b]/40 rounded p-3 flex flex-col gap-1">
                  {hasDupEnumId && (
                    <p className="text-[10px] font-mono text-[#f59e0b]">
                      ⚠ enumId {item.enumId} is used by multiple items
                    </p>
                  )}
                </div>
              )}

              {/* Identity */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">IDENTITY</span>
                <div className="grid grid-cols-2 gap-3">
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>ID</span>
                    <input className={INPUT} value={item.id} onChange={e => patch({ id: e.target.value })} />
                  </label>
                  <label className="flex flex-col gap-1">
                    <span className={`${LABEL} ${hasDupEnumId ? 'text-[#f59e0b]' : ''}`}>
                      ENUM ID{hasDupEnumId ? ' ⚠' : ''}
                    </span>
                    <input type="number" className={`${INPUT} ${hasDupEnumId ? 'border-[#f59e0b]' : ''}`}
                      value={item.enumId ?? ''} onChange={e => patchNum('enumId', e.target.value)} />
                  </label>
                  <label className="col-span-2 flex flex-col gap-1">
                    <span className={LABEL}>NAME</span>
                    <input className={INPUT} value={item.name} onChange={e => patch({ name: e.target.value })} />
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
                <div className="flex items-center justify-between">
                  <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SPRITE</span>
                  <button onClick={() => setSpritePicker(true)}
                    className="text-[9px] font-mono text-[#4a7a4a] hover:text-[#00a328] border border-[#1a2e1a] hover:border-[#00a328] px-2 py-0.5 transition-colors">
                    ◈ PICK
                  </button>
                </div>
                <div className="flex items-start gap-4">
                  {item.spriteBank == null ? (
                    <button onClick={() => setSpritePicker(true)}
                      className="w-16 h-16 border border-[#1a2e1a] hover:border-[#00a328] bg-[#080f08] flex items-center justify-center text-[10px] font-mono text-[#2a4a2a] shrink-0 transition-colors">
                      PICK
                    </button>
                  ) : item.spriteBank === 255 ? (
                    <button onClick={() => setSpritePicker(true)}
                      className="w-16 h-16 border border-[#1a2e1a] hover:border-[#4a7a4a] bg-[#080f08] flex items-center justify-center text-[10px] font-mono text-[#4a7a4a] shrink-0 transition-colors"
                      title="255 = no sprite (abstract item). Click to assign one.">
                      ∅
                    </button>
                  ) : (
                    // eslint-disable-next-line @next/next/no-img-element
                    <img src={`/api/sprites/${item.spriteBank}/${item.spriteIndex ?? 0}`}
                      alt={`bank ${item.spriteBank} frame ${item.spriteIndex}`}
                      width={64} height={64}
                      className="border border-[#1a2e1a] bg-[#080f08] shrink-0 cursor-pointer hover:border-[#00a328] transition-colors"
                      style={{ imageRendering: 'pixelated', objectFit: 'contain' }}
                      onClick={() => setSpritePicker(true)} />
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
                    <span className={LABEL}>TECH SLOTS (COST)</span>
                    <input type="number" className={INPUT} value={item.techSlots ?? ''}
                      onChange={e => patchNum('techSlots', e.target.value)} />
                  </label>
                  <label className="flex flex-col gap-1">
                    <span className={LABEL}>AGENCY RESTRICTION</span>
                    <select className={INPUT} value={item.agencyRestriction}
                      onChange={e => patch({ agencyRestriction: Number(e.target.value) })}>
                      {AGENCY_NAMES.map(([val, name]) => (
                        <option key={val} value={val}>{name}</option>
                      ))}
                    </select>
                  </label>
                </div>
                <div className="flex flex-col gap-1">
                  <span className={LABEL}>TECH CHOICE BITS (which tech slots unlock this item)</span>
                  <div className="grid grid-cols-2 gap-x-4 gap-y-1 mt-1">
                    {TECH_BITS.map(({ bit, label }) => {
                      const checked = ((item.techChoice ?? 0) & (1 << bit)) !== 0;
                      return (
                        <label key={bit} className="flex items-center gap-2 cursor-pointer group">
                          <div
                            onClick={() => patch({ techChoice: (item.techChoice ?? 0) ^ (1 << bit) })}
                            className={`w-3 h-3 border flex items-center justify-center shrink-0 transition-colors cursor-pointer ${
                              checked ? 'border-[#00a328] bg-[#00a328]' : 'border-[#1a2e1a] group-hover:border-[#4a7a4a]'
                            }`}>
                            {checked && <span className="text-[8px] text-black leading-none">✓</span>}
                          </div>
                          <span className="text-[10px] font-mono text-[#7aaa7a] group-hover:text-[#d1fad7] transition-colors">
                            {label}
                          </span>
                          <span className="text-[9px] font-mono text-[#2a4a2a]">{bit}</span>
                        </label>
                      );
                    })}
                  </div>
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

              {/* Sounds */}
              <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                <div className="flex items-center justify-between">
                  <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SOUNDS</span>
                  <Link href="/sound-studio"
                    className="text-[9px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors">
                    BROWSE →
                  </Link>
                </div>
                <p className="text-[9px] font-mono text-[#2a4a2a]">
                  GAS extension — requires C++ update to activate in-game.
                  Current item audio uses shared player sounds (soundPickup / soundReload / soundPowerUp).
                </p>
                {SOUND_FIELDS.map(({ key, label }) => {
                  const val = String(item[key] ?? '');
                  return (
                    <div key={key} className="flex items-center gap-2">
                      <span className="text-[9px] font-mono text-[#4a7a4a] w-14 shrink-0">{label}</span>
                      <select value={val}
                        onChange={e => patch({ [key]: e.target.value || undefined })}
                        className="flex-1 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 focus:border-[#00a328] outline-none">
                        <option value="">— none —</option>
                        {soundList.map(name => (
                          <option key={name} value={name}>{name}</option>
                        ))}
                        {val && !soundList.includes(val) && (
                          <option value={val}>{val}</option>
                        )}
                      </select>
                      {val && (
                        <button title={`Play ${val}`} onClick={() => playSound(val)}
                          className="shrink-0 text-[10px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors px-1">
                          {playingSound === val ? '■' : '▶'}
                        </button>
                      )}
                    </div>
                  );
                })}
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
