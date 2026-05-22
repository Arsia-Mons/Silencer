'use client';
import {
  ChangeEvent,
  CSSProperties,
  DragEvent,
  useEffect,
  useMemo,
  useRef,
  useState,
} from 'react';
import Sidebar from '../../components/Sidebar';
import { useAuth } from '../../lib/auth';
import { useWsConnected } from '../../lib/socket';
import {
  PALETTE_NODE_KINDS,
  canHaveChildren,
  createDefaultUiDocument,
  createNode,
  duplicateNode,
  exportClaySnippet,
  findNode,
  findParent,
  insertAfter,
  insertChild,
  removeNode,
  updateNode,
  validateUiDocument,
  type UiAlign,
  type UiAxis,
  type UiDocument,
  type UiFont,
  type UiJustify,
  type UiNode,
  type UiNodeKind,
  type UiSize,
  type UiSizeMode,
  type UiStyle,
} from '../../lib/ui-layout';

const STORAGE_KEY = 'silencer-ui-editor-document-v1';

const KIND_LABELS: Record<UiNodeKind, string> = {
  screen: 'SCREEN',
  panel: 'PANEL',
  stack: 'STACK',
  row: 'ROW',
  text: 'TEXT',
  button: 'BUTTON',
  input: 'INPUT',
  spacer: 'SPACER',
};

const PRESETS = [
  { name: '1280 x 720', width: 1280, height: 720, zoom: 0.72 },
  { name: '960 x 540', width: 960, height: 540, zoom: 0.92 },
  { name: '640 x 480', width: 640, height: 480, zoom: 1 },
];

const SIZE_MODES: UiSizeMode[] = ['fit', 'grow', 'fixed'];
const AXES: UiAxis[] = ['column', 'row'];
const ALIGNS: UiAlign[] = ['start', 'center', 'end', 'stretch'];
const JUSTIFIES: UiJustify[] = ['start', 'center', 'end', 'between'];
const FONTS: UiFont[] = ['ui', 'uiLarge', 'title', 'tiny'];

type ClientPreviewElement = {
  id?: string;
  label?: string;
  kind?: string;
  x: number;
  y: number;
  w: number;
  h: number;
};

type ClientPreviewState = {
  status: 'idle' | 'syncing' | 'live' | 'offline';
  screenshot?: string;
  elements: ClientPreviewElement[];
  error?: string;
};

