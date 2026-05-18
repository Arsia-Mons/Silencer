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

// Magistrate actor id
const MAGISTRATE_ID = 72;
// Vanta actor id
const VANTA_ID = 73;
// type 0=guard-blaster, 1=guard-laser, 2=guard-rocket, 3=robot
const MAGISTRATE_SPAWN_TYPES: Record<number, string> = {
  0: 'Guard (Blaster)',
  1: 'Guard (Laser)',
  2: 'Guard (Rocket)',
  3: 'Robot',
};

// Decode 4 packed spawn entries from a Uint32.
// Each byte: high nibble = type (0-3), low nibble = count (1-15; 0=unused).
function decodeMagistrateEntries(matchid: number): Array<{ type: number; count: number }> {
  const out: Array<{ type: number; count: number }> = [];
  for(let slot = 0; slot < 4; slot++){
    const byte = (matchid >>> (slot * 8)) & 0xFF;
    const count = byte & 0x0F;
    const type  = (byte >>> 4) & 0x0F;
    if(count > 0) out.push({ type, count });
  }
  return out.length > 0 ? out : [{ type: 0, count: 3 }]; // default 3 blasters
}
function encodeMagistrateEntries(entries: Array<{ type: number; count: number }>): number {
  let v = 0;
  for(let i = 0; i < Math.min(entries.length, 4); i++){
    const count = Math.min(15, Math.max(1, entries[i].count));
    const type  = entries[i].type & 0x0F;
    v |= ((type << 4) | count) << (i * 8);
  }
  return v >>> 0;
}

interface MagistrateFieldState {
  facing: string;
  radius: string;
  securityid: string;
  entries: Array<{ type: number; count: number }>;
  activationSecs: string;  // 0 = use GAS default; stored as ticks (×60) in bits 8-23 of actor.type
  secretTriggerN: string;  // 0 = disabled; stored in bits 24-31 of actor.type
}

// Light actor (id=71) actortype bitfield helpers
// bits 0-1: size, bit 2: shape, bits 3-4: animation, bits 5-6: pulse speed, bits 8-15: colorR, bits 16-23: colorG, bits 24-31: colorB
// actor.direction (separate field): spot direction 0-7 (E,NE,N,NW,W,SW,S,SE)
function decodeLightType(type: number): { size: number; shape: number; anim: number; pulseSpeed: number; dynShadows: boolean; r: number; g: number; b: number } {
  const u = (type ?? 0) >>> 0;
  return { size: u & 3, shape: (u >>> 2) & 1, anim: (u >>> 3) & 3, pulseSpeed: (u >>> 5) & 3, dynShadows: !!((u >>> 7) & 1), r: (u >>> 8) & 0xFF, g: (u >>> 16) & 0xFF, b: (u >>> 24) & 0xFF };
}
function encodeLightType(size: number, shape: number, anim: number, pulseSpeed: number, dynShadows: boolean, r: number, g: number, b: number): number {
  return ((size & 3) | ((shape & 1) << 2) | ((anim & 3) << 3) | ((pulseSpeed & 3) << 5) | (dynShadows ? 1 << 7 : 0) | ((r & 0xFF) << 8) | ((g & 0xFF) << 16) | ((b & 0xFF) << 24)) | 0;
}
function rgbToHex(r: number, g: number, b: number): string {
  return '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
}
function hexToRgb(hex: string): { r: number; g: number; b: number } {
  const m = /^#?([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(hex);
  return m ? { r: parseInt(m[1], 16), g: parseInt(m[2], 16), b: parseInt(m[3], 16) } : { r: 0, g: 0, b: 0 };
}
const LIGHT_SIZE_LABELS: Record<number, string> = { 0: 'Small', 1: 'Medium', 2: 'Large' };
const SPOT_DIR_LABELS: Record<number, string> = { 0: 'E', 1: 'NE', 2: 'N', 3: 'NW', 4: 'W', 5: 'SW', 6: 'S', 7: 'SE' };
// Compass layout: [row][col] = direction index (or -1 for center)
const COMPASS_GRID = [[3,2,1],[4,-1,0],[5,6,7]] as const;

