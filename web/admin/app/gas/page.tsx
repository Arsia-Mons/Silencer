'use client';
import { useRef, useState, useCallback, useEffect } from 'react';
import dynamic from 'next/dynamic';
import { useAuth } from '../../lib/auth';
import { useSocket } from '../../lib/socket';
import { getStats } from '../../lib/api';
import type { StatsSnapshot } from '../../lib/types';
import Sidebar from '../../components/Sidebar';
import type { EditorAPI, CursorInfo } from '../../components/GasMonacoEditor';
import { GAS_SCHEMAS } from '../../lib/gas-schemas';
// Lazy-load the Monaco-based editor (client only)
const GasMonacoEditor = dynamic(() => import('../../components/GasMonacoEditor'), { ssr: false });

const TABS = [
  { label: 'PLAYER',       file: 'player',      icon: '👤' },
  { label: 'AGENCIES',     file: 'agencies',    icon: '🏛' },
  { label: 'WEAPONS',      file: 'weapons',     icon: '🔫' },
  { label: 'ENEMIES',      file: 'enemies',     icon: '🤖' },
  { label: 'ITEMS',        file: 'items',       icon: '🛒' },
  { label: 'GAME OBJECTS', file: 'gameobjects', icon: '🏗' },
  { label: 'ABILITIES',    file: 'abilities',   icon: '⚡' },
] as const;

type FileKey = (typeof TABS)[number]['file'];

