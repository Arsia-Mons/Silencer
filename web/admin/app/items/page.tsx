'use client';
import { useRef, useState, useEffect } from 'react';
import Link from 'next/link';
import { useRouter } from 'next/navigation';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { useWsConnected } from '../../lib/socket';
import * as gasStore from '../../lib/gas-store';

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

export default function ItemsPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const router = useRouter();
  const folderInputRef = useRef<HTMLInputElement>(null);
  const [items, setItems] = useState<ItemDef[]>([]);
  const [folderName, setFolderName] = useState<string | null>(null);
  const [error, setError] = useState('');

  useEffect(() => {
    if (gasStore.isLoaded()) {
      const raw = gasStore.getFile('items');
      if (raw) {
        try {
          const parsed = JSON.parse(raw) as Record<string, unknown>;
          setItems((parsed.items as ItemDef[]) ?? []);
          setFolderName(gasStore.getFolderName());
        } catch { /* ignore corrupt store */ }
      }
    }
  }, []);

  async function handleFolderPicked(e: React.ChangeEvent<HTMLInputElement>) {
    const files = Array.from(e.target.files ?? []);
    e.target.value = '';
    if (!files.length) return;
    const name = files[0]?.webkitRelativePath?.split('/')[0] ?? 'gas';
    const iFile = files.find(f => f.name === 'items.json');
    if (!iFile) { setError('items.json not found in selected folder.'); return; }
    const text = await iFile.text();
    try {
      const parsed = JSON.parse(text) as Record<string, unknown>;
      const list = (parsed.items as ItemDef[]) ?? [];
      setItems(list);
      setFolderName(name);
      setError('');
      const existing = gasStore.getAllFiles();
      gasStore.loadFolder(name, { ...existing, items: text });
    } catch (err) {
      setError(String(err));
    }
  }

  function closeFolder() {
    gasStore.clear();
    setItems([]);
    setFolderName(null);
  }

  if (!folderName) {
    return (
      <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
        <Sidebar wsConnected={wsConnected} />
        <input ref={folderInputRef} type="file" className="hidden"
          /* @ts-expect-error webkitdirectory not in TS types */
          webkitdirectory="" multiple onChange={handleFolderPicked} />
        <div className="flex-1 flex items-center justify-center">
          <div className="flex flex-col items-center gap-6 text-center max-w-sm">
            <div className="text-5xl">🎒</div>
            <div className="text-xl font-mono tracking-widest text-[#00a328]">ITEM TOOL</div>
            <div className="text-xs text-[#4a7a4a] font-mono leading-relaxed">
              Open your <code className="text-[#7aaa7a]">shared/assets/gas/</code> folder to manage item definitions.
            </div>
            <ul className="text-[10px] text-[#4a7a4a] font-mono text-left space-y-1">
              <li>◆ Identity (id, enumId, name, description)</li>
              <li>◆ Sprite (bank + index)</li>
              <li>◆ Purchase price &amp; repair cost</li>
              <li>◆ Tech tree (choice bitmask, slots)</li>
              <li>◆ Agency restriction</li>
              <li>◆ Stats &amp; effects (ammo, heal, poison)</li>
            </ul>
            {error && <p className="text-xs text-red-400 font-mono">{error}</p>}
            <button
              onClick={() => folderInputRef.current?.click()}
              className="px-8 py-3 border border-[#00a328] text-[#00a328] font-mono text-sm tracking-widest hover:bg-[#00a328]/10 transition-colors"
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
          <span className="text-xs font-mono text-[#00a328] tracking-widest">🎒 ITEM TOOL</span>
          <span className="text-xs font-mono text-[#4a7a4a]">[ {folderName} ]</span>
          <div className="flex-1" />
          {error && <span className="text-xs text-red-400 font-mono">{error}</span>}
          <button
            onClick={() => folderInputRef.current?.click()}
            className="px-2 py-1 text-xs font-mono text-[#4a7a4a] hover:text-[#00a328] border border-[#1a2e1a] hover:border-[#00a328] transition-colors"
          >
            ↺ CHANGE
          </button>
          <button
            onClick={closeFolder}
            className="px-2 py-1 text-xs font-mono text-[#4a7a4a] hover:text-red-400 border border-[#1a2e1a] hover:border-red-400 transition-colors"
          >
            ✕ CLOSE
          </button>
        </div>

        {/* Item list */}
        <div className="flex-1 overflow-y-auto">
          <div className="px-3 py-2 border-b border-[#1a2e1a]">
            <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">ITEMS ({items.length})</span>
          </div>
          {items.map(item => (
            <Link
              key={item.id}
              href={`/items/${item.id}`}
              onClick={() => router.prefetch(`/items/${item.id}`)}
              className="flex items-center justify-between px-4 py-3 border-b border-[#1a2e1a] hover:bg-[#0a180a] transition-colors group"
            >
              <div>
                <div className="text-xs font-mono text-[#d1fad7] group-hover:text-[#00a328] transition-colors">
                  {item.name || item.id}
                </div>
                <div className="text-[10px] font-mono text-[#4a7a4a]">
                  {item.id} · enumId {item.enumId}
                </div>
              </div>
              <div className="text-right">
                <div className="text-xs font-mono text-[#7aaa7a]">₢{item.price}</div>
                {item.agencyRestriction >= 0 && (
                  <div className="text-[10px] font-mono text-[#4a7a4a]">
                    {['Noxis','Lazarus','Caliber','Static','Blackrose'][item.agencyRestriction] ?? ''}
                  </div>
                )}
              </div>
            </Link>
          ))}
        </div>
      </div>
    </div>
  );
}
