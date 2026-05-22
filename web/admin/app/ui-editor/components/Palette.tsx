import { KIND_LABELS } from '../ui-editor-constants';
import { PALETTE_NODE_KINDS, type UiNodeKind } from '../../../lib/ui-layout';

export function Palette({ onAdd }: { onAdd: (kind: UiNodeKind) => void }) {
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
