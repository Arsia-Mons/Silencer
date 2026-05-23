"use client";
import { type ChangeEvent, useEffect, useMemo, useRef, useState } from "react";
import Sidebar from "../../components/Sidebar";
import { getUiEditorDocument, listUiEditorDocuments, saveUiEditorDocument } from "../../lib/api";
import { useAuth } from "../../lib/auth";
import { useWsConnected } from "../../lib/socket";
import {
  canHaveChildren,
  createDefaultUiDocument,
  createNode,
  duplicateNode,
  findNode,
  findParent,
  insertAfter,
  insertChild,
  moveNode,
  normalizeUiSurface,
  removeNode,
  updateNode,
  validateUiDocument,
  type UiDocument,
  type UiDocumentReference,
  type UiMovePlacement,
  type UiNode,
  type UiNodeKind,
  type UiStyle,
} from "../../lib/ui-layout";
import { Canvas } from "./components/Canvas";
import { EditorTopBar } from "./components/EditorTopBar";
import { ExportPanel } from "./components/ExportPanel";
import { Hierarchy } from "./components/Hierarchy";
import { Inspector } from "./components/Inspector";
import { Palette } from "./components/Palette";
import { PRESETS, STORAGE_KEY } from "./ui-editor-constants";
import { downloadText } from "./ui-editor-utils";
import { useClientPreview } from "./useClientPreview";

