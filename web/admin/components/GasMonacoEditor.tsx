'use client';

import { useRef, useEffect, useCallback, useState } from 'react';
import dynamic from 'next/dynamic';
import type { OnMount } from '@monaco-editor/react';
import type { editor as EditorNS } from 'monaco-editor';
import type { Monaco } from '@monaco-editor/react';
import { GAS_SCHEMAS } from '../lib/gas-schemas';

// Lazy-load Monaco — SSR incompatible and large.
const MonacoEditorComponent = dynamic(() => import('@monaco-editor/react'), {
  ssr: false,
  loading: () => (
    <div className="flex items-center justify-center h-full text-game-textDim text-sm font-mono">
      <span className="animate-pulse">⚡ Loading editor…</span>
    </div>
  ),
});

const THEME = 'silencer-dark';

export interface CursorInfo {
  line: number;
  col: number;
  lines: number;
  bytes: number;
}

interface Props {
  fileKey: string;                           // e.g. "player"
  uri: string;                               // e.g. "inmemory://gas/player.json"
  value: string;
  onChange: (v: string) => void;
  onMarkersChange: (errorCount: number) => void;
  onCursorChange: (info: CursorInfo) => void;
  onEditorReady: (api: EditorAPI) => void;
}

export interface EditorAPI {
  format: () => void;
  hasErrors: () => boolean;
}

type IEditor  = EditorNS.IStandaloneCodeEditor;
type IModel   = EditorNS.ITextModel;

/** Stable map from uri → model — persists across tab switches */
const modelCache = new Map<string, IModel>();

