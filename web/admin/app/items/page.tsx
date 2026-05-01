'use client';
import { useRef, useState, useEffect } from 'react';
import { useRouter } from 'next/navigation';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { useWsConnected } from '../../lib/socket';
import * as gasStore from '../../lib/gas-store';

interface ItemDef { id: string; [key: string]: unknown }

export default function ItemsIndexPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const router = useRouter();
  const folderInputRef = useRef<HTMLInputElement>(null);
  const [error, setError] = useState('');

  // If folder already loaded, jump straight to first item
  useEffect(() => {
    const raw = gasStore.getFile('items');
    if (raw) {
      try {
        const items = (JSON.parse(raw) as Record<string, unknown>).items as ItemDef[];
        if (items?.[0]) { router.replace(`/items/${items[0].id}`); return; }
      } catch { /* fall through to picker */ }
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

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
      const items = (parsed.items as ItemDef[]) ?? [];
      const existing = gasStore.getAllFiles();
      gasStore.loadFolder(name, { ...existing, items: text });
      if (items[0]) router.push(`/items/${items[0].id}`);
    } catch (err) {
      setError(String(err));
    }
  }

  return (
    <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
      <Sidebar wsConnected={wsConnected} />
      <input ref={folderInputRef} type="file" className="hidden"
        /* @ts-expect-error webkitdirectory not in TS types */
        webkitdirectory="" multiple onChange={handleFolderPicked} />
      <div className="flex-1 flex items-center justify-center">
        <div className="flex flex-col items-center gap-6 text-center max-w-sm">
          <div className="text-5xl">⊟</div>
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