export default function UiEditorPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const importInputRef = useRef<HTMLInputElement | null>(null);
  const [document, setDocument] = useState<UiDocument>(() => createDefaultUiDocument());
  const [documents, setDocuments] = useState<UiDocumentReference[]>([]);
  const [currentRevision, setCurrentRevision] = useState<string | null>(null);
  const [dirty, setDirty] = useState(false);
  const [selectedId, setSelectedId] = useState("MainMenuActionGroup");
  const [hydrated, setHydrated] = useState(false);
  const [zoom, setZoom] = useState(0.72);
  const [status, setStatus] = useState("READY");
  const clientPreview = useClientPreview(document, hydrated);

  useEffect(() => {
    let cancelled = false;

    async function loadInitialDocument() {
      let draft = readPreferredLocalDraft([]);
      try {
        const loadedDocuments = await listUiEditorDocuments();
        if (cancelled) return;
        setDocuments(loadedDocuments);
        draft = readPreferredLocalDraft(loadedDocuments);
        if (draft) {
          setDocument(draft.document);
          setSelectedId(draft.document.root.id);
          setCurrentRevision(draft.baseRevision);
          setDirty(true);
          setStatus(`LOCAL DRAFT ${draft.document.surface}`);
          return;
        }
        const preferred =
          loadedDocuments.find((candidate) => candidate.surface === "main-menu") ??
          loadedDocuments[0];
        if (preferred) {
          const loaded = await getUiEditorDocument(preferred.surface);
          const parsed = validateUiDocument(loaded.document);
          if (cancelled) return;
          setDocument(parsed);
          setSelectedId(parsed.root.id);
          setCurrentRevision(loaded.reference.revision);
          setDirty(false);
          setStatus(`LOADED ${preferred.surface}`);
          return;
        }
        setStatus("NO SAVED DOCUMENTS");
      } catch (error) {
        if (cancelled) return;
        if (draft) {
          setDocument(draft.document);
          setSelectedId(draft.document.root.id);
          setCurrentRevision(draft.baseRevision);
          setDirty(true);
          setStatus(error instanceof Error ? `LOCAL DRAFT: ${error.message}` : "LOCAL DRAFT");
        } else {
          setStatus(error instanceof Error ? error.message : "LOAD FAILED");
        }
      } finally {
        if (!cancelled) setHydrated(true);
      }
    }

    loadInitialDocument();
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    if (!hydrated) return;
    try {
      if (dirty) {
        writeLocalDraft(document, currentRevision);
      } else {
        clearLocalDraft(document.surface);
      }
    } catch (error) {
      setStatus(error instanceof Error ? error.message : "FAILED TO CACHE LOCAL DOCUMENT");
    }
  }, [currentRevision, dirty, document, hydrated]);

  const selectedNode = useMemo(
    () => findNode(document.root, selectedId) ?? document.root,
    [document, selectedId],
  );
  const selectedParent = useMemo(
    () => findParent(document.root, selectedNode.id),
    [document, selectedNode.id],
  );
  const exportText = useMemo(() => JSON.stringify(document, null, 2), [document]);

  function commit(next: UiDocument, nextSelectedId = selectedNode.id) {
    if (normalizeUiSurface(next.surface) !== normalizeUiSurface(document.surface)) {
      setCurrentRevision(null);
    }
    setDocument(next);
    setSelectedId(nextSelectedId);
    setDirty(true);
    setStatus("DIRTY");
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
    commit(
      updateNode(document, selectedNode.id, (node) => ({ ...node, ...patch })),
      nextId,
    );
  }

  function updateSelectedStyle(style: Partial<UiStyle>) {
    updateSelectedNode((node) => ({ ...node, style: { ...node.style, ...style } }));
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
      if (normalizeUiSurface(parsed.surface) !== normalizeUiSurface(document.surface)) {
        clearLocalDraft(document.surface);
      }
      setDocument(parsed);
      setSelectedId(parsed.root.id);
      setCurrentRevision(null);
      setDirty(true);
      setStatus("IMPORTED");
    } catch (error) {
      setStatus(error instanceof Error ? error.message : "IMPORT FAILED");
    } finally {
      event.target.value = "";
    }
  }

  async function loadDocument(surface: string) {
    try {
      setStatus(`LOADING ${surface}`);
      if (dirty) writeLocalDraft(document, currentRevision);

      const draft = readLocalDraft(surface);
      if (draft) {
        setDocument(draft.document);
        setSelectedId(draft.document.root.id);
        setCurrentRevision(draft.baseRevision);
        setDirty(true);
        setStatus(`LOCAL DRAFT ${draft.document.surface}`);
        return;
      }

      const loaded = await getUiEditorDocument(surface);
      const parsed = validateUiDocument(loaded.document);
      setDocument(parsed);
      setSelectedId(parsed.root.id);
      setCurrentRevision(loaded.reference.revision);
      setDirty(false);
      setStatus(`LOADED ${parsed.surface}`);
    } catch (error) {
      setStatus(error instanceof Error ? error.message : `LOAD FAILED: ${surface}`);
    }
  }

  async function saveDocument() {
    try {
      const next = validateUiDocument({
        ...document,
        surface: normalizeUiSurface(document.surface),
      });
      setStatus(`SAVING ${next.surface}`);
      const saved = await saveUiEditorDocument(next, currentRevision);
      const parsed = validateUiDocument(saved.document);
      setDocument(parsed);
      setSelectedId(selectedNode.id);
      setCurrentRevision(saved.reference.revision);
      setDirty(false);
      setDocuments((current) => upsertDocumentReference(current, saved.reference));
      setStatus(`SAVED ${parsed.surface}`);
    } catch (error) {
      setStatus(error instanceof Error ? error.message : "SAVE FAILED");
    }
  }

  function downloadDocument() {
    downloadText(
      `${document.surface}.silencer-ui.json`,
      JSON.stringify(document, null, 2),
      "application/json",
    );
    setStatus("EXPORTED JSON");
  }

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 min-w-0 flex flex-col">
        <EditorTopBar
          document={document}
          documents={documents}
          status={status}
          zoom={zoom}
          onZoom={setZoom}
          onDocumentChange={(next) => {
            if (normalizeUiSurface(next.surface) !== normalizeUiSurface(document.surface)) {
              clearLocalDraft(document.surface);
            }
            setDocument(next);
            if (normalizeUiSurface(next.surface) !== normalizeUiSurface(document.surface)) {
              setCurrentRevision(null);
            }
            setDirty(true);
            setStatus("DIRTY");
          }}
          onLoadDocument={loadDocument}
          onPreset={(preset) => {
            setDocument(applyViewportPreset(document, preset));
            setZoom(preset.zoom);
            setDirty(true);
            setStatus("DIRTY");
          }}
          onImport={() => importInputRef.current?.click()}
          onSave={saveDocument}
          onDownloadJson={downloadDocument}
          onReset={() => {
            const next = validateUiDocument({
              ...createDefaultUiDocument(),
              surface: normalizeUiSurface(document.surface),
            });
            setDocument(next);
            setSelectedId(next.root.id);
            setCurrentRevision(currentRevision);
            setDirty(true);
            setStatus("RESET");
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
            <ExportPanel exportText={exportText} />
          </section>

          <Inspector
            node={selectedNode}
            parent={selectedParent}
            isRoot={selectedNode.id === document.root.id}
            onPatch={patchSelectedNode}
            onStyle={updateSelectedStyle}
            onDelete={deleteSelectedNode}
            onDuplicate={duplicateSelectedNode}
            onAddChild={(kind) => addNode(kind, selectedNode.id)}
          />
        </div>
        <input
          ref={importInputRef}
          className="hidden"
          type="file"
          accept=".json,.silencer-ui.json,application/json"
          onChange={importDocument}
        />
      </main>
    </div>
  );
}

