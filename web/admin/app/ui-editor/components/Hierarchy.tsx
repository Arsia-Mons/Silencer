import { KIND_LABELS } from '../ui-editor-constants';
import { type UiNode } from '../../../lib/ui-layout';

export function Hierarchy({ root, selectedId, onSelect, onMoveNode }: {
  root: UiNode;
  selectedId: string;
  onSelect: (id: string) => void;
  onMoveNode: (nodeId: string, targetId: string) => void;
}) {
  return (
    <div className="min-h-0 flex-1 overflow-auto p-4">
      <div className="text-xs tracking-widest text-game-primary mb-3">HIERARCHY</div>
      <HierarchyNode node={root} depth={0} selectedId={selectedId} onSelect={onSelect} onMoveNode={onMoveNode} />
    </div>
  );
}

function HierarchyNode({ node, depth, selectedId, onSelect, onMoveNode }: {
  node: UiNode;
  depth: number;
  selectedId: string;
  onSelect: (id: string) => void;
  onMoveNode: (nodeId: string, targetId: string) => void;
}) {
  const selected = node.id === selectedId;
  return (
    <div>
      <button
        draggable={depth > 0}
        onDragStart={event => {
          if (depth === 0) return;
          event.dataTransfer.effectAllowed = 'move';
          event.dataTransfer.setData('application/silencer-ui-node-id', node.id);
        }}
        onDragOver={event => event.preventDefault()}
        onDrop={event => {
          const nodeId = event.dataTransfer.getData('application/silencer-ui-node-id');
          if (!nodeId) return;
          event.preventDefault();
          event.stopPropagation();
          onMoveNode(nodeId, node.id);
        }}
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
        <HierarchyNode
          key={child.id}
          node={child}
          depth={depth + 1}
          selectedId={selectedId}
          onSelect={onSelect}
          onMoveNode={onMoveNode}
        />
      ))}
    </div>
  );
}
