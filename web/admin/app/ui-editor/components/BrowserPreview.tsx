import { type DragEvent } from "react";
import {
  PALETTE_NODE_KINDS,
  canHaveChildren,
  type UiDocument,
  type UiMovePlacement,
  type UiNode,
  type UiNodeKind,
} from "../../../lib/ui-layout";
import {
  UI_NODE_DRAG_TYPE,
  UI_PALETTE_DRAG_TYPE,
  dropAxisFromDirection,
  resolveEventMovePlacement,
  type DropAxis,
} from "../ui-editor-dnd";

export function PreviewNode({
  node,
  selectedId,
  rootViewport,
  dropAxis = "vertical",
  onSelect,
  onDropNode,
  onMoveNode,
}: {
  node: UiNode;
  selectedId: string;
  rootViewport: UiDocument["viewport"];
  dropAxis?: DropAxis;
  onSelect: (id: string) => void;
  onDropNode: (targetId: string, kind: UiNodeKind) => void;
  onMoveNode: (nodeId: string, targetId: string, placement: UiMovePlacement) => void;
}) {
  const isSelected = node.id === selectedId;
  const root = node.kind === "screen";
  const childAxis = dropAxisFromDirection(node.style.direction);

  function handleDrop(event: DragEvent<HTMLDivElement>) {
    const nodeId = event.dataTransfer.getData(UI_NODE_DRAG_TYPE);
    if (nodeId) {
      event.preventDefault();
      event.stopPropagation();
      onMoveNode(nodeId, node.id, resolveEventMovePlacement(event, node, dropAxis));
      return;
    }
    const kind = event.dataTransfer.getData(UI_PALETTE_DRAG_TYPE) as UiNodeKind;
    if (!kind || !PALETTE_NODE_KINDS.includes(kind)) return;
    event.preventDefault();
    event.stopPropagation();
    onDropNode(node.id, kind);
  }

  return (
    <div
      data-node-id={node.id}
      draggable={!root}
      onDragStart={(event) => {
        if (root) return;
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData(UI_NODE_DRAG_TYPE, node.id);
      }}
      onClick={(event) => {
        event.stopPropagation();
        onSelect(node.id);
      }}
      onDragOver={(event) => {
        event.preventDefault();
      }}
      onDrop={handleDrop}
      className={
        root
          ? "absolute inset-0 overflow-auto bg-game-bg p-4"
          : "relative mt-2 border bg-game-bgCard/80 p-2"
      }
      style={{
        width: root ? rootViewport.width : undefined,
        minHeight: root ? rootViewport.height : undefined,
        borderColor: isSelected ? "#f59e0b" : "rgba(86, 94, 111, 0.8)",
        outline: isSelected ? "2px solid #f59e0b" : undefined,
        outlineOffset: isSelected ? 2 : undefined,
      }}
    >
      <div className="flex min-h-7 items-center gap-2 text-[11px] tracking-widest">
        <span className="border border-game-border px-1.5 py-0.5 text-game-primary">
          {node.kind.toUpperCase()}
        </span>
        <span className="truncate text-game-text">{node.name}</span>
        <span className="truncate text-game-textDim">{node.id}</span>
        <span className="ml-auto truncate text-game-textDim">{tokenSummary(node)}</span>
      </div>
      {canHaveChildren(node.kind) && (
        <div className="ml-4 border-l border-game-border/70 pl-3">
          {(node.children ?? []).map((child) => (
            <PreviewNode
              key={child.id}
              node={child}
              selectedId={selectedId}
              rootViewport={rootViewport}
              dropAxis={childAxis}
              onSelect={onSelect}
              onDropNode={onDropNode}
              onMoveNode={onMoveNode}
            />
          ))}
        </div>
      )}
    </div>
  );
}

function tokenSummary(node: UiNode): string {
  if (node.kind === "button") return node.action ?? "";
  if (node.kind === "component") return node.component ?? "";
  if (node.textBinding) return node.textBinding;
  if (node.text) return node.text;
  return "";
}
