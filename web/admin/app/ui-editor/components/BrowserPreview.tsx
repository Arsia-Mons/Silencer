import { type CSSProperties, type DragEvent } from 'react';
import {
  PALETTE_NODE_KINDS,
  canHaveChildren,
  type UiAlign,
  type UiDocument,
  type UiFont,
  type UiJustify,
  type UiNode,
  type UiNodeKind,
  type UiSize,
} from '../../../lib/ui-layout';

export function PreviewNode({ node, selectedId, rootViewport, onSelect, onDropNode }: {
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
    css.alignItems = alignToCss(style.align ?? 'start');
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