export default function GasPage() {
  useAuth();
  const wsConnected = useSocket({});

  const [stats, setStats] = useState<StatsSnapshot | null>(null);
  useEffect(() => {
    const load = () => getStats().then(setStats).catch(() => {});
    load();
    const t = setInterval(load, 10_000);
    return () => clearInterval(t);
  }, []);

  const folderInputRef    = useRef<HTMLInputElement>(null);
  const editorApiRef      = useRef<EditorAPI | null>(null);

  const [localFolder, setLocalFolder] = useState<string | null>(null);
  const [files,       setFiles]       = useState<Partial<Record<FileKey, string>>>({});
  const [savedFiles,  setSavedFiles]  = useState<Partial<Record<FileKey, string>>>({});
  const [activeTab,   setActiveTab]   = useState<FileKey>('player');
  const [errors,      setErrors]      = useState<Partial<Record<FileKey, number>>>({});
  const [saveMsg,     setSaveMsg]     = useState('');
  const [saveErr,     setSaveErr]     = useState('');
  const [cursor,      setCursor]      = useState<CursorInfo>({ line: 1, col: 1, lines: 0, bytes: 0 });
  const [copyTick,    setCopyTick]    = useState(false);

  // ── Folder picker ─────────────────────────────────────────────────────────
  async function handleFolderPicked(e: React.ChangeEvent<HTMLInputElement>) {
    const picked = e.target.files;
    if (!picked || picked.length === 0) return;
    const first = picked[0] as { webkitRelativePath?: string } & File;
    const folderName = first?.webkitRelativePath?.split('/')[0] ?? 'local';
    const data: Partial<Record<FileKey, string>> = {};
    await Promise.all(
      Array.from(picked)
        .filter(f => f.name.endsWith('.json'))
        .map(f => f.text().then(text => {
          const key = f.name.replace('.json', '') as FileKey;
          if (TABS.some(t => t.file === key)) data[key] = text;
        }))
    );
    setFiles(data);
    setSavedFiles(data);
    setLocalFolder(folderName);
    setSaveErr('');
    setSaveMsg('');
    setErrors({});
    e.target.value = '';
  }

  function handleCloseFolder() {
    setLocalFolder(null);
    setFiles({});
    setSavedFiles({});
    setSaveErr('');
    setSaveMsg('');
    setErrors({});
  }

  function handleTextChange(value: string) {
    setFiles(prev => ({ ...prev, [activeTab]: value }));
    setSaveErr('');
    setSaveMsg('');
  }

  // ── Save ──────────────────────────────────────────────────────────────────
  const handleSave = useCallback(async () => {
    const content = files[activeTab] ?? '';
    try { JSON.parse(content); } catch {
      setSaveErr('Invalid JSON — fix syntax before saving.');
      return;
    }
    const filename = activeTab;
    if (typeof window !== 'undefined' && 'showSaveFilePicker' in window) {
      try {
        const handle = await (window as unknown as {
          showSaveFilePicker: (o: unknown) => Promise<FileSystemFileHandle>
        }).showSaveFilePicker({
          suggestedName: `${filename}.json`,
          types: [{ description: 'JSON', accept: { 'application/json': ['.json'] } }],
        });
        const writable = await handle.createWritable();
        await writable.write(content);
        await writable.close();
      } catch { return; }
    } else {
      const blob = new Blob([content], { type: 'application/json' });
      const url  = URL.createObjectURL(blob);
      const a    = document.createElement('a');
      a.href     = url;
      a.download = `${filename}.json`;
      a.click();
      URL.revokeObjectURL(url);
    }
    setSavedFiles(prev => ({ ...prev, [activeTab]: content }));
    setSaveErr('');
    setSaveMsg(`✓ Saved ${filename}.json`);
    setTimeout(() => setSaveMsg(''), 2500);
  }, [files, activeTab]);

  // ── Format ────────────────────────────────────────────────────────────────
  function handleFormat() {
    if (editorApiRef.current?.hasErrors()) {
      setSaveErr('Cannot format: fix syntax errors first.');
      return;
    }
    editorApiRef.current?.format();
  }

  // ── Copy ──────────────────────────────────────────────────────────────────
  async function handleCopy() {
    const content = files[activeTab] ?? '';
    try {
      if (navigator.clipboard) {
        await navigator.clipboard.writeText(content);
      } else {
        const ta = document.createElement('textarea');
        ta.value = content;
        document.body.appendChild(ta);
        ta.select();
        document.execCommand('copy');
        document.body.removeChild(ta);
      }
      setCopyTick(true);
      setTimeout(() => setCopyTick(false), 1500);
    } catch { /* silent */ }
  }

  // ── Ctrl+S global listener ────────────────────────────────────────────────
  useEffect(() => {
    const onSave = () => handleSave();
    const onKeyDown = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === 's') {
        e.preventDefault();
        handleSave();
      }
    };
    document.addEventListener('gas:save', onSave);
    window.addEventListener('keydown', onKeyDown);
    return () => {
      document.removeEventListener('gas:save', onSave);
      window.removeEventListener('keydown', onKeyDown);
    };
  }, [handleSave]);

  // ── Derived state ─────────────────────────────────────────────────────────
  const activeErrors  = errors[activeTab] ?? 0;
  const isDirty       = files[activeTab] !== savedFiles[activeTab];
  const sizeKB        = (cursor.bytes / 1024).toFixed(1);
  const totalErrors   = Object.values(errors).reduce((s, n) => s + (n ?? 0), 0);

  // ── Render ────────────────────────────────────────────────────────────────
  return (
    <div className="flex h-screen overflow-hidden bg-game-bg text-game-text">
      <Sidebar wsConnected={wsConnected} />
      <input
        ref={folderInputRef}
        type="file"
        // @ts-expect-error non-standard
        webkitdirectory=""
        multiple
        className="hidden"
        onChange={handleFolderPicked}
      />

      <div className="flex flex-col flex-1 overflow-hidden">
        {/* ── Top bar ── */}
        <div className="flex items-center justify-between px-6 py-3 border-b border-game-border bg-game-bgCard shrink-0">
          <div className="flex items-center gap-4">
            <div>
              <h1 className="text-lg font-bold tracking-widest text-game-primary leading-none">GAS EDITOR</h1>
              <p className="text-game-textDim text-xs mt-0.5 tracking-wide">Game Ability System — data-driven balance</p>
            </div>
            {totalErrors > 0 && (
              <span className="text-xs font-mono text-game-danger border border-game-danger/40 px-2 py-0.5">
                {totalErrors} error{totalErrors !== 1 ? 's' : ''} across files
              </span>
            )}
          </div>

          {/* ── Service status pills ── */}
          <div className="flex items-center gap-3">
            {[
              { label: 'LOBBY',    ok: !!stats },
              { label: 'MONGODB',  ok: stats?.db.status === 'connected' },
              { label: 'RABBITMQ', ok: stats?.rabbitmq.status === 'connected' },
              { label: 'WS',       ok: wsConnected },
            ].map(({ label, ok }) => (
              <div key={label} className="flex items-center gap-1.5 font-mono text-xs">
                <span className={`w-2 h-2 rounded-full shrink-0 ${ok ? 'bg-game-primary' : stats === null && label !== 'WS' ? 'bg-game-border animate-pulse' : 'bg-game-danger'}`} />
                <span className={ok ? 'text-game-textDim' : 'text-game-danger'}>{label}</span>
              </div>
            ))}
            <span className="w-px h-5 bg-game-border mx-1" />
          </div>

          <div className="flex items-center gap-2">
            {localFolder ? (
              <>
                <span className="text-xs text-game-warning tracking-wider border border-game-warning/30 px-2 py-1 font-mono">
                  📁 {localFolder}
                </span>
                <button
                  onClick={handleCloseFolder}
                  className="px-3 py-1.5 border border-game-border text-game-textDim hover:text-game-danger text-xs tracking-wider transition-colors"
                >
                  ✕ CLOSE
                </button>
              </>
            ) : (
              <button
                onClick={() => folderInputRef.current?.click()}
                className="px-4 py-2 border border-game-primary text-game-primary hover:bg-game-primary/10 text-sm tracking-wider font-bold transition-colors"
              >
                📁 OPEN GAS FOLDER
              </button>
            )}
          </div>
        </div>

        {/* ── Empty state ── */}
        {!localFolder && (
          <div className="flex-1 flex items-center justify-center">
            <div className="text-center flex flex-col items-center gap-6">
              <div className="text-6xl opacity-60">⚡</div>
              <div>
                <p className="text-game-text text-lg font-bold tracking-wide mb-1">GAS EDITOR</p>
                <p className="text-game-textDim text-sm">Open <code className="text-game-primary">shared/assets/gas/</code> to start editing</p>
              </div>
              <div className="text-xs text-game-textDim font-mono space-y-1 border border-game-border p-4 text-left">
                <p>✦ Syntax highlighting &amp; line numbers</p>
                <p>✦ JSON schema validation with hover docs</p>
                <p>✦ Format with Ctrl+Shift+F · Save with Ctrl+S</p>
                <p>✦ Per-file error badges · dirty state tracking</p>
              </div>
              <button
                onClick={() => folderInputRef.current?.click()}
                className="px-8 py-3 border border-game-primary text-game-primary hover:bg-game-primary/10 text-sm tracking-widest font-bold transition-colors"
              >
                OPEN GAS FOLDER
              </button>
            </div>
          </div>
        )}

        {/* ── Editor area ── */}
        {localFolder && (
          <div className="flex flex-col flex-1 overflow-hidden">
            {/* Tab bar */}
            <div className="flex items-center border-b border-game-border bg-game-bgCard shrink-0 px-2 pt-1">
              {TABS.map(({ label, file, icon }) => {
                const tabErrors  = errors[file as FileKey] ?? 0;
                const tabDirty   = files[file as FileKey] !== savedFiles[file as FileKey];
                const missing    = !(file as FileKey in files);
                const isActive   = activeTab === file;
                return (
                  <button
                    key={file}
                    onClick={() => { setActiveTab(file as FileKey); setSaveErr(''); setSaveMsg(''); }}
                    className={`relative flex items-center gap-1.5 px-4 py-2 text-xs tracking-wider font-mono border-b-2 -mb-px transition-colors ${
                      isActive
                        ? 'border-game-primary text-game-primary bg-game-bg'
                        : 'border-transparent text-game-textDim hover:text-game-text'
                    } ${missing ? 'opacity-40' : ''}`}
                  >
                    <span className="text-sm leading-none">{icon}</span>
                    {label}
                    {tabErrors > 0 && (
                      <span className="w-1.5 h-1.5 rounded-full bg-game-danger shrink-0" title={`${tabErrors} error(s)`} />
                    )}
                    {tabDirty && tabErrors === 0 && (
                      <span className="w-1.5 h-1.5 rounded-full bg-game-warning shrink-0" title="Unsaved changes" />
                    )}
                  </button>
                );
              })}

              {/* Toolbar (right-aligned in tab row) */}
              <div className="ml-auto flex items-center gap-1 pb-1">
                <button
                  onClick={handleFormat}
                  disabled={activeErrors > 0}
                  className="px-3 py-1 text-xs font-mono text-game-textDim hover:text-game-text border border-transparent hover:border-game-border disabled:opacity-30 disabled:cursor-not-allowed transition-colors"
                  title="Format JSON (Ctrl+Shift+F)"
                >
                  { } FORMAT
                </button>
                <button
                  onClick={handleCopy}
                  className="px-3 py-1 text-xs font-mono text-game-textDim hover:text-game-text border border-transparent hover:border-game-border transition-colors"
                  title="Copy to clipboard"
                >
                  {copyTick ? '✓ COPIED' : '⎘ COPY'}
                </button>
                <button
                  onClick={handleSave}
                  disabled={!isDirty && !saveMsg}
                  className={`px-4 py-1 text-xs font-mono font-bold tracking-wider transition-colors border ${
                    saveMsg
                      ? 'border-game-primary text-game-primary'
                      : isDirty
                      ? 'border-game-warning text-game-warning hover:bg-game-warning/10'
                      : 'border-game-border text-game-textDim'
                  }`}
                  title="Save (Ctrl+S)"
                >
                  {saveMsg || (isDirty ? '● SAVE' : 'SAVE')}
                </button>
              </div>
            </div>

            {/* Monaco editor — fills remaining space */}
            <div className="flex-1 overflow-hidden">
              {files[activeTab] !== undefined ? (
                <GasMonacoEditor
                  fileKey={activeTab}
                  uri={GAS_SCHEMAS[activeTab]?.uri ?? `inmemory://gas/${activeTab}.json`}
                  value={files[activeTab] ?? ''}
                  onChange={handleTextChange}
                  onMarkersChange={count => setErrors(prev => ({ ...prev, [activeTab]: count }))}
                  onCursorChange={setCursor}
                  onEditorReady={api => { editorApiRef.current = api; }}
                />
              ) : (
                <div className="flex items-center justify-center h-full text-game-textDim text-sm font-mono">
                  <div className="text-center space-y-2">
                    <p className="text-2xl opacity-40">∅</p>
                    <p>File not found in opened folder</p>
                    <p className="text-xs opacity-60">{activeTab}.json was not in the selected directory</p>
                  </div>
                </div>
              )}
            </div>

            {/* ── Status bar ── */}
            <div className="flex items-center justify-between px-4 py-1 bg-game-bgCard border-t border-game-border text-xs font-mono shrink-0">
              <div className="flex items-center gap-4 text-game-textDim">
                <span>Ln {cursor.line}, Col {cursor.col}</span>
                <span className="text-game-border">│</span>
                <span>{cursor.lines} lines</span>
                <span className="text-game-border">│</span>
                <span>{sizeKB} KB</span>
                <span className="text-game-border">│</span>
                <span>UTF-8 · JSON</span>
              </div>
              <div className="flex items-center gap-4">
                {saveErr && (
                  <span className="text-game-danger">{saveErr}</span>
                )}
                {activeErrors > 0 ? (
                  <span className="text-game-danger">⊗ {activeErrors} error{activeErrors !== 1 ? 's' : ''}</span>
                ) : (
                  <span className="text-game-primary">✓ Valid JSON</span>
                )}
                <span className="text-game-textDim">
                  {isDirty ? <span className="text-game-warning">● modified</span> : 'saved'}
                </span>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