interface FieldState {
  type: string;
  direction: string;
  matchid: string;
  securityid: string;
  destructible: boolean;
  collectible: boolean;
  health: string;
}

interface LightFieldState {
  size: number;
  shape: number;
  anim: number;
  pulseSpeed: number;
  dynShadows: boolean;
  colorHex: string;
  direction: number;
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
  const isMagistrate = actor.id === MAGISTRATE_ID;
  const isVanta = actor.id === VANTA_ID;
  const isMagistrateClass = isMagistrate || isVanta;

  useEffect(() => { if (isLight) loadLights(); }, [isLight, loadLights]);

  // For non-light actors, build type hint label from static hints catalogue
  const typeHint = !isLight && !isMagistrateClass ? ACTOR_TYPE_HINTS[actor.id] : undefined;

  const [fields, setFields] = useState<FieldState>({
    type:         String(actor.type ?? 0),
    direction:    String(actor.direction ?? 0),
    matchid:      String(actor.matchid ?? 0),
    securityid:   String(actor.securityid ?? 0),
    destructible: !!(actor.unknown & 1),
    collectible:  !!(actor.unknown & 2),
    health:       String(((actor.unknown ?? 0) >> 8) & 0xFF || 100),
  });

  // Magistrate-specific decoded state
  // actor.type bits 0-7 = radius; actor.matchid = 4 packed spawn entries
  const [magistrateFields, setMagistrateFields] = useState<MagistrateFieldState>({
    facing:         String(actor.direction ?? 0),
    radius:         String((actor.type ?? 0) & 0xFF || 64),
    securityid:     String(actor.securityid ?? 0),
    entries:        decodeMagistrateEntries(actor.matchid ?? 0),
    activationSecs: String((((actor.type ?? 0) >>> 8) & 0xFFFF) / 60 | 0),
    secretTriggerN: String(((actor.type ?? 0) >>> 24) & 0xFF),
  });

