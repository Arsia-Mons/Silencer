'use client';
import { useRef, useState, useCallback, useEffect, Suspense } from 'react';
import { useRouter, useSearchParams } from 'next/navigation';
import dynamic from 'next/dynamic';
import { useAuth } from '../../lib/auth';
import { useSocket } from '../../lib/socket';
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

// Required top-level keys per file (what the game loader expects).
// Deep comparison: every key/field present in `orig` must exist in `curr`.
// Array items are matched by their `id` property.
function collectViolations(curr: unknown, orig: unknown, path: string): string[] {
  if (typeof orig !== 'object' || orig === null) return [];
  if (typeof curr !== 'object' || curr === null) return [path ? `"${path}" removed` : 'root removed'];
  const v: string[] = [];
  if (Array.isArray(orig)) {
    if (!Array.isArray(curr)) return [path ? `"${path}" changed from array` : 'root changed from array'];
    for (const origItem of orig) {
      if (typeof origItem !== 'object' || origItem === null) continue;
      const id = (origItem as Record<string, unknown>).id;
      if (id === undefined) continue;
      const currItem = (curr as unknown[]).find(
        i => typeof i === 'object' && i !== null && (i as Record<string, unknown>).id === id,
      );
      if (!currItem) { v.push(`[id=${id}] entry removed`); continue; }
      v.push(...collectViolations(currItem, origItem, `[id=${id}]`));
    }
  } else {
    const o = orig as Record<string, unknown>;
    const c = curr as Record<string, unknown>;
    for (const key of Object.keys(o)) {
      const label = path ? `${path}.${key}` : key;
      if (!(key in c)) {
        v.push(`"${label}" removed`);
      } else if (typeof o[key] === 'object' && o[key] !== null) {
        v.push(...collectViolations(c[key], o[key], label));
      }
    }
  }
  return v;
}

function validateBaseline(current: string, original: string): string[] {
  if (!original) return [];
  try {
    return collectViolations(JSON.parse(current), JSON.parse(original), '');
  } catch { return []; }
}

