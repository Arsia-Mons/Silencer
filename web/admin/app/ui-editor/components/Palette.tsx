import { KIND_LABELS } from "../ui-editor-constants";
import {
  PALETTE_NODE_KINDS,
  canCreateUiNodeKind,
  type UiNodeKind,
  type UiSurfaceTokenManifest,
} from "../../../lib/ui-layout";
import { UI_PALETTE_DRAG_TYPE } from "../ui-editor-dnd";

export function Palette({
  tokenManifest,
  onAdd,
}: {
  tokenManifest: UiSurfaceTokenManifest | null;
  onAdd: (kind: UiNodeKind) => void;
}) {
  return (
    <div className="border-b border-game-border p-4">
      <div className="text-xs tracking-widest text-game-primary mb-3">PALETTE</div>
      <div className="grid grid-cols-2 gap-2">
        {PALETTE_NODE_KINDS.map((kind) => {
          const enabled = canCreateUiNodeKind(kind, tokenManifest);
          return (
            <button
              key={kind}
              draggable={enabled}
              disabled={!enabled}
              onDragStart={(event) => {
                if (enabled) event.dataTransfer.setData(UI_PALETTE_DRAG_TYPE, kind);
              }}
              onClick={() => {
                if (enabled) onAdd(kind);
              }}
              className="border border-game-border bg-game-bg px-2 py-2 text-[11px] tracking-widest text-game-textDim hover:border-game-primary hover:text-game-text disabled:opacity-35"
            >
              {KIND_LABELS[kind]}
            </button>
          );
        })}
      </div>
    </div>
  );
}
