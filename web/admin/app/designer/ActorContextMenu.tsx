'use client';
import { useEffect, useRef, useState } from 'react';
import { ACTOR_DEFS, ACTOR_TYPE_HINTS } from './Toolbar';
import { useLightsStore } from '../../lib/lights-store';
import type { MapActor } from '../../lib/types';

// Guards (0,2,3) use direction as a facing boolean only: 0=Right, 1=Left
const FACING_ACTOR_IDS = new Set([0, 2, 3]);
const FACING_LABELS: Record<number, string> = { 0: 'Right', 1: 'Left' };
const DIRECTION_LABELS: Record<number, string> = {
  0:'Right', 1:'Down-Right', 2:'Down', 3:'Down-Left',
  4:'Left', 5:'Up-Left', 6:'Up', 7:'Up-Right',
};

// Light actor (id=71) actortype bitfield helpers
// bits 0-1: size, bit 2: shape, bits 3-4: animation, bits 8-15: colorR, bits 16-23: colorG, bits 24-31: colorB
function decodeLightType(type: number): { size: number; r: number; g: number; b: number } {
  const u = (type ?? 0) >>> 0;
  return { size: u & 3, r: (u >>> 8) & 0xFF, g: (u >>> 16) & 0xFF, b: (u >>> 24) & 0xFF };
}
function encodeLightType(size: number, r: number, g: number, b: number, existing: number): number {
  const u = (existing >>> 0) & ~0xFFFFFF07; // preserve shape/anim bits (2-4), clear size+color
  return (u | (size & 3) | ((r & 0xFF) << 8) | ((g & 0xFF) << 16) | ((b & 0xFF) << 24)) | 0;
}
function rgbToHex(r: number, g: number, b: number): string {
  return '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
}
function hexToRgb(hex: string): { r: number; g: number; b: number } {
  const m = /^#?([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(hex);
  return m ? { r: parseInt(m[1], 16), g: parseInt(m[2], 16), b: parseInt(m[3], 16) } : { r: 0, g: 0, b: 0 };
}
const LIGHT_SIZE_LABELS: Record<number, string> = { 0: 'Small', 1: 'Medium', 2: 'Large' };

interface FieldState {
  type: string;
  direction: string;
  matchid: string;
  securityid: string;
}

interface LightFieldState {
  size: number;
  colorHex: string;
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
  const { lights, load: loadLights } = useLightsStore();
  const isLight = actor.id === 71;

  useEffect(() => { if (isLight) loadLights(); }, [isLight, loadLights]);

  // For Light actors, build type options from the catalogue; fall back to static hints
  const typeHint = !isLight && actor.id === 71 && lights.length > 0
    ? { label: 'Light Type', options: Object.fromEntries(lights.map(l => [String(l.frame), l.name])) }
    : ACTOR_TYPE_HINTS[actor.id];

  const [fields, setFields] = useState<FieldState>({
    type:       String(actor.type ?? 0),
    direction:  String(actor.direction ?? 0),
    matchid:    String(actor.matchid ?? 0),
    securityid: String(actor.securityid ?? 0),
  });

  // Light-specific decoded state
  const initLight = decodeLightType(actor.type ?? 0);
  const [lightFields, setLightFields] = useState<LightFieldState>({
    size: initLight.size,
    colorHex: (initLight.r || initLight.g || initLight.b) ? rgbToHex(initLight.r, initLight.g, initLight.b) : '#000000',
  });
  // Whether the color is active (non-black = tinted)
  const [lightColorEnabled, setLightColorEnabled] = useState(!!(initLight.r || initLight.g || initLight.b));

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
    if (isLight) {
      const { r, g, b } = lightColorEnabled ? hexToRgb(lightFields.colorHex) : { r: 0, g: 0, b: 0 };
      onUpdate(actorIdx, { type: encodeLightType(lightFields.size, r, g, b, actor.type ?? 0) });
    } else {
      onUpdate(actorIdx, {
        type:       parseInt(fields.type,       10) || 0,
        direction:  parseInt(fields.direction,  10) || 0,
        matchid:    parseInt(fields.matchid,    10) || 0,
        securityid: parseInt(fields.securityid, 10) || 0,
      });
    }
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

      {isLight ? (
        <>
          <div className="mb-1.5">
            <div className={lbl}>Size</div>
            <select value={lightFields.size} onChange={e => setLightFields(f => ({ ...f, size: Number(e.target.value) }))} className={inp + ' cursor-pointer'}>
              {Object.entries(LIGHT_SIZE_LABELS).map(([v, l]) => (
                <option key={v} value={v}>{l}</option>
              ))}
            </select>
          </div>
          <div className="mb-1.5">
            <div className="flex items-center gap-2 mb-0.5">
              <span className={lbl}>Color tint</span>
              <label className="flex items-center gap-1 cursor-pointer ml-auto">
                <input type="checkbox" checked={lightColorEnabled} onChange={e => setLightColorEnabled(e.target.checked)} className="accent-[#4a8a4a]" />
                <span className="text-[#5a8a5a] text-[10px]">enable</span>
              </label>
            </div>
            <input
              type="color"
              value={lightFields.colorHex}
              disabled={!lightColorEnabled}
              onChange={e => setLightFields(f => ({ ...f, colorHex: e.target.value }))}
              className="w-full h-8 rounded cursor-pointer border border-[#1a2e1a] disabled:opacity-30"
            />
            {lightColorEnabled && (
              <div className="text-[#3a6a3a] text-[10px] mt-0.5">{lightFields.colorHex} — hue-shifts lit pixels toward this color</div>
            )}
            {!lightColorEnabled && (
              <div className="text-[#3a6a3a] text-[10px] mt-0.5">Neutral white light (no tint)</div>
            )}
          </div>
        </>
      ) : (
        <>
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
            <div className={lbl}>Direction / Facing</div>
            {FACING_ACTOR_IDS.has(actor.id) ? (
              <select value={fields.direction} onChange={e => setFields(f => ({ ...f, direction: e.target.value }))} className={inp + ' cursor-pointer'}>
                {Object.entries(FACING_LABELS).map(([v, l]) => (
                  <option key={v} value={v}>{v} — {l}</option>
                ))}
              </select>
            ) : (
              <select value={fields.direction} onChange={e => setFields(f => ({ ...f, direction: e.target.value }))} className={inp + ' cursor-pointer'}>
                {Object.entries(DIRECTION_LABELS).map(([v, l]) => (
                  <option key={v} value={v}>{v} — {l}</option>
                ))}
              </select>
            )}
          </div>

          <div className="mb-1.5">
            <div className={lbl}>Match ID</div>
            <input type="number" value={fields.matchid} min={0} onChange={e => setFields(f => ({ ...f, matchid: e.target.value }))} className={inp} />
          </div>

          <div className="mb-1.5">
            <div className={lbl}>Security ID — spawn condition</div>
            <select value={fields.securityid} onChange={e => setFields(f => ({ ...f, securityid: e.target.value }))} className={inp + ' cursor-pointer'}>
              <option value="0">0 — Always spawn</option>
              <option value="1">1 — Low security only</option>
              <option value="2">2 — Medium security only</option>
              <option value="3">3 — Low or Medium</option>
              <option value="4">4 — High security only</option>
              <option value="5">5 — Low or High</option>
              <option value="6">6 — Medium or High</option>
            </select>
          </div>
        </>
      )}

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
