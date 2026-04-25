'use client';
import { useState } from 'react';
import { ACTOR_DEFS } from './Toolbar.js';

function getActorDef(id) {
  return ACTOR_DEFS.find(a => a.id === id) ?? { icon: '??', color: '#6b7280', label: `Unknown (${id})` };
}

export default function ActorListPanel({ actors, highlightIdx, onCenter, onActorRightClick }) {
  const [expanded, setExpanded] = useState(true);

  if (!actors) return null;

  return (
    <div className="border-t border-game-border flex-shrink-0 flex flex-col min-h-0">
      <button
        onClick={() => setExpanded(e => !e)}
        className="w-full flex items-center justify-between px-3 py-2 text-xs font-mono text-game-textDim hover:text-game-text bg-game-bgCard flex-shrink-0"
      >
        <span>ACTORS ({actors.length})</span>
        <span>{expanded ? '▲' : '▼'}</span>
      </button>
      {expanded && (
        <div className="overflow-y-auto" style={{ maxHeight: '180px' }}>
          {actors.length === 0 && (
            <div className="px-3 py-2 text-xs text-game-muted font-mono">No actors placed</div>
          )}
          {actors.map((actor, i) => {
            const def = getActorDef(actor.id);
            const isSelected = i === highlightIdx;
            return (
              <div
                key={i}
                className={`flex items-center gap-2 px-3 py-1 text-xs font-mono cursor-pointer border-b border-game-border/30 select-none transition-colors ${isSelected ? 'bg-[#1a2e1a] text-game-text' : 'hover:bg-game-dark text-game-textDim'}`}
                onClick={() => onCenter?.(actor)}
                onContextMenu={e => { e.preventDefault(); onActorRightClick?.(i, e.clientX, e.clientY); }}
                title="Click to center · right-click to edit"
              >
                <span className={`w-2 h-2 rounded-full flex-shrink-0 inline-block ${isSelected ? 'ring-1 ring-white' : ''}`} style={{ background: def.color }} />
                <span className="flex-1 truncate">{def.label}</span>
                <span className="text-game-muted text-[10px]">{actor.x},{actor.y}</span>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}
