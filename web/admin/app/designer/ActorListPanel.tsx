'use client';
import { useState } from 'react';
import { ACTOR_DEFS } from './Toolbar';
import type { MapActor } from '../../lib/types';

function getActorDef(id: number) {
  return ACTOR_DEFS.find(a => a.id === id) ?? { icon: '??', color: '#6b7280', label: `Unknown (${id})` };
}

interface Props {
  actors: MapActor[] | null;
  highlightIdx: number | null;
  onCenter?: (actor: MapActor) => void;
  onActorRightClick?: (idx: number, sx: number, sy: number) => void;
}

export default function ActorListPanel({ actors, highlightIdx, onCenter, onActorRightClick }: Props) {
  const [expanded, setExpanded] = useState(true);
  const [query, setQuery] = useState('');

  if (!actors) return null;

  const filtered = actors
    .map((actor, i) => ({ actor, i }))
    .filter(({ actor }) => {
      if (!query) return true;
      const def = getActorDef(actor.id);
      return def.label.toLowerCase().includes(query.toLowerCase());
    });

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
        <>
          <div className="px-2 py-1 border-b border-game-border/50 bg-game-bgCard flex-shrink-0">
            <input
              type="text"
              value={query}
              onChange={e => setQuery(e.target.value)}
              placeholder="filter actors…"
              className="w-full bg-transparent text-[11px] font-mono text-game-text placeholder:text-game-muted outline-none border border-game-border/40 rounded px-2 py-0.5 focus:border-game-primary/60"
            />
          </div>
          <div className="overflow-y-auto" style={{ maxHeight: '180px' }}>
            {filtered.length === 0 && (
              <div className="px-3 py-2 text-xs text-game-muted font-mono">
                {query ? 'No matches' : 'No actors placed'}
              </div>
            )}
            {filtered.map(({ actor, i }) => {
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
        </>
      )}
    </div>
  );
}
