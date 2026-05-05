'use client';
import type { NavLink, MapPlatform } from '../../lib/types';

const TYPE_LABEL: Record<number, string> = { 0: 'JUMP', 1: 'FALL', 2: 'JETPACK' };
const TYPE_COLOR: Record<number, string> = { 0: '#00ff88', 1: '#ffdd00', 2: '#ff6644' };

interface Props {
  navLinks: NavLink[];
  platforms: MapPlatform[];
  selectedIdx: number | null;
  onSelect: (idx: number) => void;
  onRemove: (idx: number) => void;
  onTypeChange: (idx: number, type: 0 | 1 | 2) => void;
  onSourceXChange?: (idx: number, v: number) => void;
  onTargetXChange?: (idx: number, v: number) => void;
  onCenter?: (midX: number, midY: number) => void;
}

function platLabel(p: MapPlatform | undefined, idx: number) {
  if (!p) return `#${idx}`;
  const cx = Math.round((p.x1 + p.x2) / 2);
  const cy = Math.round(Math.min(p.y1, p.y2));
  return `#${idx} (${cx},${cy})`;
}

function WaypointInput({ label, value, onChange }: { label: string; value: number; onChange: (v: number) => void }) {
  const isSet = value !== -2147483648;
  return (
    <div className="flex items-center gap-1 mt-1 ml-1">
      <span className="text-[9px] text-game-muted font-mono">{label}</span>
      <input
        type="number"
        value={isSet ? value : ''}
        placeholder="auto"
        onClick={e => e.stopPropagation()}
        onChange={e => {
          const v = e.target.value === '' ? -2147483648 : parseInt(e.target.value, 10);
          if (!isNaN(v)) onChange(v);
        }}
        className="w-20 bg-game-bgCard border border-game-border/60 font-mono text-[10px] px-1 py-0.5 rounded focus:outline-none focus:border-game-primary text-game-text"
      />
      {isSet && (
        <button
          onClick={e => { e.stopPropagation(); onChange(-2147483648); }}
          className="text-game-muted hover:text-red-400 text-[9px] px-0.5"
          title="Clear (auto)"
        >✕</button>
      )}
    </div>
  );
}

export default function NavLinkPanel({ navLinks, platforms, selectedIdx, onSelect, onRemove, onTypeChange, onSourceXChange, onTargetXChange, onCenter }: Props) {
  return (
    <div className="flex-1 min-h-0 flex flex-col">
      <div className="px-3 py-1.5 border-b border-game-border/50 flex items-center justify-between flex-shrink-0">
        <span className="text-[10px] font-mono text-game-textDim tracking-widest">
          {navLinks.length} LINK{navLinks.length !== 1 ? 'S' : ''}
        </span>
        <span className="text-[9px] font-mono text-game-muted">use ⇒ LINK tool to add</span>
      </div>
      <div className="flex-1 min-h-0 overflow-y-auto">
        {navLinks.length === 0 && (
          <div className="px-3 py-4 text-[11px] text-game-muted font-mono text-center">
            No nav links.<br />Select the ⇒ LINK tool<br />and click two platforms.
          </div>
        )}
        {navLinks.map((nl, i) => {
          const from = platforms[nl.fromIdx];
          const to   = platforms[nl.toIdx];
          const isSelected = i === selectedIdx;
          return (
            <div
              key={i}
              onClick={() => onSelect(i)}
              onDoubleClick={() => {
                if (!from || !to) return;
                const midX = ((from.x1 + from.x2) / 2 + (to.x1 + to.x2) / 2) / 2;
                const midY = (Math.min(from.y1, from.y2) + Math.min(to.y1, to.y2)) / 2;
                onCenter?.(midX, midY);
              }}
              className={`flex flex-col px-3 py-1.5 text-[11px] font-mono cursor-pointer border-b border-game-border/30 select-none transition-colors ${
                isSelected ? 'bg-[#1a2e1a] text-game-text' : 'hover:bg-game-dark text-game-textDim'
              }`}
            >
              <div className="flex items-center gap-2">
                {/* Type badge / picker */}
                <select
                  value={nl.type}
                  onChange={e => { e.stopPropagation(); onTypeChange(i, Number(e.target.value) as 0 | 1 | 2); }}
                  onClick={e => e.stopPropagation()}
                  className="bg-game-bgCard border border-game-border/60 font-mono text-[10px] px-1 py-0.5 rounded focus:outline-none focus:border-game-primary flex-shrink-0"
                  style={{ color: TYPE_COLOR[nl.type] }}
                >
                  <option value={0} style={{ color: '#00ff88' }}>JUMP</option>
                  <option value={1} style={{ color: '#ffdd00' }}>FALL</option>
                  <option value={2} style={{ color: '#ff6644' }}>JETPACK</option>
                </select>

                {/* From → To */}
                <span className="flex-1 truncate text-[10px]">
                  <span className="text-game-textDim">{platLabel(from, nl.fromIdx)}</span>
                  <span className="text-game-muted mx-1">→</span>
                  <span className="text-game-textDim">{platLabel(to, nl.toIdx)}</span>
                </span>

                {/* Delete */}
                <button
                  onClick={e => { e.stopPropagation(); onRemove(i); }}
                  className="text-game-muted hover:text-red-400 transition-colors flex-shrink-0 px-1"
                  title="Remove link"
                >✕</button>
              </div>

              {/* JETPACK waypoint inputs */}
              {nl.type === 2 && isSelected && (
                <div onClick={e => e.stopPropagation()}>
                  <WaypointInput
                    label="launch X"
                    value={nl.sourceX ?? -2147483648}
                    onChange={v => onSourceXChange?.(i, v)}
                  />
                  <WaypointInput
                    label="target X"
                    value={nl.targetX}
                    onChange={v => onTargetXChange?.(i, v)}
                  />
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