export default function UiEditorPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const importInputRef = useRef<HTMLInputElement | null>(null);
  const [document, setDocument] = useState<UiDocument>(() => createDefaultUiDocument());
  const [selectedId, setSelectedId] = useState('main-menu-panel');
  const [hydrated, setHydrated] = useState(false);
  const [zoom, setZoom] = useState(0.72);
  const [exportMode, setExportMode] = useState<'json' | 'clay'>('json');
  const [status, setStatus] = useState('READY');
  const [clientPreview, setClientPreview] = useState<ClientPreviewState>({
    status: 'idle',
    elements: [],
  });

  useEffect(() => {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (raw) {
        const parsed = validateUiDocument(JSON.parse(raw));
        setDocument(parsed);
        setSelectedId(parsed.root.id);
      }
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'FAILED TO LOAD LOCAL DOCUMENT');
    } finally {
      setHydrated(true);
    }
  }, []);

  useEffect(() => {
    if (!hydrated) return;
    localStorage.setItem(STORAGE_KEY, JSON.stringify(document));
  }, [document, hydrated]);

  useEffect(() => {
    if (!hydrated) return;
    const controller = new AbortController();
    const timer = window.setTimeout(async () => {
      setClientPreview(prev => ({ ...prev, status: 'syncing', error: undefined }));
      try {
        const response = await fetch('/api/ui-editor/preview', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ document }),
          signal: controller.signal,
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
          throw new Error(data.error ?? 'CLIENT PREVIEW FAILED');
        }
        setClientPreview({
          status: 'live',
          screenshot: data.screenshot,
          elements: inspectElements(data.inspect),
        });
      } catch (error) {
        if (controller.signal.aborted) return;
        setClientPreview(prev => ({
          ...prev,
          status: 'offline',
          error: error instanceof Error ? error.message : 'CLIENT PREVIEW FAILED',
        }));
      }
    }, 240);
    return () => {
      controller.abort();
      window.clearTimeout(timer);
    };
  }, [document, hydrated]);

  const selectedNode = useMemo(() => findNode(document.root, selectedId) ?? document.root, [document, selectedId]);
  const selectedParent = useMemo(() => findParent(document.root, selectedNode.id), [document, selectedNode.id]);
  const exportText = useMemo(() => (
    exportMode === 'json'
      ? JSON.stringify(document, null, 2)
      : exportClaySnippet(document)
  ), [document, exportMode]);

  function commit(next: UiDocument, nextSelectedId = selectedNode.id) {
    setDocument(next);
    setSelectedId(nextSelectedId);
    setStatus('DIRTY');
  }

  function updateSelectedNode(updater: (node: UiNode) => UiNode) {
    commit(updateNode(document, selectedNode.id, updater));
  }

  function patchSelectedNode(patch: Partial<UiNode>) {
    const nextId = patch.id ?? selectedNode.id;
    if (patch.id && patch.id !== selectedNode.id && findNode(document.root, patch.id)) {
      setStatus(`DUPLICATE ID: ${patch.id}`);
      return;
    }
    commit(updateNode(document, selectedNode.id, node => ({ ...node, ...patch })), nextId);
  }

  function updateSelectedStyle(style: Partial<UiStyle>) {
    updateSelectedNode(node => ({ ...node, style: { ...node.style, ...style } }));
  }

  function addNode(kind: UiNodeKind, targetId = selectedNode.id) {
    const target = findNode(document.root, targetId) ?? document.root;
    const node = createNode(kind);
    const next = canHaveChildren(target.kind)
      ? insertChild(document, target.id, node)
      : insertAfter(document, target.id, node);
    commit(next, node.id);
  }

  function deleteSelectedNode() {
    if (selectedNode.id === document.root.id) return;
    const fallbackId = selectedParent?.id ?? document.root.id;
    commit(removeNode(document, selectedNode.id), fallbackId);
  }

  function duplicateSelectedNode() {
    if (selectedNode.id === document.root.id) return;
    const next = duplicateNode(document, selectedNode.id);
    commit(next, selectedNode.id);
  }

  async function importDocument(event: ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    if (!file) return;
    try {
      const parsed = validateUiDocument(JSON.parse(await file.text()));
      setDocument(parsed);
      setSelectedId(parsed.root.id);
      setStatus('IMPORTED');
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'IMPORT FAILED');
    } finally {
      event.target.value = '';
    }
  }

  function downloadDocument() {
    downloadText(`${document.surface}.silencer-ui.json`, JSON.stringify(document, null, 2), 'application/json');
    setStatus('EXPORTED JSON');
  }

  function downloadClay() {
    downloadText(`${document.surface}.clay-scaffold.cpp`, exportClaySnippet(document), 'text/plain');
    setStatus('EXPORTED CLAY');
  }

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 min-w-0 flex flex-col">
        <EditorTopBar
          document={document}
          status={status}
          zoom={zoom}
          onZoom={setZoom}
          onDocumentChange={next => {
            setDocument(next);
            setStatus('DIRTY');
          }}
          onPreset={preset => {
            setDocument({
              ...document,
              viewport: { width: preset.width, height: preset.height },
              root: {
                ...document.root,
                style: {
                  ...document.root.style,
                  width: { mode: 'fixed', value: preset.width },
                  height: { mode: 'fixed', value: preset.height },
                },
              },
            });
            setZoom(preset.zoom);
            setStatus('DIRTY');
          }}
          onImport={() => importInputRef.current?.click()}
          onDownloadJson={downloadDocument}
          onDownloadClay={downloadClay}
          onReset={() => {
            const next = createDefaultUiDocument();
            setDocument(next);
            setSelectedId(next.root.id);
            setStatus('RESET');
          }}
        />

        <div className="min-h-0 flex-1 grid grid-cols-[260px_minmax(0,1fr)_340px] border-t border-game-border">
          <section className="min-h-0 border-r border-game-border bg-game-bgCard/90 flex flex-col">
            <Palette onAdd={addNode} />
            <Hierarchy selectedId={selectedNode.id} root={document.root} onSelect={setSelectedId} />
          </section>

          <section className="min-w-0 min-h-0 flex flex-col bg-black/45">
            <Canvas
              document={document}
              selectedId={selectedNode.id}
              zoom={zoom}
              clientPreview={clientPreview}
              onSelect={setSelectedId}
              onDropNode={(targetId, kind) => addNode(kind, targetId)}
            />
            <ExportPanel
              exportMode={exportMode}
              exportText={exportText}
              onMode={setExportMode}
            />
          </section>

          <Inspector
            node={selectedNode}
            parent={selectedParent}
            isRoot={selectedNode.id === document.root.id}
            onPatch={patchSelectedNode}
            onStyle={updateSelectedStyle}
            onDelete={deleteSelectedNode}
            onDuplicate={duplicateSelectedNode}
            onAddChild={kind => addNode(kind, selectedNode.id)}
          />
        </div>
        <input ref={importInputRef} className="hidden" type="file" accept=".json,.silencer-ui.json,application/json" onChange={importDocument} />
      </main>
    </div>
  );
}

