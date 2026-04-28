'use client';
import { useRef, useState } from 'react';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';

const TABS = [
  { label: 'PLAYER',       file: 'player' },
  { label: 'AGENCIES',     file: 'agencies' },
  { label: 'WEAPONS',      file: 'weapons' },
  { label: 'ENEMIES',      file: 'enemies' },
  { label: 'ITEMS',        file: 'items' },
  { label: 'GAME OBJECTS', file: 'gameobjects' },
  { label: 'ABILITIES',    file: 'abilities' },
] as const;

type FileKey = (typeof TABS)[number]['file'];

export default function GasPage() {
  useAuth();

  const folderInputRef = useRef<HTMLInputElement>(null);
  const [localFolder, setLocalFolder] = useState<string | null>(null);
  const [files, setFiles] = useState<Partial<Record<FileKey, string>>>({});
  const [activeTab, setActiveTab] = useState<FileKey>('player');
  const [error, setError] = useState('');
  const [savedMsg, setSavedMsg] = useState('');

  async function handleFolderPicked(e: React.ChangeEvent<HTMLInputElement>) {
    const picked = e.target.files;
    if (!picked || picked.length === 0) return;
    const first = picked[0] as { webkitRelativePath?: string } & File;
    const folderName = first?.webkitRelativePath?.split('/')[0] ?? 'local';
    const data: Partial<Record<FileKey, string>> = {};
    await Promise.all(
      Array.from(picked)
        .filter(f => f.name.endsWith('.json'))
        .map(f =>
          f.text().then(text => {
            const key = f.name.replace('.json', '') as FileKey;
            if (TABS.some(t => t.file === key)) data[key] = text;
          })
        )
    );
    setFiles(data);
    setLocalFolder(folderName);
    setError('');
    setSavedMsg('');
    e.target.value = '';
  }

  function handleCloseFolder() {
    setLocalFolder(null);
    setFiles({});
    setError('');
    setSavedMsg('');
  }

  function handleTextChange(value: string) {
    setFiles(prev => ({ ...prev, [activeTab]: value }));
    setError('');
    setSavedMsg('');
  }

  async function handleSave() {
    const content = files[activeTab] ?? '';
    try {
      JSON.parse(content);
    } catch {
      setError('Invalid JSON — fix syntax before saving.');
      return;
    }
    const filename = activeTab;
    if (typeof window !== 'undefined' && 'showSaveFilePicker' in window) {
      try {
        const handle = await (window as unknown as { showSaveFilePicker: (opts: unknown) => Promise<FileSystemFileHandle> }).showSaveFilePicker({
          suggestedName: `${filename}.json`,
          types: [{ description: 'JSON', accept: { 'application/json': ['.json'] } }],
        });
        const writable = await handle.createWritable();
        await writable.write(content);
        await writable.close();
      } catch {
        return;
      }
    } else {
      const blob = new Blob([content], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `${filename}.json`;
      a.click();
      URL.revokeObjectURL(url);
    }
    setError('');
    setSavedMsg(`Saved ${filename}.json`);
    setTimeout(() => setSavedMsg(''), 2000);
  }

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar />
      <input
        ref={folderInputRef}
        type="file"
        // @ts-expect-error non-standard attribute
        webkitdirectory=""
        multiple
        className="hidden"
        onChange={handleFolderPicked}
      />

      <main className="flex-1 p-8">
        {/* Header */}
        <div className="flex items-center justify-between mb-6">
          <div>
            <h1 className="text-2xl font-bold tracking-widest text-game-primary">GAS EDITOR</h1>
            <p className="text-game-textDim text-sm mt-1">
              Edit game balance data — player stats, agencies, weapons, items, enemies, game objects
            </p>
          </div>
          <div className="flex items-center gap-3">
            {localFolder ? (
              <>
                <span className="text-xs text-game-warning tracking-wider border border-game-warning/40 px-2 py-1">
                  📁 {localFolder}
                </span>
                <button
                  onClick={handleCloseFolder}
                  className="px-3 py-2 border border-game-border text-game-textDim hover:text-game-danger text-sm tracking-wider"
                >
                  ✕ CLOSE
                </button>
              </>
            ) : (
              <button
                onClick={() => folderInputRef.current?.click()}
                className="px-4 py-2 border border-game-primary text-game-primary hover:bg-game-primary/10 text-sm tracking-wider font-bold"
              >
                📁 OPEN GAS FOLDER
              </button>
            )}
          </div>
        </div>

        {/* Empty state */}
        {!localFolder && (
          <div className="text-game-textDim text-sm border border-game-border p-12 text-center flex flex-col items-center gap-4">
            <div className="text-4xl">⚡</div>
            <div>Open the <code>shared/assets/gas/</code> folder to start editing.</div>
            <button
              onClick={() => folderInputRef.current?.click()}
              className="px-6 py-3 border border-game-primary text-game-primary hover:bg-game-primary/10 text-sm tracking-wider font-bold"
            >
              OPEN GAS FOLDER
            </button>
          </div>
        )}

        {/* Editor */}
        {localFolder && (
          <>
            {/* Tabs */}
            <div className="flex border-b border-game-border mb-4">
              {TABS.map(({ label, file }) => (
                <button
                  key={file}
                  onClick={() => { setActiveTab(file); setError(''); setSavedMsg(''); }}
                  className={`px-4 py-2 text-xs tracking-wider font-mono border-b-2 -mb-px transition-colors ${
                    activeTab === file
                      ? 'border-game-primary text-game-primary'
                      : 'border-transparent text-game-textDim hover:text-game-text'
                  }`}
                >
                  {label}
                </button>
              ))}
            </div>

            {/* Textarea */}
            <textarea
              className="font-mono text-sm bg-game-bg border border-game-border text-game-text p-4 w-full"
              style={{ minHeight: '60vh' }}
              value={files[activeTab] ?? ''}
              onChange={e => handleTextChange(e.target.value)}
              spellCheck={false}
            />

            {/* Actions */}
            <div className="flex items-center gap-4 mt-3">
              <button
                onClick={handleSave}
                className="px-6 py-2 border border-game-primary text-game-primary hover:bg-game-primary/10 text-sm tracking-wider font-bold"
              >
                SAVE
              </button>
              {error && <span className="text-game-danger text-sm">{error}</span>}
              {savedMsg && <span className="text-game-primary text-sm">{savedMsg}</span>}
            </div>
          </>
        )}
      </main>
    </div>
  );
}