function GasPageInner() {
  useAuth();
  const wsConnected = useSocket({});
  const router = useRouter();
  const searchParams = useSearchParams();

  const folderInputRef    = useRef<HTMLInputElement>(null);
  const editorApiRef      = useRef<EditorAPI | null>(null);

  const [localFolder, setLocalFolder] = useState<string | null>(null);
  const [files,         setFiles]         = useState<Partial<Record<FileKey, string>>>({});
  const [savedFiles,    setSavedFiles]    = useState<Partial<Record<FileKey, string>>>({});
  const [originalFiles, setOriginalFiles] = useState<Partial<Record<FileKey, string>>>({});
  const [errors,      setErrors]      = useState<Partial<Record<FileKey, number>>>({});
  const [saveMsg,     setSaveMsg]     = useState('');
  const [saveErr,     setSaveErr]     = useState('');
  const [cursor,      setCursor]      = useState<CursorInfo>({ line: 1, col: 1, lines: 0, bytes: 0 });
  const [copyTick,    setCopyTick]    = useState(false);
  const [showValidate, setShowValidate] = useState(false);

  // URL-driven active tab — ?tab=weapons etc.
  const tabParam = searchParams.get('tab') as FileKey | null;
  const validTab = TABS.some(t => t.file === tabParam) ? tabParam! : 'player';
  const [activeTab, setActiveTab] = useState<FileKey>(validTab);

  // Keep local state in sync if URL param changes (e.g. back/forward).
  useEffect(() => {
    if (validTab !== activeTab) setActiveTab(validTab);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [validTab]);

  function switchTab(file: FileKey) {
    setActiveTab(file);
    setSaveErr('');
    setSaveMsg('');
    const params = new URLSearchParams(searchParams.toString());
    params.set('tab', file);
    router.replace(`?${params.toString()}`, { scroll: false });
  }

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
    setOriginalFiles(data);
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
    setOriginalFiles({});
    setSaveMsg('');
    setErrors({});
    setShowValidate(false);
  }

  function handleTextChange(value: string) {
    setFiles(prev => ({ ...prev, [activeTab]: value }));
    setSaveErr('');
    setSaveMsg('');
  }

  // ── Save ──────────────────────────────────────────────────────────────────
  const handleSave = useCallback(async () => {
    const content = files[activeTab] ?? '';
    // 1. JSON parse check
    try { JSON.parse(content); } catch {
      setSaveErr('Invalid JSON — fix syntax before saving.');
      return;
    }
    // 2. Monaco schema error check
    const monacoErrors = errors[activeTab] ?? 0;
    if (monacoErrors > 0) {
      setSaveErr(`Fix ${monacoErrors} schema error${monacoErrors !== 1 ? 's' : ''} before saving.`);
      return;
    }
    // 3. Baseline integrity check — every key/field from the original file must still be present
    const violations = validateBaseline(content, originalFiles[activeTab] ?? '');
    if (violations.length > 0) {
      setSaveErr(`${violations.length} baseline violation${violations.length !== 1 ? 's' : ''}: ${violations.slice(0, 2).join('; ')}${violations.length > 2 ? ` (+${violations.length - 2} more — open Validate All)` : ''}`);
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
  }, [files, activeTab, errors, originalFiles]);

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

  // Per-tab validation summary (for the validate panel)
  const validateSummary = TABS.map(({ label, file, icon }) => {
    const content = files[file] ?? '';
    const monacoErr = errors[file] ?? 0;
    const violations = validateBaseline(content, originalFiles[file] ?? '');
    const absent = !(file in files);
    return { file, label, icon, monacoErr, violations, absent };
  });
  const allValid = !localFolder || validateSummary.every(r => r.absent || (r.monacoErr === 0 && r.violations.length === 0));

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

          <div className="flex items-center gap-2">
            {/* Validate All button */}
            {localFolder && (
              <button
                onClick={() => setShowValidate(v => !v)}
                className={`px-3 py-1.5 text-xs font-mono border transition-colors ${
                  showValidate
                    ? 'border-game-primary text-game-primary'
                    : allValid
                    ? 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-text'
                    : 'border-game-danger text-game-danger hover:border-game-danger/60'
                }`}
              >
                {allValid ? '✓ VALIDATE ALL' : `⊗ VALIDATE ALL`}
              </button>
            )}

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
                <p>✦ Validate All checks required keys before save</p>
                <p>✦ Each tab has a direct URL link (?tab=weapons)</p>
              </div>
              {/* Quick tab links even before folder is open */}
              <div className="flex flex-wrap gap-1 justify-center">
                {TABS.map(({ label, file, icon }) => (
                  <a
                    key={file}
                    href={`?tab=${file}`}
                    className="px-2 py-1 text-[10px] font-mono border border-game-border text-game-textDim hover:border-game-primary hover:text-game-text rounded transition-colors"
                  >
                    {icon} {label}
                  </a>
                ))}
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
                const tabErrors     = errors[file as FileKey] ?? 0;
                const tabViolations = validateBaseline(files[file as FileKey] ?? '', originalFiles[file as FileKey] ?? '');
                const tabDirty      = files[file as FileKey] !== savedFiles[file as FileKey];
                const missing       = !(file as FileKey in files);
                const isActive      = activeTab === file;
                const hasIssue      = tabErrors > 0 || tabViolations.length > 0;
                return (
                  <a
                    key={file}
                    href={`?tab=${file}`}
                    onClick={e => { e.preventDefault(); switchTab(file as FileKey); }}
                    className={`relative flex items-center gap-1.5 px-4 py-2 text-xs tracking-wider font-mono border-b-2 -mb-px transition-colors ${
                      isActive
                        ? 'border-game-primary text-game-primary bg-game-bg'
                        : 'border-transparent text-game-textDim hover:text-game-text'
                    } ${missing ? 'opacity-40' : ''}`}
                    title={`/gas?tab=${file}`}
                  >
                    <span className="text-sm leading-none">{icon}</span>
                    {label}
                    {hasIssue && (
                      <span className="w-1.5 h-1.5 rounded-full bg-game-danger shrink-0" title={`${tabErrors > 0 ? `${tabErrors} error(s)` : ''}${tabViolations.length > 0 ? ` ${tabViolations.length} removed field(s)` : ''}`} />
                    )}
                    {tabDirty && !hasIssue && (
                      <span className="w-1.5 h-1.5 rounded-full bg-game-warning shrink-0" title="Unsaved changes" />
                    )}
                  </a>
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

            {/* ── Validation tray (Problems panel) ── */}
            {showValidate && (
              <div className="border-b border-game-border bg-game-bg shrink-0 flex flex-col" style={{ maxHeight: '40%' }}>
                <div className="flex items-center justify-between px-4 py-2 border-b border-game-border bg-game-bgCard shrink-0">
                  <span className="text-xs font-mono text-game-primary tracking-wider">
                    PROBLEMS — baseline integrity check (files as loaded = required)
                  </span>
                  <button
                    onClick={() => setShowValidate(false)}
                    className="text-xs font-mono text-game-textDim hover:text-game-text px-2 py-0.5 border border-game-border hover:border-game-primary transition-colors"
                  >
                    ✕ CLOSE
                  </button>
                </div>
                <div className="overflow-y-auto">
                  {validateSummary.every(r => r.absent || (r.monacoErr === 0 && r.violations.length === 0)) ? (
                    <div className="px-6 py-4 text-xs font-mono text-game-primary">✓ All files pass baseline check</div>
                  ) : (
                    validateSummary.map(({ file, label, icon, monacoErr, violations, absent }) => {
                      const ok = !absent && monacoErr === 0 && violations.length === 0;
                      if (ok || absent) return null;
                      return (
                        <div key={file} className="border-b border-game-border/40 last:border-0">
                          {/* Tab header row */}
                          <button
                            onClick={() => switchTab(file as FileKey)}
                            className="w-full flex items-center gap-2 px-4 py-2 text-left hover:bg-game-bgCard transition-colors"
                          >
                            <span className="text-sm shrink-0">{icon}</span>
                            <span className="text-xs font-mono font-bold text-game-danger tracking-wider">{label}</span>
                            <span className="text-xs font-mono text-game-textDim ml-1">
                              {violations.length > 0 && `${violations.length} removed field${violations.length !== 1 ? 's' : ''}`}
                              {violations.length > 0 && monacoErr > 0 && '  ·  '}
                              {monacoErr > 0 && `${monacoErr} schema error${monacoErr !== 1 ? 's' : ''}`}
                            </span>
                            <span className="ml-auto text-[10px] text-game-textDim font-mono">click to jump →</span>
                          </button>
                          {/* Violation rows */}
                          {violations.map((v, i) => (
                            <div key={i} className="flex items-start gap-3 px-8 py-1 text-xs font-mono text-game-danger hover:bg-game-bgCard/50">
                              <span className="text-game-danger/60 shrink-0 select-none">⊗</span>
                              <span className="break-all">{v}</span>
                            </div>
                          ))}
                          {monacoErr > 0 && (
                            <div className="flex items-start gap-3 px-8 py-1 text-xs font-mono text-game-warning">
                              <span className="shrink-0 select-none">⚠</span>
                              <span>{monacoErr} JSON schema error{monacoErr !== 1 ? 's' : ''} — check Monaco editor for details</span>
                            </div>
                          )}
                        </div>
                      );
                    })
                  )}
                </div>
              </div>
            )}

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

export default function GasPage() {
  return (
    <Suspense>
      <GasPageInner />
    </Suspense>
  );
}