interface EditorTopBarProps {
  document: UiDocument;
  status: string;
  zoom: number;
  onZoom: (zoom: number) => void;
  onDocumentChange: (document: UiDocument) => void;
  onPreset: (preset: typeof PRESETS[number]) => void;
  onImport: () => void;
  onDownloadJson: () => void;
  onDownloadClay: () => void;
  onReset: () => void;
}

function EditorTopBar(props: EditorTopBarProps) {
  const { document, status, zoom, onZoom, onDocumentChange, onPreset, onImport, onDownloadJson, onDownloadClay, onReset } = props;
  return (
    <header className="px-5 py-3 bg-game-bgCard border-b border-game-border flex items-center gap-4">
      <div className="min-w-0">
        <h1 className="text-xl font-bold tracking-widest text-game-primary">UI EDITOR</h1>
        <div className="text-[11px] tracking-wider text-game-textDim">{status}</div>
      </div>
      <label className="ml-4 text-[11px] tracking-widest text-game-textDim">
        SURFACE
        <input
          className="block mt-1 w-44 bg-game-bg border border-game-border px-2 py-1 text-game-text focus:outline-none focus:border-game-primary"
          value={document.surface}
          onChange={event => onDocumentChange({ ...document, surface: slugify(event.target.value) })}
        />
      </label>
      <label className="text-[11px] tracking-widest text-game-textDim">
        VIEWPORT
        <select
          className="block mt-1 bg-game-bg border border-game-border px-2 py-1 text-game-text focus:outline-none focus:border-game-primary"
          value={`${document.viewport.width}x${document.viewport.height}`}
          onChange={event => {
            const preset = PRESETS.find(candidate => `${candidate.width}x${candidate.height}` === event.target.value);
            if (preset) onPreset(preset);
          }}
        >
          {PRESETS.map(preset => <option key={preset.name} value={`${preset.width}x${preset.height}`}>{preset.name}</option>)}
        </select>
      </label>
      <label className="text-[11px] tracking-widest text-game-textDim">
        ZOOM
        <input
          className="block mt-2 w-32 accent-game-primary"
          type="range"
          min="0.35"
          max="1.25"
          step="0.01"
          value={zoom}
          onChange={event => onZoom(Number(event.target.value))}
        />
      </label>
      <div className="ml-auto flex items-center gap-2">
        <ToolbarButton onClick={onImport}>IMPORT</ToolbarButton>
        <ToolbarButton onClick={onDownloadJson}>JSON</ToolbarButton>
        <ToolbarButton onClick={onDownloadClay}>CLAY</ToolbarButton>
        <ToolbarButton onClick={onReset}>RESET</ToolbarButton>
      </div>
    </header>
  );
}

function Palette({ onAdd }: { onAdd: (kind: UiNodeKind) => void }) {
  return (
    <div className="border-b border-game-border p-4">
      <div className="text-xs tracking-widest text-game-primary mb-3">PALETTE</div>
      <div className="grid grid-cols-2 gap-2">
        {PALETTE_NODE_KINDS.map(kind => (
          <button
            key={kind}
            draggable
            onDragStart={event => event.dataTransfer.setData('application/silencer-ui-kind', kind)}
            onClick={() => onAdd(kind)}
            className="border border-game-border bg-game-bg px-2 py-2 text-[11px] tracking-widest text-game-textDim hover:border-game-primary hover:text-game-text"
          >
            {KIND_LABELS[kind]}
          </button>
        ))}
      </div>
    </div>
  );
}

