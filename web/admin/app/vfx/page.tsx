'use client';
import { useEffect, useRef, useState } from 'react';
import { useRouter } from 'next/navigation';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import * as vfxStore from '../../lib/vfx-store';

export default function VFXPage() {
  useAuth();
  const router = useRouter();
  const fileRef = useRef<HTMLInputElement>(null);
  const [folderName, setFolderName] = useState<string | null>(null);
  const [error, setError] = useState('');

  useEffect(() => {
    if (vfxStore.isLoaded()) {
      const all = vfxStore.listAll();
      if (all.length > 0) router.replace(`/vfx/${all[0].id}`);
      else setFolderName(vfxStore.getFolderName());
    }
  }, [router]);

  async function handleFile(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    try {
      const text = await file.text();
      vfxStore.loadFromJson(file.name.replace(/\.json$/, ''), text);
      const all = vfxStore.listAll();
      if (all.length > 0) router.push(`/vfx/${all[0].id}`);
      else { setFolderName(vfxStore.getFolderName()); }
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    }
    e.target.value = '';
  }

  function handleNew() {
    vfxStore.loadFromJson('new-vfx-presets', JSON.stringify({ presets: [] }));
    const id = 'new-effect';
    vfxStore.addPreset({ ...vfxStore.DEFAULT_PRESET, id, name: 'New Effect' });
    router.push(`/vfx/${id}`);
  }

  return (
    <div className="flex min-h-screen bg-[#080f08] text-[#d1fad7]">
      <Sidebar />
      <main className="flex-1 flex items-center justify-center">
        <div className="border border-[#1a2e1a] rounded p-8 max-w-sm w-full flex flex-col gap-4">
          <div className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">✦ VFX EDITOR</div>
          <p className="text-xs font-mono text-[#d1fad7]">
            Load a <code className="text-[#00a328]">vfx-presets.json</code> file to begin.
          </p>
          {folderName && (
            <p className="text-[10px] font-mono text-[#4a7a4a]">Loaded: {folderName} (no presets)</p>
          )}
          {error && <p className="text-[10px] font-mono text-red-400">{error}</p>}
          <input ref={fileRef} type="file" accept=".json" className="hidden" onChange={handleFile} />
          <button
            onClick={() => fileRef.current?.click()}
            className="px-4 py-2 text-xs font-mono border border-[#00a328] text-[#00a328] hover:bg-[#00a328]/10 transition-colors">
            ↑ OPEN vfx-presets.json
          </button>
          <button
            onClick={handleNew}
            className="px-4 py-2 text-xs font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:text-[#00a328] hover:border-[#00a328] transition-colors">
            + NEW PRESET FILE
          </button>
        </div>
      </main>
    </div>
  );
}
