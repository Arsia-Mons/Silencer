import { type CSSProperties, type DragEvent } from "react";
import {
  PALETTE_NODE_KINDS,
  UI_ALIGNS,
  UI_AXES,
  UI_FONTS,
  UI_IMAGE_MODES,
  UI_JUSTIFIES,
  UI_NODE_KINDS,
  UI_SIZE_MODES,
  canHaveChildren,
  type UiAlign,
  type UiDocument,
  type UiFont,
  type UiJustify,
  type UiMovePlacement,
  type UiNode,
  type UiNodeKind,
  type UiSize,
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
  const style = nodeToCss(node, rootViewport);

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
      draggable={node.kind !== "screen"}
      onDragStart={(event) => {
        if (node.kind === "screen") return;
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
      style={{
        ...style,
        outline: isSelected ? "2px solid #f59e0b" : undefined,
        outlineOffset: isSelected ? 2 : undefined,
        position: node.floating ? "absolute" : "relative",
      }}
    >
      {renderLeaf(node)}
      {(node.children ?? []).map((child) => (
        <PreviewNode
          key={child.id}
          node={child}
          selectedId={selectedId}
          rootViewport={rootViewport}
          dropAxis={dropAxisFromDirection(node.style.direction)}
          onSelect={onSelect}
          onDropNode={onDropNode}
          onMoveNode={onMoveNode}
        />
      ))}
    </div>
  );
}

function renderLeaf(node: UiNode) {
  if (node.kind === "text") return <span>{node.text}</span>;
  if (node.kind === "button") {
    return (
      <button type="button" tabIndex={-1} className="w-full h-full" style={{ color: "inherit" }}>
        {node.text}
      </button>
    );
  }
  if (node.kind === "component") {
    return <div className="text-[10px] tracking-widest text-game-textDim">{node.component}</div>;
  }
  return null;
}

function nodeToCss(node: UiNode, rootViewport: UiDocument["viewport"]): CSSProperties {
  const style = node.style;
  const css: CSSProperties = {
    boxSizing: "border-box",
    color: paletteColor(style.textPalette, "text"),
    backgroundColor: paletteColor(style.backgroundPalette, "surface"),
    border:
      style.borderPalette !== undefined && style.borderPalette >= 0
        ? `1px solid ${paletteColor(style.borderPalette, "surface")}`
        : undefined,
    borderRadius: style.radius,
    padding: style.padding,
    gap: style.gap,
    fontFamily: fontFamily(style.font),
    fontSize: fontSize(style.font),
    lineHeight: 1.2,
  };
  if (node.image) {
    css.backgroundImage = `linear-gradient(135deg, rgba(84,156,104,0.35), rgba(8,84,0,0.35))`;
    css.backgroundSize = node.image.mode === UI_IMAGE_MODES[0] ? undefined : "cover";
  }

  applySize(css, "width", style.width);
  applySize(css, "height", style.height);

  if (node.kind === "screen") {
    css.width = rootViewport.width;
    css.height = rootViewport.height;
    css.overflow = "hidden";
  }

  if (canHaveChildren(node.kind)) {
    css.display = "flex";
    css.flexDirection =
      style.direction ?? (node.kind === UI_NODE_KINDS[3] ? UI_AXES[1] : UI_AXES[0]);
    css.alignItems = alignToCss(style.align ?? UI_ALIGNS[0]);
    css.justifyContent = justifyToCss(style.justify ?? UI_JUSTIFIES[0]);
  } else if (node.kind === "spacer") {
    css.minHeight = style.height.mode === "fixed" ? style.height.value : 8;
  } else {
    css.display = "inline-flex";
    css.alignItems = "center";
    css.justifyContent = "center";
    css.textAlign = "center";
    css.minHeight = node.kind === "button" ? 34 : undefined;
    css.whiteSpace = "pre-wrap";
  }
  if (node.floating) {
    css.left = node.floating.offsetX ?? 0;
    css.top = node.floating.offsetY ?? 0;
    css.zIndex = node.floating.zIndex ?? 1;
  }

  return css;
}

function applySize(css: CSSProperties, property: "width" | "height", size: UiSize) {
  const maxProperty = property === "width" ? "maxWidth" : "maxHeight";
  const minProperty = property === "width" ? "minWidth" : "minHeight";
  if (size.max !== undefined) css[maxProperty] = size.max;
  if (size.min !== undefined) css[minProperty] = size.min;
  if (size.mode === UI_SIZE_MODES[2]) {
    css[property] = size.value ?? 0;
    return;
  }
  if (size.mode === UI_SIZE_MODES[1]) {
    css.flexGrow = 1;
    if (property === "width") css.alignSelf = "stretch";
    return;
  }
  css[property] = "fit-content";
}

function alignToCss(align: UiAlign): CSSProperties["alignItems"] {
  if (align === UI_ALIGNS[0]) return "flex-start";
  if (align === UI_ALIGNS[2]) return "flex-end";
  return align;
}

function justifyToCss(justify: UiJustify): CSSProperties["justifyContent"] {
  if (justify === UI_JUSTIFIES[0]) return "flex-start";
  if (justify === UI_JUSTIFIES[2]) return "flex-end";
  return justify;
}

function fontFamily(font: UiFont | undefined): string {
  if (font === UI_FONTS[2]) return '"Silencer Title", "Courier New", monospace';
  if (font === UI_FONTS[3]) return '"Silencer Tiny", "Courier New", monospace';
  if (font === UI_FONTS[4]) return '"Silencer UI", "Courier New", monospace';
  if (font === UI_FONTS[1]) return '"Silencer UI Large", "Courier New", monospace';
  return '"Silencer UI", "Courier New", monospace';
}

function fontSize(font: UiFont | undefined): number {
  if (font === UI_FONTS[2]) return 64;
  if (font === UI_FONTS[3]) return 9;
  if (font === UI_FONTS[4]) return 11;
  if (font === UI_FONTS[1]) return 13;
  return 11;
}

function paletteColor(index: number | undefined, role: "surface" | "text"): string | undefined {
  if (index === undefined || index < 0) return undefined;
  if (role === "text" && index === 0) return undefined;
  const knownPalette: Record<number, string> = {
    0: "#000000",
    74: "#a8541c",
    112: "#549c68",
    216: "#085400",
  };
  return knownPalette[index] ?? `hsl(${(index * 47) % 360} 58% 42%)`;
}
