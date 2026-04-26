'use client';
import { useEffect, useRef, useState } from 'react';
import { ACTOR_DEFS, ACTOR_TYPE_HINTS } from './Toolbar';
import type { MapActor } from '../../lib/types';

const DIRECTION_LABELS: Record<number, string> = {
  0:'Right', 1:'Down-Right', 2:'Down', 3:'Down-Left',
  4:'Left', 5:'Up-Left', 6:'Up', 7:'Up-Right',
};

interface FieldState {
  type: string;
  direction: string;
  matchid: string;
  securityid: string;
  subplane: string;
}

interface Props {
  actor: MapActor;
  actorIdx: number;
  screenX: number;
  screenY: number;
  onUpdate: (idx: number, patch: Partial<MapActor>) => void;
  onDelete: (idx: number) => void;
  onClose: () => void;
}

export default function ActorContextMenu({ actor, actorIdx, screenX, screenY, onUpdate, onDelete, onClose }: Props) {
  const ref = useRef<HTMLDivElement>(null);
  const def = ACTOR_DEFS.find(d => d.id === actor.id) ?? { label: `Unknown (${actor.id})`, color: '#6b7280' };
  const typeHint = ACTOR_TYPE_HINTS[actor.id];

  const [fields, setFields] = useState<FieldState>({
    type:       String(actor.type ?? 0),
    direction:  String(actor.direction ?? 0),
    matchid:    String(actor.matchid ?? 0),
    securityid: String(actor.securityid ?? 0),
    subplane:   String(actor.subplane ?? 0),
  });
  const [confirmDelete, setConfirmDelete] = useState(false);

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => { if (e.key === 'Escape') onClose(); };
    const handleClick = (e: MouseEvent) => { if (ref.current && !ref.current.contains(e.target as Node)) onClose(); };
    window.addEventListener('keydown', handleKey);
    window.addEventListener('mousedown', handleClick);
    return () => {
      window.removeEventListener('keydown', handleKey);
      window.removeEventListener('mousedown', handleClick);
    };
  }, [onClose]);

  const menuW = 240, menuH = 300;
  const left = Math.min(screenX, window.innerWidth  - menuW - 8);
  const top  = Math.min(screenY, window.innerHeight - menuH - 8);

  const apply = () => {
    onUpdate(actorIdx, {
      type:       parseInt(fields.type,       10) || 0,
      direction:  parseInt(fields.direction,  10) || 0,
      matchid:    parseInt(fields.matchid,    10) || 0,
      securityid: parseInt(fields.securityid, 10) || 0,
      subplane:   parseInt(fields.subplane,   10) || 0,
    });
    onClose();
  };

  const inp = 'w-full bg-[#0a0a18] border border-[#1a2e1a] text-[#c0f0c0] text-xs px-2 py-0.5 rounded font-mono';
  const lbl = 'text-[#5a8a5a] text-xs font-mono';

  return (
    <div
      ref={ref}
      className="fixed z-50 bg-[#0d1117] border border-[#1a3a1a] rounded shadow-xl p-3 text-xs font-mono"
      style={{ left, top, width: menuW }}
      onMouseDown={e => e.stopPropagation()}
    >
      <div className="flex items-center gap-2 mb-2 pb-2 border-b border-[#1a3a1a]">
        <span className="w-2 h-2 rounded-full inline-block" style={{ background: def.color }} />
        <span className="text-[#c0f0c0] font-bold">{def.label}</span>
        <span className="text-[#3a5a3a] ml-auto">id={actor.id}</span>
      </div>

      <div className="flex gap-2 mb-2">
        <div className="flex-1"><div className={lbl}>X (px)</div><div className="text-[#5a8a5a] font-mono text-xs">{actor.x}</div></div>
        <div className="flex-1"><div className={lbl}>Y (px)</div><div className="text-[#5a8a5a] font-mono text-xs">{actor.y}</div></div>
      </div>

      <div className="mb-1.5">
        <div className={lbl}>{typeHint ? `Type — ${typeHint.label}` : 'Type'}</div>
        {typeHint && Object.keys(typeHint.options).length > 0 ? (
          <select value={fields.type} onChange={e => setFields(f => ({ ...f, type: e.target.value }))} className={inp + ' cursor-pointer'}>
            {Object.entries(typeHint.options).map(([v, l]) => (
              <option key={v} value={v}>{v} — {l}</option>
            ))}
          </select>
        ) : (
          <input type="number" value={fields.type} min={0}
            onChange={e => setFields(f => ({ ...f, type: e.target.value }))} className={inp} />
        )}
      </div>

      <div className="mb-1.5">
        <div className={lbl}>Direction</div>
        <select value={fields.direction} onChange={e => setFields(f => ({ ...f, direction: e.target.value }))} className={inp + ' cursor-pointer'}>
          {Object.entries(DIRECTION_LABELS).map(([v, l]) => (
            <option key={v} value={v}>{v} — {l}</option>
          ))}
        </select>
      </div>

      <div className="mb-1.5">
        <div className={lbl}>Match ID</div>
        <input type="number" value={fields.matchid} min={0} onChange={e => setFields(f => ({ ...f, matchid: e.target.value }))} className={inp} />
      </div>

      <div className="mb-1.5">
        <div className={lbl}>Security ID</div>
        <input type="number" value={fields.securityid} min={0} onChange={e => setFields(f => ({ ...f, securityid: e.target.value }))} className={inp} />
      </div>

      <div className="mb-3">
        <div className={lbl}>Subplane</div>
        <input type="number" value={fields.subplane} min={0} onChange={e => setFields(f => ({ ...f, subplane: e.target.value }))} className={inp} />
      </div>

      <div className="flex gap-2">
        <button onClick={apply}
          className="flex-1 bg-[#1a3a1a] hover:bg-[#2a5a2a] text-[#c0f0c0] text-xs px-2 py-1 rounded border border-[#2a5a2a]">
          ✓ Apply
        </button>
        {confirmDelete ? (
          <>
            <button onClick={() => { onDelete(actorIdx); onClose(); }}
              className="flex-1 bg-[#5a1a1a] hover:bg-[#7a2a2a] text-[#f08080] text-xs px-2 py-1 rounded border border-[#7a2a2a]">
              Yes
            </button>
            <button onClick={() => setConfirmDelete(false)}
              className="bg-[#1a1a2a] hover:bg-[#2a2a3a] text-[#a0a0a0] text-xs px-2 py-1 rounded border border-[#2a2a3a]">
              No
            </button>
          </>
        ) : (
          <button onClick={() => setConfirmDelete(true)}
            className="bg-[#3a1a1a] hover:bg-[#5a2a2a] text-[#f08080] text-xs px-2 py-1 rounded border border-[#5a2a2a]"
            title="Delete actor">
            ✕ Del
          </button>
        )}
        <button onClick={onClose}
          className="bg-[#1a1a2a] hover:bg-[#2a2a3a] text-[#808080] text-xs px-2 py-1 rounded border border-[#2a2a3a]">
          ✗
        </button>
      </div>
    </div>
  );
}