function Hierarchy({ root, selectedId, onSelect }: { root: UiNode; selectedId: string; onSelect: (id: string) => void }) {
  return (
    <div className="min-h-0 flex-1 overflow-auto p-4">
      <div className="text-xs tracking-widest text-game-primary mb-3">HIERARCHY</div>
      <HierarchyNode node={root} depth={0} selectedId={selectedId} onSelect={onSelect} />
    </div>
  );
}

function HierarchyNode({ node, depth, selectedId, onSelect }: { node: UiNode; depth: number; selectedId: string; onSelect: (id: string) => void }) {
  const selected = node.id === selectedId;
  return (
    <div>
      <button
        onClick={() => onSelect(node.id)}
        className={`w-full text-left py-1.5 pr-2 text-[12px] border-l transition-colors ${
          selected
            ? 'border-game-primary bg-game-dark/60 text-game-text'
            : 'border-transparent text-game-textDim hover:text-game-text hover:bg-game-bgHover'
        }`}
        style={{ paddingLeft: 8 + depth * 14 }}
      >
        <span className="text-game-muted">{KIND_LABELS[node.kind]}</span> {node.name}
      </button>
      {(node.children ?? []).map(child => (
        <HierarchyNode key={child.id} node={child} depth={depth + 1} selectedId={selectedId} onSelect={onSelect} />
      ))}
    </div>
  );
}

interface CanvasProps {
  document: UiDocument;
  selectedId: string;
  zoom: number;
  clientPreview: ClientPreviewState;
  onSelect: (id: string) => void;
  onDropNode: (targetId: string, kind: UiNodeKind) => void;
}

function Canvas({ document, selectedId, zoom, clientPreview, onSelect, onDropNode }: CanvasProps) {
  const live = clientPreview.status === 'live' && clientPreview.screenshot;
  return (
    <div className="min-h-0 flex-1 overflow-auto p-6">
      <div
        className="relative mx-auto shadow-[0_0_0_1px_rgba(86,94,111,0.8),0_24px_80px_rgba(0,0,0,0.65)]"
        style={{
          width: Math.round(document.viewport.width * zoom),
          height: Math.round(document.viewport.height * zoom),
        }}
      >
        <div
          className="origin-top-left relative"
          style={{
            width: document.viewport.width,
            height: document.viewport.height,
            transform: `scale(${zoom})`,
          }}
        >
          {live ? (
            <>
              <img
                alt=""
                src={clientPreview.screenshot}
                className="absolute inset-0 h-full w-full select-none"
                draggable={false}
              />
              <ClientPreviewOverlay
                elements={clientPreview.elements}
                document={document}
                selectedId={selectedId}
                onSelect={onSelect}
                onDropNode={onDropNode}
              />
            </>
          ) : (
            <PreviewNode
              node={document.root}
              selectedId={selectedId}
              rootViewport={document.viewport}
              onSelect={onSelect}
              onDropNode={onDropNode}
            />
          )}
          <div className="absolute left-2 top-2 border border-game-border bg-game-bg/85 px-2 py-1 text-[10px] tracking-widest text-game-textDim">
            {clientPreview.status === 'live' ? 'CLIENT LIVE' : clientPreview.status === 'syncing' ? 'SYNCING CLIENT' : 'BROWSER FALLBACK'}
          </div>
        </div>
      </div>
      {clientPreview.status === 'offline' && clientPreview.error && (
        <div className="mx-auto mt-3 max-w-[720px] border border-game-danger/60 bg-game-bgCard px-3 py-2 text-[11px] tracking-wider text-game-danger">
          {clientPreview.error}
        </div>
      )}
    </div>
  );
}

function ClientPreviewOverlay({ elements, document, selectedId, onSelect, onDropNode }: {
  elements: ClientPreviewElement[];
  document: UiDocument;
  selectedId: string;
  onSelect: (id: string) => void;
  onDropNode: (targetId: string, kind: UiNodeKind) => void;
}) {
  return (
    <div className="absolute inset-0">
      {elements.filter(element => element.id).map(element => {
        const id = element.id!;
        const node = findNode(document.root, id);
        const selected = id === selectedId;
        return (
          <div
            key={`${id}-${element.kind ?? 'element'}`}
            className="absolute"
            title={element.label ?? id}
            onClick={event => {
              event.stopPropagation();
              onSelect(id);
            }}
            onDragOver={event => {
              if (node && canHaveChildren(node.kind)) event.preventDefault();
            }}
            onDrop={event => {
              if (!node || !canHaveChildren(node.kind)) return;
              const kind = event.dataTransfer.getData('application/silencer-ui-kind') as UiNodeKind;
              if (!kind || !PALETTE_NODE_KINDS.includes(kind)) return;
              event.preventDefault();
              event.stopPropagation();
              onDropNode(id, kind);
            }}
            style={{
              left: element.x,
              top: element.y,
              width: element.w,
              height: element.h,
              outline: selected ? '2px solid #f59e0b' : '1px solid rgba(245, 158, 11, 0.22)',
              outlineOffset: selected ? 2 : 0,
            }}
          />
        );
      })}
    </div>
  );
}

