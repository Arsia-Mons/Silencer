import { KIND_LABELS } from '../ui-editor-constants';
import { type UiNode } from '../../../lib/ui-layout';

export function Hierarchy({ root, selectedId, onSelect }: {
  root: UiNode;
  selectedId: string;
  onSelect: (id: string) => void;
}) {
  return (
    <div className="min-h-0 flex-1 overflow-auto p-4">
      <div className="text-xs tracking-widest text-game-primary mb-3">HIERARCHY</div>
      <HierarchyNode node={root} depth={0} selectedId={selectedId} onSelect={onSelect} />
    </div>
  );
}

function HierarchyNode({ node, depth, selectedId, onSelect }: {
  node: UiNode;
  depth: number;
  selectedId: string;
  onSelect: (id: string) => void;
}) {
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