function upsertDocumentReference(
  documents: UiDocumentReference[],
  reference: UiDocumentReference,
): UiDocumentReference[] {
  const others = documents.filter((candidate) => candidate.surface !== reference.surface);
  return [...others, reference].sort((a, b) => a.surface.localeCompare(b.surface));
}

interface LocalDraft {
  document: UiDocument;
  baseRevision: string | null;
}

const DRAFT_KEY_PREFIX = `${STORAGE_KEY}:`;

function draftStorageKey(surface: string): string {
  return `${DRAFT_KEY_PREFIX}${normalizeUiSurface(surface)}`;
}

function readLocalDraft(surface: string): LocalDraft | null {
  return readLocalDraftFromKey(draftStorageKey(surface));
}

function readPreferredLocalDraft(documents: UiDocumentReference[]): LocalDraft | null {
  const checkedKeys = new Set<string>();
  for (const surface of ["main-menu", ...documents.map((candidate) => candidate.surface)]) {
    const key = draftStorageKey(surface);
    checkedKeys.add(key);
    const draft = readLocalDraftFromKey(key);
    if (draft) return draft;
  }

  checkedKeys.add(STORAGE_KEY);
  const legacyDraft = readLocalDraftFromKey(STORAGE_KEY);
  if (legacyDraft) return legacyDraft;

  for (let index = 0; index < localStorage.length; index += 1) {
    const key = localStorage.key(index);
    if (!key || checkedKeys.has(key) || !key.startsWith(DRAFT_KEY_PREFIX)) continue;
    const draft = readLocalDraftFromKey(key);
    if (draft) return draft;
  }
  return null;
}

function readLocalDraftFromKey(key: string): LocalDraft | null {
  try {
    const raw = localStorage.getItem(key);
    if (!raw) return null;
    const parsed = JSON.parse(raw);
    if (parsed?.document) {
      return {
        document: validateUiDocument(parsed.document),
        baseRevision: typeof parsed.baseRevision === "string" ? parsed.baseRevision : null,
      };
    }
    return {
      document: validateUiDocument(parsed),
      baseRevision: null,
    };
  } catch {
    return null;
  }
}

function writeLocalDraft(document: UiDocument, baseRevision: string | null) {
  localStorage.setItem(
    draftStorageKey(document.surface),
    JSON.stringify({ document, baseRevision }),
  );
}

function clearLocalDraft(surface: string) {
  const normalizedSurface = normalizeUiSurface(surface);
  localStorage.removeItem(draftStorageKey(normalizedSurface));
  const legacyDraft = readLocalDraftFromKey(STORAGE_KEY);
  if (legacyDraft && normalizeUiSurface(legacyDraft.document.surface) === normalizedSurface) {
    localStorage.removeItem(STORAGE_KEY);
  }
}

function applyViewportPreset(document: UiDocument, preset: (typeof PRESETS)[number]): UiDocument {
  return {
    ...document,
    viewport: { width: preset.width, height: preset.height },
  };
}