function PreviewNode({ node, selectedId, rootViewport, onSelect, onDropNode }: {
  node: UiNode;
  selectedId: string;
  rootViewport: UiDocument['viewport'];
  onSelect: (id: string) => void;
  onDropNode: (targetId: string, kind: UiNodeKind) => void;
}) {
  const isSelected = node.id === selectedId;
  const allowsDrop = canHaveChildren(node.kind);
  const style = nodeToCss(node, rootViewport);

  function handleDrop(event: DragEvent<HTMLDivElement>) {
    const kind = event.dataTransfer.getData('application/silencer-ui-kind') as UiNodeKind;
    if (!kind || !PALETTE_NODE_KINDS.includes(kind)) return;
    event.preventDefault();
    event.stopPropagation();
    onDropNode(node.id, kind);
  }

  return (
    <div
      data-node-id={node.id}
      onClick={event => {
        event.stopPropagation();
        onSelect(node.id);
      }}
      onDragOver={event => {
        if (allowsDrop) event.preventDefault();
      }}
      onDrop={handleDrop}
      style={{
        ...style,
        outline: isSelected ? '2px solid #f59e0b' : undefined,
        outlineOffset: isSelected ? 2 : undefined,
        position: 'relative',
      }}
    >
      {renderLeaf(node)}
      {(node.children ?? []).map(child => (
        <PreviewNode
          key={child.id}
          node={child}
          selectedId={selectedId}
          rootViewport={rootViewport}
          onSelect={onSelect}
          onDropNode={onDropNode}
        />
      ))}
    </div>
  );
}

function renderLeaf(node: UiNode) {
  if (node.kind === 'text') return <span>{node.text}</span>;
  if (node.kind === 'button') {
    return (
      <button type="button" tabIndex={-1} className="w-full h-full" style={{ color: 'inherit' }}>
        {node.text}
      </button>
    );
  }
  if (node.kind === 'input') {
    return <div className="min-w-[140px] text-game-textDim">{node.placeholder}</div>;
  }
  return null;
}

interface InspectorProps {
  node: UiNode;
  parent: UiNode | null;
  isRoot: boolean;
  onPatch: (patch: Partial<UiNode>) => void;
  onStyle: (style: Partial<UiStyle>) => void;
  onDelete: () => void;
  onDuplicate: () => void;
  onAddChild: (kind: UiNodeKind) => void;
}

