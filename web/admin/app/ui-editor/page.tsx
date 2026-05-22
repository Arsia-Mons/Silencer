'use client';
import { type ChangeEvent, useEffect, useMemo, useRef, useState } from 'react';
import Sidebar from '../../components/Sidebar';
import { useAuth } from '../../lib/auth';
import { useWsConnected } from '../../lib/socket';
import {
  canHaveChildren,
  createDefaultUiDocument,
  createNode,
  duplicateNode,
  exportClaySnippet,
  findNode,
  findParent,
  insertAfter,
  insertChild,
  moveNode,
  removeNode,
  updateNode,
  validateUiDocument,
  type UiDocument,
  type UiMovePlacement,
  type UiNode,
  type UiNodeKind,
  type UiStyle,
} from '../../lib/ui-layout';
import { Canvas } from './components/Canvas';
import { EditorTopBar } from './components/EditorTopBar';
import { ExportPanel } from './components/ExportPanel';
import { Hierarchy } from './components/Hierarchy';
import { Inspector } from './components/Inspector';
import { Palette } from './components/Palette';
import { PRESETS, STORAGE_KEY } from './ui-editor-constants';
import { downloadText } from './ui-editor-utils';
import { useClientPreview } from './useClientPreview';

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
  const clientPreview = useClientPreview(document, hydrated);

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

  function moveExistingNode(nodeId: string, targetId: string, placement: UiMovePlacement) {
    const next = moveNode(document, nodeId, { targetId, placement });
    if (next === document) return;
    commit(next, nodeId);
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
            setDocument(applyViewportPreset(document, preset));
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
            <Hierarchy
              selectedId={selectedNode.id}
              root={document.root}
              onSelect={setSelectedId}
              onMoveNode={moveExistingNode}
            />
          </section>

          <section className="min-w-0 min-h-0 flex flex-col bg-black/45">
            <Canvas
              document={document}
              selectedId={selectedNode.id}
              zoom={zoom}
              clientPreview={clientPreview}
              onSelect={setSelectedId}
              onDropNode={(targetId, kind) => addNode(kind, targetId)}
              onMoveNode={moveExistingNode}
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

function applyViewportPreset(document: UiDocument, preset: typeof PRESETS[number]): UiDocument {
  return {
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
  };
}
