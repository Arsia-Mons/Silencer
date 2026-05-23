import {
  PALETTE_NODE_KINDS,
  findNode,
  findParent,
  type UiDocument,
  type UiMovePlacement,
  type UiNodeKind,
} from "../../../lib/ui-layout";
import { type ClientPreviewElement, type ClientPreviewState } from "../useClientPreview";
import {
  UI_NODE_DRAG_TYPE,
  UI_PALETTE_DRAG_TYPE,
  dropAxisFromDirection,
  resolveEventMovePlacement,
} from "../ui-editor-dnd";
import { PreviewNode } from "./BrowserPreview";

interface CanvasProps {
  document: UiDocument;
  selectedId: string;
  zoom: number;
  clientPreview: ClientPreviewState;
  onSelect: (id: string) => void;
  onDropNode: (targetId: string, kind: UiNodeKind) => void;
  onMoveNode: (nodeId: string, targetId: string, placement: UiMovePlacement) => void;
}

export function Canvas({
  document,
  selectedId,
  zoom,
  clientPreview,
  onSelect,
  onDropNode,
  onMoveNode,
}: CanvasProps) {
  const live = clientPreview.status === "live" && clientPreview.screenshot;
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
                onMoveNode={onMoveNode}
              />
            </>
          ) : (
            <PreviewNode
              node={document.root}
              selectedId={selectedId}
              rootViewport={document.viewport}
              onSelect={onSelect}
              onDropNode={onDropNode}
              onMoveNode={onMoveNode}
            />
          )}
          <div className="absolute left-2 top-2 border border-game-border bg-game-bg/85 px-2 py-1 text-[10px] tracking-widest text-game-textDim">
            {clientPreview.status === "live"
              ? "CLIENT LIVE"
              : clientPreview.status === "syncing"
                ? "SYNCING CLIENT"
                : "STRUCTURE FALLBACK"}
          </div>
        </div>
      </div>
      {clientPreview.status === "offline" && clientPreview.error && (
        <div className="mx-auto mt-3 max-w-[720px] border border-game-danger/60 bg-game-bgCard px-3 py-2 text-[11px] tracking-wider text-game-danger">
          {clientPreview.error}
        </div>
      )}
    </div>
  );
}

function ClientPreviewOverlay({
  elements,
  document,
  selectedId,
  onSelect,
  onDropNode,
  onMoveNode,
}: {
  elements: ClientPreviewElement[];
  document: UiDocument;
  selectedId: string;
  onSelect: (id: string) => void;
  onDropNode: (targetId: string, kind: UiNodeKind) => void;
  onMoveNode: (nodeId: string, targetId: string, placement: UiMovePlacement) => void;
}) {
  return (
    <div className="absolute inset-0">
      {elements.map((element) => (
        <ClientPreviewOverlayTarget
          key={`${element.id ?? "anonymous"}-${element.source}-${element.kind ?? "element"}`}
          element={element}
          document={document}
          selectedId={selectedId}
          onSelect={onSelect}
          onDropNode={onDropNode}
          onMoveNode={onMoveNode}
        />
      ))}
    </div>
  );
}

function ClientPreviewOverlayTarget({
  element,
  document,
  selectedId,
  onSelect,
  onDropNode,
  onMoveNode,
}: {
  element: ClientPreviewElement;
  document: UiDocument;
  selectedId: string;
  onSelect: (id: string) => void;
  onDropNode: (targetId: string, kind: UiNodeKind) => void;
  onMoveNode: (nodeId: string, targetId: string, placement: UiMovePlacement) => void;
}) {
  if (!element.id) return null;
  const node = findNode(document.root, element.id);
  if (!node) return null;
  const selected = element.id === selectedId;
  return (
    <div
      className="absolute"
      draggable={element.id !== document.root.id}
      title={element.label ?? element.id}
      onDragStart={(event) => {
        if (element.id === document.root.id) return;
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData(UI_NODE_DRAG_TYPE, element.id!);
      }}
      onClick={(event) => {
        event.stopPropagation();
        onSelect(element.id!);
      }}
      onDragOver={(event) => {
        event.preventDefault();
      }}
      onDrop={(event) => {
        const movingId = event.dataTransfer.getData(UI_NODE_DRAG_TYPE);
        if (movingId) {
          event.preventDefault();
          event.stopPropagation();
          const parent = findParent(document.root, element.id!);
          onMoveNode(
            movingId,
            element.id!,
            resolveEventMovePlacement(event, node, dropAxisFromDirection(parent?.style.direction)),
          );
          return;
        }
        const kind = event.dataTransfer.getData(UI_PALETTE_DRAG_TYPE) as UiNodeKind;
        if (!kind || !PALETTE_NODE_KINDS.includes(kind)) return;
        event.preventDefault();
        event.stopPropagation();
        onDropNode(element.id!, kind);
      }}
      style={{
        left: element.x,
        top: element.y,
        width: element.w,
        height: element.h,
        outline: selected ? "2px solid #f59e0b" : "1px solid rgba(245, 158, 11, 0.22)",
        outlineOffset: selected ? 2 : 0,
      }}
    />
  );
}