function Inspector({ node, parent, isRoot, onPatch, onStyle, onDelete, onDuplicate, onAddChild }: InspectorProps) {
  const supportsChildren = canHaveChildren(node.kind);
  return (
    <aside className="min-h-0 overflow-auto border-l border-game-border bg-game-bgCard/95">
      <div className="p-4 border-b border-game-border">
        <div className="text-xs tracking-widest text-game-primary mb-3">INSPECTOR</div>
        <div className="text-[11px] tracking-widest text-game-textDim">{KIND_LABELS[node.kind]} {parent ? `/ ${parent.name}` : ''}</div>
      </div>

      <div className="p-4 space-y-5">
        <Field label="NAME">
          <TextInput value={node.name} onChange={value => onPatch({ name: value })} />
        </Field>
        <Field label="STABLE ID">
          <TextInput value={node.id} onChange={value => onPatch({ id: slugify(value) })} />
        </Field>

        {(node.kind === 'text' || node.kind === 'button') && (
          <Field label="TEXT">
            <TextInput value={node.text ?? ''} onChange={value => onPatch({ text: value })} />
          </Field>
        )}
        {node.kind === 'button' && (
          <Field label="ACTION">
            <TextInput value={node.action ?? ''} onChange={value => onPatch({ action: slugify(value) })} />
          </Field>
        )}
        {node.kind === 'input' && (
          <Field label="PLACEHOLDER">
            <TextInput value={node.placeholder ?? ''} onChange={value => onPatch({ placeholder: value })} />
          </Field>
        )}

        {supportsChildren && (
          <div className="grid grid-cols-2 gap-2">
            {PALETTE_NODE_KINDS.map(kind => (
              <button
                key={kind}
                onClick={() => onAddChild(kind)}
                className="border border-game-border px-2 py-1.5 text-[11px] tracking-widest text-game-textDim hover:text-game-text hover:border-game-primary"
              >
                + {KIND_LABELS[kind]}
              </button>
            ))}
          </div>
        )}

        <StyleInspector node={node} onStyle={onStyle} />

        <div className="grid grid-cols-2 gap-2 pt-2">
          <button
            disabled={isRoot}
            onClick={onDuplicate}
            className="border border-game-border px-2 py-2 text-[11px] tracking-widest text-game-textDim hover:text-game-text hover:border-game-primary disabled:opacity-35"
          >
            DUPLICATE
          </button>
          <button
            disabled={isRoot}
            onClick={onDelete}
            className="border border-game-danger px-2 py-2 text-[11px] tracking-widest text-game-danger hover:bg-game-danger/20 disabled:opacity-35"
          >
            DELETE
          </button>
        </div>
      </div>
    </aside>
  );
}

function StyleInspector({ node, onStyle }: { node: UiNode; onStyle: (style: Partial<UiStyle>) => void }) {
  const style = node.style;
  return (
    <div className="space-y-4">
      <div className="text-xs tracking-widest text-game-primary">LAYOUT</div>
      <div className="grid grid-cols-2 gap-3">
        <SizeField label="WIDTH" size={style.width} onChange={width => onStyle({ width })} />
        <SizeField label="HEIGHT" size={style.height} onChange={height => onStyle({ height })} />
      </div>

      {canHaveChildren(node.kind) && (
        <>
          <div className="grid grid-cols-2 gap-3">
            <Field label="DIRECTION">
              <Select value={style.direction ?? 'column'} options={AXES} onChange={value => onStyle({ direction: value as UiAxis })} />
            </Field>
            <Field label="GAP">
              <NumberInput value={style.gap ?? 0} min={0} max={64} onChange={gap => onStyle({ gap })} />
            </Field>
          </div>
          <div className="grid grid-cols-2 gap-3">
            <Field label="ALIGN">
              <Select value={style.align ?? 'stretch'} options={ALIGNS} onChange={value => onStyle({ align: value as UiAlign })} />
            </Field>
            <Field label="JUSTIFY">
              <Select value={style.justify ?? 'start'} options={JUSTIFIES} onChange={value => onStyle({ justify: value as UiJustify })} />
            </Field>
          </div>
        </>
      )}

      <div className="grid grid-cols-2 gap-3">
        <Field label="PADDING">
          <NumberInput value={style.padding ?? 0} min={0} max={96} onChange={padding => onStyle({ padding })} />
        </Field>
        <Field label="RADIUS">
          <NumberInput value={style.radius ?? 0} min={0} max={8} onChange={radius => onStyle({ radius })} />
        </Field>
      </div>

      <div className="text-xs tracking-widest text-game-primary">STYLE</div>
      <div className="grid grid-cols-2 gap-3">
        <Field label="BACKGROUND">
          <ColorInput value={style.background ?? '#000000'} onChange={background => onStyle({ background })} />
        </Field>
        <Field label="BORDER">
          <ColorInput value={style.border ?? '#000000'} onChange={border => onStyle({ border })} />
        </Field>
        <Field label="TEXT">
          <ColorInput value={style.textColor ?? '#d1fad7'} onChange={textColor => onStyle({ textColor })} />
        </Field>
        <Field label="FONT">
          <Select value={style.font ?? 'ui'} options={FONTS} onChange={value => onStyle({ font: value as UiFont })} />
        </Field>
      </div>
      <div className="grid grid-cols-3 gap-3">
        <Field label="BG IDX">
          <NumberInput value={style.backgroundPalette ?? -1} min={-1} max={255} onChange={backgroundPalette => onStyle({ backgroundPalette })} />
        </Field>
        <Field label="BORDER IDX">
          <NumberInput value={style.borderPalette ?? -1} min={-1} max={255} onChange={borderPalette => onStyle({ borderPalette })} />
        </Field>
        <Field label="TEXT IDX">
          <NumberInput value={style.textPalette ?? 0} min={0} max={255} onChange={textPalette => onStyle({ textPalette })} />
        </Field>
      </div>
    </div>
  );
}