export default function GasMonacoEditor({
  fileKey: _fileKey,
  uri,
  value,
  onChange,
  onMarkersChange,
  onCursorChange,
  onEditorReady,
}: Props) {
  const editorRef  = useRef<IEditor | null>(null);
  const monacoRef  = useRef<Monaco | null>(null);
  const [ready, setReady] = useState(false);

  // ── Theme definition (once per Monaco instance) ───────────────────────────
  function applyTheme(monaco: Monaco) {
    monaco.editor.defineTheme(THEME, {
      base: 'vs-dark',
      inherit: true,
      rules: [
        { token: 'string.key.json',        foreground: '7dff9d', fontStyle: 'bold' },
        { token: 'string.value.json',      foreground: 'b8f5c0' },
        { token: 'number.json',            foreground: 'f59e0b' },
        { token: 'keyword.json',           foreground: '22d3ee' },
        { token: 'comment',                foreground: '3a6b3a', fontStyle: 'italic' },
        { token: 'delimiter.bracket.json', foreground: '00a328' },
        { token: 'delimiter.colon.json',   foreground: '4a7a4a' },
        { token: 'delimiter.comma.json',   foreground: '4a7a4a' },
      ],
      colors: {
        'editor.background':                     '#050a05',
        'editor.foreground':                     '#d1fad7',
        'editor.lineHighlightBackground':        '#0a180a',
        'editor.lineHighlightBorder':            '#00000000',
        'editor.selectionBackground':            '#005b1c66',
        'editor.selectionHighlightBackground':   '#005b1c33',
        'editor.wordHighlightBackground':        '#00a32818',
        'editorLineNumber.foreground':           '#2d4a2d',
        'editorLineNumber.activeForeground':     '#00a328',
        'editorCursor.foreground':               '#00ff41',
        'editorIndentGuide.background1':         '#0d1e0d',
        'editorIndentGuide.activeBackground1':   '#1a3d1a',
        'editorBracketMatch.background':         '#005b1c44',
        'editorBracketMatch.border':             '#00a328',
        'editorWidget.background':               '#080e08',
        'editorWidget.border':                   '#1a2e1a',
        'editorSuggestWidget.background':        '#080e08',
        'editorSuggestWidget.border':            '#1a2e1a',
        'editorSuggestWidget.selectedBackground':'#0a2e0a',
        'editorSuggestWidget.highlightForeground':'#00a328',
        'editorHoverWidget.background':          '#080e08',
        'editorHoverWidget.border':              '#1a2e1a',
        'editorInfo.foreground':                 '#22d3ee',
        'editorWarning.foreground':              '#f59e0b',
        'editorError.foreground':                '#ef4444',
        'scrollbarSlider.background':            '#005b1c55',
        'scrollbarSlider.hoverBackground':       '#00a32877',
        'scrollbarSlider.activeBackground':      '#00a328',
        'minimap.background':                    '#030603',
        'minimapSlider.background':              '#005b1c44',
        'minimapSlider.hoverBackground':         '#00a32844',
        'focusBorder':                           '#00a328',
        'list.focusBackground':                  '#0a2e0a',
        'list.hoverBackground':                  '#0d1e0d',
      },
    });
    monaco.editor.setTheme(THEME);
  }

  // ── Mount ─────────────────────────────────────────────────────────────────
  const handleMount: OnMount = useCallback((editor, monaco) => {
    editorRef.current  = editor;
    monacoRef.current  = monaco;

    applyTheme(monaco);

    // Register JSON schemas for all GAS files (once per Monaco instance)
    monaco.languages.json.jsonDefaults.setDiagnosticsOptions({
      validate: true,
      allowComments: false,
      schemas: Object.values(GAS_SCHEMAS).map(({ uri, schema }) => ({
        uri,
        fileMatch: [uri],
        schema,
      })),
    });

    setReady(true);

    // Cursor position tracking
    editor.onDidChangeCursorPosition(e => {
      const model = editor.getModel();
      onCursorChange({
        line:  e.position.lineNumber,
        col:   e.position.column,
        lines: model?.getLineCount() ?? 0,
        bytes: new TextEncoder().encode(model?.getValue() ?? '').length,
      });
    });

    // Keyboard: Ctrl/Cmd+S → custom save event
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => {
      document.dispatchEvent(new CustomEvent('gas:save'));
    });

    // Expose API to parent
    onEditorReady({
      format: () => {
        const markers = monaco.editor.getModelMarkers({});
        const errors  = markers.filter((m: { severity: number }) => m.severity === monaco.MarkerSeverity.Error);
        if (errors.length === 0) {
          editor.getAction('editor.action.formatDocument')?.run();
        }
      },
      hasErrors: () => {
        const markers = monaco.editor.getModelMarkers({});
        return markers.some((m: { severity: number }) => m.severity === (monacoRef.current?.MarkerSeverity.Error ?? 8));
      },
    });

    // Marker listener for error badges
    monaco.editor.onDidChangeMarkers((_uris: unknown) => {
      const model  = editor.getModel();
      if (!model) return;
      const markers = monaco.editor.getModelMarkers({ resource: model.uri });
      const errors  = markers.filter((m: { severity: number }) => m.severity === monaco.MarkerSeverity.Error);
      onMarkersChange(errors.length);
    });
  }, [onCursorChange, onEditorReady, onMarkersChange]);

  // ── Clear stale model refs on unmount ────────────────────────────────────
  useEffect(() => {
    return () => {
      modelCache.clear();
      editorRef.current = null;
      monacoRef.current = null;
    };
  }, []);

  // ── Swap model when URI changes (tab switch) ──────────────────────────────
  useEffect(() => {
    const monaco = monacoRef.current;
    const editor = editorRef.current;
    if (!monaco || !editor || !ready) return;

    const monacoUri = monaco.Uri.parse(uri);
    let model = modelCache.get(uri) ?? monaco.editor.getModel(monacoUri);

    // Discard stale models from a previous Monaco instance
    if (model?.isDisposed()) {
      modelCache.delete(uri);
      model = null;
    }

    if (!model) {
      model = monaco.editor.createModel(value, 'json', monacoUri);
      modelCache.set(uri, model);
    }

    editor.setModel(model);

    // Re-report cursor for the new model
    const pos = editor.getPosition();
    onCursorChange({
      line:  pos?.lineNumber ?? 1,
      col:   pos?.column ?? 1,
      lines: model.getLineCount(),
      bytes: new TextEncoder().encode(model.getValue()).length,
    });

    // Re-report markers for new model
    const markers = monaco.editor.getModelMarkers({ resource: monacoUri });
    const errors  = markers.filter((m: { severity: number }) => m.severity === monaco.MarkerSeverity.Error);
    onMarkersChange(errors.length);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [uri, ready]);

  // ── Sync external value changes into the cached model ────────────────────
  useEffect(() => {
    const monaco = monacoRef.current;
    const editor = editorRef.current;
    if (!monaco || !editor || !ready) return;

    const model = editor.getModel();
    if (model && model.getValue() !== value) {
      // Use pushEditOperations for undo-able updates
      model.pushEditOperations([], [{ range: model.getFullModelRange(), text: value }], () => null);
    }
  }, [value, ready]);

  return (
    <MonacoEditorComponent
      language="json"
      defaultValue={value}
      onMount={handleMount}
      onChange={v => onChange(v ?? '')}
      options={{
        theme:               THEME,
        minimap:             { enabled: true, scale: 1, renderCharacters: false },
        fontSize:            13,
        lineHeight:          20,
        fontFamily:          '"Cascadia Code", "Fira Code", "JetBrains Mono", "Courier New", monospace',
        fontLigatures:       true,
        tabSize:             2,
        insertSpaces:        true,
        scrollBeyondLastLine:false,
        automaticLayout:     true,
        formatOnPaste:       true,
        wordWrap:            'off',
        bracketPairColorization: { enabled: true },
        guides:              { bracketPairs: true, indentation: true },
        renderLineHighlight: 'all',
        cursorBlinking:      'phase',
        cursorStyle:         'line',
        smoothScrolling:     true,
        padding:             { top: 12, bottom: 12 },
        scrollbar:           { useShadows: false, verticalScrollbarSize: 6, horizontalScrollbarSize: 6 },
        suggest:             { showStatusBar: true },
        // JSON features
        quickSuggestions:    { strings: true },
        hover:               { enabled: true },
      }}
    />
  );
}