  // Light-specific decoded state
  const initLight = decodeLightType(actor.type ?? 0);
  const [lightFields, setLightFields] = useState<LightFieldState>({
    size: initLight.size,
    shape: initLight.shape,
    anim: initLight.anim,
    pulseSpeed: initLight.pulseSpeed,
    dynShadows: initLight.dynShadows,
    colorHex: (initLight.r || initLight.g || initLight.b) ? rgbToHex(initLight.r, initLight.g, initLight.b) : '#000000',
    direction: actor.direction ?? 0,
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
      onUpdate(actorIdx, {
        type: encodeLightType(lightFields.size, lightFields.shape, lightFields.anim, lightFields.pulseSpeed, lightFields.dynShadows, r, g, b),
        direction: lightFields.direction,
      });
    } else if (isMagistrateClass) {
      const radius       = Math.min(255, Math.max(8, parseInt(magistrateFields.radius, 10) || 64));
      const actSecs      = Math.min(65535 / 60 | 0, Math.max(0, parseInt(magistrateFields.activationSecs, 10) || 0));
      const actTicks     = actSecs * 60;
      const secretN      = Math.min(255, Math.max(0, parseInt(magistrateFields.secretTriggerN, 10) || 0));
      // Pack: bits 0-7 = radius, bits 8-23 = activationTicks, bits 24-31 = secretTriggerN
      const packedType   = (radius & 0xFF) | ((actTicks & 0xFFFF) << 8) | ((secretN & 0xFF) << 24);
      onUpdate(actorIdx, {
        type:       packedType >>> 0,
        direction:  parseInt(magistrateFields.facing, 10) || 0,
        matchid:    encodeMagistrateEntries(magistrateFields.entries),
        securityid: parseInt(magistrateFields.securityid, 10) || 0,
      });
    } else {
      const hp = Math.min(255, Math.max(1, parseInt(fields.health, 10) || 100));
      const unknownFlags = (fields.destructible ? 1 : 0) | (fields.collectible ? 2 : 0) | ((fields.destructible ? hp : 0) << 8);
      onUpdate(actorIdx, {
        type:       parseInt(fields.type,       10) || 0,
        direction:  parseInt(fields.direction,  10) || 0,
        matchid:    parseInt(fields.matchid,    10) || 0,
        securityid: parseInt(fields.securityid, 10) || 0,
        unknown:    unknownFlags,
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
            <div className={lbl}>Shape</div>
            <select value={lightFields.shape} onChange={e => setLightFields(f => ({ ...f, shape: Number(e.target.value) }))} className={inp + ' cursor-pointer'}>
              <option value={0}>Halo (radial)</option>
              <option value={1}>Spot (cone) ⚠ needs art</option>
            </select>
          </div>
          {lightFields.shape === 1 && (
            <div className="mb-1.5">
              <div className={lbl}>Direction</div>
              <div className="grid grid-cols-3 gap-0.5 w-20 mt-0.5">
                {COMPASS_GRID.map((row, ri) => row.map((dir, ci) => (
                  dir === -1
                    ? <div key={`${ri}-${ci}`} className="w-6 h-6 rounded bg-[#0a0a18] border border-[#1a2e1a]" />
                    : <button
                        key={dir}
                        onClick={() => setLightFields(f => ({ ...f, direction: dir }))}
                        className={`w-6 h-6 rounded text-[9px] font-bold border ${lightFields.direction === dir ? 'bg-[#2a4a2a] border-[#4a8a4a] text-[#c0f0c0]' : 'bg-[#0a0a18] border-[#1a2e1a] text-[#5a8a5a]'} cursor-pointer`}
                      >
                        {SPOT_DIR_LABELS[dir]}
                      </button>
                )))}
              </div>
            </div>
          )}
          <div className="mb-1.5">
            <div className={lbl}>Animation</div>
            <select value={lightFields.anim} onChange={e => setLightFields(f => ({ ...f, anim: Number(e.target.value) }))} className={inp + ' cursor-pointer'}>
              <option value={0}>Static</option>
              <option value={1}>Flicker</option>
              <option value={2}>Pulse</option>
            </select>
          </div>
          {lightFields.anim === 2 && (
            <div className="mb-1.5">
              <div className={lbl}>Pulse speed</div>
              <select value={lightFields.pulseSpeed} onChange={e => setLightFields(f => ({ ...f, pulseSpeed: Number(e.target.value) }))} className={inp + ' cursor-pointer'}>
                <option value={0}>Slow</option>
                <option value={1}>Medium</option>
                <option value={2}>Fast</option>
              </select>
            </div>
          )}
          <div className="mb-1.5">
            <label className="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" checked={lightFields.dynShadows} onChange={e => setLightFields(f => ({ ...f, dynShadows: e.target.checked }))} className="accent-[#4a8a4a]" />
              <span className={lbl}>Dynamic shadows</span>
            </label>
            <div className="text-[#3a6a3a] text-[10px] mt-0.5">Players &amp; guards cast shadows at runtime</div>
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
      ) : isMagistrateClass ? (
        <>
          <div className="mb-1.5">
            <div className={lbl}>Facing</div>
            <select value={magistrateFields.facing} onChange={e => setMagistrateFields(f => ({ ...f, facing: e.target.value }))} className={inp + ' cursor-pointer'}>
              <option value="0">0 — Right</option>
              <option value="1">1 — Left</option>
            </select>
          </div>
          <div className="border-t border-[#1a3a1a] pt-1.5 mt-1.5 mb-1">
            <div className="flex items-center mb-1">
              <span className="text-[#5a8a5a] text-[10px] uppercase tracking-wide">Death spawn</span>
              {magistrateFields.entries.length < 4 && (
                <button
                  onClick={() => setMagistrateFields(f => ({ ...f, entries: [...f.entries, { type: 0, count: 1 }] }))}
                  className="ml-auto text-[#4a8a4a] hover:text-[#c0f0c0] text-[10px] font-mono border border-[#2a4a2a] hover:border-[#4a8a4a] px-1.5 rounded"
                >+ Add</button>
              )}
            </div>
            {magistrateFields.entries.map((entry, idx) => (
              <div key={idx} className="flex items-center gap-1 mb-1">
                <select
                  value={entry.type}
                  onChange={e => setMagistrateFields(f => {
                    const entries = [...f.entries];
                    entries[idx] = { ...entries[idx], type: Number(e.target.value) };
                    return { ...f, entries };
                  })}
                  className="flex-1 bg-[#0a0a18] border border-[#1a2e1a] text-[#c0f0c0] text-[10px] px-1 py-0.5 rounded font-mono cursor-pointer"
                >
                  {Object.entries(MAGISTRATE_SPAWN_TYPES).map(([v, l]) => (
                    <option key={v} value={v}>{l}</option>
                  ))}
                </select>
                <input
                  type="number" min={1} max={15} value={entry.count}
                  onChange={e => setMagistrateFields(f => {
                    const entries = [...f.entries];
                    entries[idx] = { ...entries[idx], count: parseInt(e.target.value, 10) || 1 };
                    return { ...f, entries };
                  })}
                  className="w-10 bg-[#0a0a18] border border-[#1a2e1a] text-[#c0f0c0] text-[10px] px-1 py-0.5 rounded font-mono text-center"
                  title="Count (1–15)"
                />
                <button
                  onClick={() => setMagistrateFields(f => ({ ...f, entries: f.entries.filter((_, i) => i !== idx) }))}
                  className="text-[#5a3a3a] hover:text-[#f08080] text-[10px] font-mono px-1"
                  title="Remove"
                >✕</button>
              </div>
            ))}
            <div className="mt-1">
              <div className={lbl}>Radius (px)</div>
              <input type="number" min={8} max={255} value={magistrateFields.radius}
                onChange={e => setMagistrateFields(f => ({ ...f, radius: e.target.value }))} className={inp} />
            </div>
          </div>
          <div className="border-t border-[#1a3a1a] pt-1.5 mt-1.5 mb-1">
            <div className="text-[#5a8a5a] text-[10px] uppercase tracking-wide mb-1">Activation conditions</div>
            <div className="mb-1.5">
              <div className={lbl}>Activation delay (seconds, 0 = GAS default)</div>
              <input type="number" min={0} max={1092} value={magistrateFields.activationSecs}
                onChange={e => setMagistrateFields(f => ({ ...f, activationSecs: e.target.value }))} className={inp}
                placeholder="0 = use GAS default" />
            </div>
            <div className="mb-1.5">
              <div className={lbl}>Secrets to activate (0 = disabled)</div>
              <input type="number" min={0} max={255} value={magistrateFields.secretTriggerN}
                onChange={e => setMagistrateFields(f => ({ ...f, secretTriggerN: e.target.value }))} className={inp}
                placeholder="0 = disabled" />
            </div>
          </div>
          <div className="mb-1.5">
            <div className={lbl}>Security ID — spawn condition</div>
            <select value={magistrateFields.securityid} onChange={e => setMagistrateFields(f => ({ ...f, securityid: e.target.value }))} className={inp + ' cursor-pointer'}>
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

          <div className="border-t border-[#1a3a1a] pt-1.5 mt-1.5 flex flex-col gap-1">
            <label className="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" checked={fields.destructible}
                onChange={e => setFields(f => ({ ...f, destructible: e.target.checked }))}
                className="accent-[#ff6644]" />
              <span className={lbl}>Destructible</span>
            </label>
            {fields.destructible && (
              <div className="flex items-center gap-2 pl-5">
                <span className={lbl}>Max HP</span>
                <input type="number" min={1} max={255} value={fields.health}
                  onChange={e => setFields(f => ({ ...f, health: e.target.value }))}
                  className={inp + ' w-16'} />
              </div>
            )}
            <label className="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" checked={fields.collectible}
                onChange={e => setFields(f => ({ ...f, collectible: e.target.checked }))}
                className="accent-[#4a8a4a]" />
              <span className={lbl}>Collectible</span>
            </label>
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