function SizeField({ label, size, onChange }: { label: string; size: UiSize; onChange: (size: UiSize) => void }) {
  return (
    <Field label={label}>
      <div className="grid grid-cols-[1fr_72px] gap-2">
        <Select
          value={size.mode}
          options={SIZE_MODES}
          onChange={value => onChange({ mode: value as UiSizeMode, value: size.value ?? 120 })}
        />
        <NumberInput
          value={size.value ?? 0}
          disabled={size.mode !== 'fixed'}
          min={0}
          max={1600}
          onChange={value => onChange({ ...size, value })}
        />
      </div>
    </Field>
  );
}

function ExportPanel({ exportMode, exportText, onMode }: { exportMode: 'json' | 'clay'; exportText: string; onMode: (mode: 'json' | 'clay') => void }) {
  return (
    <div className="h-52 border-t border-game-border bg-game-bgCard/90 flex flex-col">
      <div className="px-4 py-2 border-b border-game-border flex items-center gap-2">
        <button
          onClick={() => onMode('json')}
          className={`px-3 py-1 text-[11px] tracking-widest border ${exportMode === 'json' ? 'border-game-primary text-game-primary' : 'border-game-border text-game-textDim'}`}
        >
          JSON
        </button>
        <button
          onClick={() => onMode('clay')}
          className={`px-3 py-1 text-[11px] tracking-widest border ${exportMode === 'clay' ? 'border-game-primary text-game-primary' : 'border-game-border text-game-textDim'}`}
        >
          CLAY
        </button>
      </div>
      <textarea
        readOnly
        value={exportText}
        className="min-h-0 flex-1 resize-none bg-black/60 p-4 font-mono text-[11px] leading-5 text-game-textDim outline-none"
      />
    </div>
  );
}

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <label className="block text-[11px] tracking-widest text-game-textDim">
      <span className="mb-1 block">{label}</span>
      {children}
    </label>
  );
}

function TextInput({ value, onChange }: { value: string; onChange: (value: string) => void }) {
  return (
    <input
      value={value}
      onChange={event => onChange(event.target.value)}
      className="w-full bg-game-bg border border-game-border px-2 py-1.5 text-game-text focus:outline-none focus:border-game-primary"
    />
  );
}

function NumberInput({ value, min, max, disabled, onChange }: { value: number; min: number; max: number; disabled?: boolean; onChange: (value: number) => void }) {
  return (
    <input
      value={value}
      disabled={disabled}
      type="number"
      min={min}
      max={max}
      onChange={event => onChange(Number(event.target.value))}
      className="w-full bg-game-bg border border-game-border px-2 py-1.5 text-game-text focus:outline-none focus:border-game-primary disabled:opacity-40"
    />
  );
}

function Select({ value, options, onChange }: { value: string; options: string[]; onChange: (value: string) => void }) {
  return (
    <select
      value={value}
      onChange={event => onChange(event.target.value)}
      className="w-full bg-game-bg border border-game-border px-2 py-1.5 text-game-text focus:outline-none focus:border-game-primary"
    >
      {options.map(option => <option key={option} value={option}>{option}</option>)}
    </select>
  );
}

function ColorInput({ value, onChange }: { value: string; onChange: (value: string) => void }) {
  return (
    <input
      type="color"
      value={normalizeColor(value)}
      onChange={event => onChange(event.target.value)}
      className="h-[34px] w-full bg-game-bg border border-game-border p-1"
    />
  );
}

function ToolbarButton({ children, onClick }: { children: React.ReactNode; onClick: () => void }) {
  return (
    <button
      onClick={onClick}
      className="border border-game-border px-3 py-2 text-[11px] tracking-widest text-game-textDim hover:border-game-primary hover:text-game-text"
    >
      {children}
    </button>
  );
}

function nodeToCss(node: UiNode, rootViewport: UiDocument['viewport']): CSSProperties {
  const style = node.style;
  const css: CSSProperties = {
    boxSizing: 'border-box',
    color: style.textColor,
    backgroundColor: style.background,
    border: style.border ? `1px solid ${style.border}` : undefined,
    borderRadius: style.radius,
    padding: style.padding,
    gap: style.gap,
    fontFamily: fontFamily(style.font),
    fontSize: fontSize(style.font),
    lineHeight: 1.2,
  };

  applySize(css, 'width', style.width);
  applySize(css, 'height', style.height);

  if (node.kind === 'screen') {
    css.width = rootViewport.width;
    css.height = rootViewport.height;
    css.overflow = 'hidden';
  }

  if (canHaveChildren(node.kind)) {
    css.display = 'flex';
    css.flexDirection = style.direction ?? (node.kind === 'row' ? 'row' : 'column');
    css.alignItems = alignToCss(style.align ?? 'stretch');
    css.justifyContent = justifyToCss(style.justify ?? 'start');
  } else if (node.kind === 'spacer') {
    css.minHeight = style.height.mode === 'fixed' ? style.height.value : 8;
  } else {
    css.display = 'inline-flex';
    css.alignItems = 'center';
    css.justifyContent = 'center';
    css.textAlign = 'center';
    css.minHeight = node.kind === 'button' || node.kind === 'input' ? 34 : undefined;
    css.whiteSpace = 'pre-wrap';
  }

  return css;
}

function applySize(css: CSSProperties, property: 'width' | 'height', size: UiSize) {
  if (size.mode === 'fixed') {
    css[property] = size.value ?? 0;
    return;
  }
  if (size.mode === 'grow') {
    css.flexGrow = 1;
    if (property === 'width') css.alignSelf = 'stretch';
    return;
  }
  css[property] = 'fit-content';
}

function alignToCss(align: UiAlign): CSSProperties['alignItems'] {
  if (align === 'start') return 'flex-start';
  if (align === 'end') return 'flex-end';
  return align;
}

function justifyToCss(justify: UiJustify): CSSProperties['justifyContent'] {
  if (justify === 'start') return 'flex-start';
  if (justify === 'end') return 'flex-end';
  if (justify === 'between') return 'space-between';
  return justify;
}

function fontFamily(font: UiFont | undefined): string {
  if (font === 'title') return '"Silencer Title", "Courier New", monospace';
  if (font === 'tiny') return '"Silencer Tiny", "Courier New", monospace';
  if (font === 'uiLarge') return '"Silencer UI Large", "Courier New", monospace';
  return '"Silencer UI", "Courier New", monospace';
}

function fontSize(font: UiFont | undefined): number {
  if (font === 'title') return 64;
  if (font === 'tiny') return 9;
  if (font === 'uiLarge') return 13;
  return 11;
}

function slugify(value: string): string {
  return value.trim().toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/(^-|-$)/g, '') || 'unnamed';
}

function normalizeColor(value: string): string {
  return /^#[0-9a-f]{6}$/i.test(value) ? value : '#000000';
}

function inspectElements(inspect: unknown): ClientPreviewElement[] {
  if (!inspect || typeof inspect !== 'object') return [];
  const source = inspect as { elements?: unknown; widgets?: unknown };
  const raw = [
    ...(Array.isArray(source.elements) ? source.elements : []),
    ...(Array.isArray(source.widgets) ? source.widgets : []),
  ];
  const elements: ClientPreviewElement[] = [];
  for (const item of raw) {
    if (!item || typeof item !== 'object') continue;
    const candidate = item as Record<string, unknown>;
    const x = numeric(candidate.x);
    const y = numeric(candidate.y);
    const w = numeric(candidate.w);
    const h = numeric(candidate.h);
    if (x === null || y === null || w === null || h === null || w <= 0 || h <= 0) continue;
    elements.push({
      id: typeof candidate.id === 'string' ? candidate.id : undefined,
      label: typeof candidate.label === 'string' ? candidate.label : undefined,
      kind: typeof candidate.kind === 'string' ? candidate.kind : undefined,
      x,
      y,
      w,
      h,
    });
  }
  return elements;
}

function numeric(value: unknown): number | null {
  return typeof value === 'number' && Number.isFinite(value) ? value : null;
}

function downloadText(filename: string, text: string, mime: string) {
  const blob = new Blob([text], { type: mime });
  const url = URL.createObjectURL(blob);
  const link = window.document.createElement('a');
  link.href = url;
  link.download = filename;
  link.click();
  URL.revokeObjectURL(url);
}
