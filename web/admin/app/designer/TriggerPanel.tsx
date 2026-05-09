'use client';
import { useState, useRef } from 'react';
import type {
  TriggerNode, TriggerAction, TriggerCondition, ObjectiveDef, TriggerZone,
  TriggerEventType, TriggerActionType, TriggerConditionType, ConditionLogic,
} from '../../lib/types';

// ── Label maps ────────────────────────────────────────────────────────────────

const EVENT_LABELS: Record<TriggerEventType, string> = {
  NONE:               '—',
  TRIGGER_ENTER_ZONE: '📍 Enter Zone',
  TERMINAL_ACTIVATED: '💻 Terminal',
  ACTOR_KILLED:       '💀 Actor Killed',
  ACTOR_DAMAGED:      '🩹 Actor Damaged',
  OBJECTIVE_COMPLETE: '✅ Objective Done',
  ITEM_COLLECTED:     '📦 Item Collected',
  PLAYER_DIED:        '☠️  Player Died',
  ALL_PLAYERS_DIED:   '💣 All Players Died',
  TIMER_EXPIRED:      '⏱ Timer',
  GAME_START:         '🚩 Game Start',
  PLAYER_SPAWN:       '🎮 Player Spawn',
};

const ACTION_LABELS: Record<TriggerActionType, string> = {
  NONE:                '—',
  OPEN_DOOR:           '🚪 Open Door',
  LOCK_DOOR:           '🔒 Lock Door',
  UNLOCK_DOOR:         '🔓 Unlock Door',
  PLAY_SOUND:          '🔊 Play Sound',
  SHOW_OBJECTIVE:      '📋 Show Message',
  PAN_CAMERA:          '🎥 Pan Camera',
  SPAWN_ACTOR:         '👾 Spawn Actor',
  END_MISSION:         '🏁 End Mission',
  DESTROY_ACTOR:       '💥 Destroy Actor',
  MOVE_ACTOR:          '➡️  Move Actor',
  APPLY_DAMAGE_IN_ZONE:'☢️  Damage Zone',
  ENABLE_TRIGGER:      '▶️  Enable Trigger',
  DISABLE_TRIGGER:     '⏸ Disable Trigger',
  COMPLETE_OBJECTIVE:  '✅ Complete Obj',
  LOCK_INPUT:          '🔒 Lock Input',
  UNLOCK_INPUT:        '🔓 Unlock Input',
  SET_FLAG:            '🚩 Set Flag',
};

const COND_LABELS: Record<TriggerConditionType, string> = {
  NONE:             '—',
  TEAM_CHECK:       '👥 Team',
  OBJECTIVE_STATE:  '📋 Objective',
  PLAYER_COUNT:     '🎮 Player Count',
  HEALTH_THRESHOLD: '❤️  Health',
  COUNT_REACHED:    '🔢 Hit Count',
  FLAG_SET:         '🚩 Flag Set',
};

const EVENT_OPTS = Object.keys(EVENT_LABELS) as TriggerEventType[];
const ACTION_OPTS = Object.keys(ACTION_LABELS) as TriggerActionType[];
const COND_OPTS  = Object.keys(COND_LABELS)  as TriggerConditionType[];

const SOUND_FILES = [
  'airlokj.wav','airvent2.wav','alarm3a.wav','alinvest.wav','alwarn.wav',
  'ambloop4.wav','ambloop5.wav','ambloop7.wav','ammo01.wav','ammo02.wav',
  'ammo03.wav','ammo05.wav','breath2.wav','cathdoor.wav','charged.wav',
  'cliksel2.wav','closer2.mp3','conc1-11.wav','conc2-11.wav','cphum11.wav',
  'disguise.wav','drop4.wav','fall2b.wav','flamebg2.wav','force1.wav',
  'freeze3.wav','freezrt1.wav','futstonl.wav','futstonr.wav','grenade1.wav',
  'grenade5.wav','grenthro.wav','grndown.wav','grnup.wav','groan2.wav',
  'groan2a.wav','grunt2a.wav','if15.wav','intrude.wav','jackin.wav',
  'jackout.wav','jetpak1.wav','jetpak2a.wav','juunewne.wav','ladder1.wav',
  'ladder2.wav','land1.wav','land11.wav','laserel.wav','laserew.wav',
  'laserme.wav','mgbegin.wav','mgend.wav','portal1.wav','portpas2.wav',
  'power11.wav','pwrcon1.wav','q_expl02.wav','reload2.wav','repair.wav',
  'rico1.wav','rico2.wav','robot3l.wav','robot3r.wav','robotarm.wav',
  'rocket1.wav','rocket4.wav','rocket9.wav','roll2.wav','s_flmc01.wav',
  's_hita01.wav','s_hitb01.wav','s_intd01.wav','seekexp1.wav','select2.wav',
  'shield2.wav','shlddn1.wav','stop4.wav','stostep1.wav','stostepr.wav',
  'strike01.wav','strike02.wav','strike03.wav','strike04.wav','theres3.wav',
  'transrev.wav','type1.wav','type2.wav','type3.wav','type4.wav','type5.wav',
  'typerev6.wav','vModDeto.wav','whoom.wav','wndloop1.wav','wndloop2.wav',
  'wndloopb.wav','wndloopc.wav','wndloopd.wav','wndloope.wav',
];

// ── Helpers ───────────────────────────────────────────────────────────────────

function nextId(items: Array<{ id: number }>): number {
  return items.length ? Math.max(...items.map(x => x.id)) + 1 : 1;
}

function emptyNode(id: number): TriggerNode {
  return { id, triggerEvent: 'GAME_START', actorId: 0, oneShot: true, enabled: true,
    conditionLogic: 'ALL_OF', timerSeconds: 0, conditions: [], actions: [] };
}

function emptyCondition(): TriggerCondition {
  return { type: 'TEAM_CHECK', team: 0, objectiveId: 0, playerCount: 1, healthPct: 50, count: 1 };
}

function emptyAction(): TriggerAction {
  return { type: 'OPEN_DOOR', actorId: 0, delay: 0, paramX: 0, paramY: 0, paramF: 0, paramU8: 0, sound: '', message: '' };
}

// ── Sub-components ────────────────────────────────────────────────────────────

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center gap-1.5 min-w-0">
      <span className="text-[9px] text-game-muted font-mono flex-shrink-0 w-16 text-right">{label}</span>
      {children}
    </div>
  );
}

const inp = 'bg-game-bgCard border border-game-border/60 font-mono text-[10px] px-1 py-0.5 rounded focus:outline-none focus:border-game-primary text-game-text min-w-0';
const sel = inp + ' cursor-pointer';

function ConditionRow({ c, onChange, onRemove }: {
  c: TriggerCondition;
  onChange: (patch: Partial<TriggerCondition>) => void;
  onRemove: () => void;
}) {
  return (
    <div className="flex flex-col gap-1 pl-2 border-l-2 border-game-border/40 py-1">
      <div className="flex items-center gap-1">
        <select className={sel} value={c.type} onChange={e => onChange({ type: e.target.value as TriggerConditionType })}>
          {COND_OPTS.map(o => <option key={o} value={o}>{COND_LABELS[o]}</option>)}
        </select>
        <button onClick={onRemove} className="text-game-muted hover:text-red-400 px-1 text-[10px] ml-auto flex-shrink-0">✕</button>
      </div>
      {c.type === 'TEAM_CHECK' && (
        <Field label="team">
          <input className={inp + ' w-12'} type="number" min={0} max={3} value={c.team} onChange={e => onChange({ team: +e.target.value })} />
        </Field>
      )}
      {c.type === 'OBJECTIVE_STATE' && (
        <Field label="obj id">
          <input className={inp + ' w-16'} type="number" min={0} value={c.objectiveId} onChange={e => onChange({ objectiveId: +e.target.value })} />
        </Field>
      )}
      {c.type === 'PLAYER_COUNT' && (
        <Field label="min players">
          <input className={inp + ' w-12'} type="number" min={0} value={c.playerCount} onChange={e => onChange({ playerCount: +e.target.value })} />
        </Field>
      )}
      {c.type === 'HEALTH_THRESHOLD' && (
        <Field label="health %">
          <input className={inp + ' w-12'} type="number" min={0} max={100} value={c.healthPct} onChange={e => onChange({ healthPct: +e.target.value })} />
        </Field>
      )}
      {c.type === 'COUNT_REACHED' && (
        <Field label="min hits">
          <input className={inp + ' w-12'} type="number" min={1} max={255} value={c.count ?? 1} onChange={e => onChange({ count: +e.target.value })} />
        </Field>
      )}
      {c.type === 'FLAG_SET' && (
        <Field label="flag id">
          <input className={inp + ' w-12'} type="number" min={0} max={255} value={c.team} onChange={e => onChange({ team: +e.target.value })} />
        </Field>
      )}
    </div>
  );
}

function ActionRow({ a, onChange, onRemove, onRequestActorLink, dragHandle }: {
  a: TriggerAction;
  onChange: (patch: Partial<TriggerAction>) => void;
  onRemove: () => void;
  onRequestActorLink?: (target: { label: string; onLinked: (matchid: number) => void }) => void;
  dragHandle?: React.ReactNode;
}) {
  const needsActor = ['OPEN_DOOR','LOCK_DOOR','UNLOCK_DOOR','DESTROY_ACTOR','MOVE_ACTOR','ENABLE_TRIGGER','DISABLE_TRIGGER','COMPLETE_OBJECTIVE'].includes(a.type);
  const needsXY    = ['PAN_CAMERA','SPAWN_ACTOR','MOVE_ACTOR','APPLY_DAMAGE_IN_ZONE'].includes(a.type);
  const needsSound = a.type === 'PLAY_SOUND';
  const needsMsg   = a.type === 'SHOW_OBJECTIVE' || a.type === 'PAN_CAMERA';
  const needsF     = a.type === 'MOVE_ACTOR' || a.type === 'APPLY_DAMAGE_IN_ZONE' || a.type === 'SET_FLAG';
  const needsOutcome = a.type === 'END_MISSION';

  return (
    <div className="flex flex-col gap-1 pl-2 border-l-2 border-game-border/40 py-1">
      <div className="flex items-center gap-1">
        {dragHandle}
        <select className={sel} value={a.type} onChange={e => onChange({ type: e.target.value as TriggerActionType })}>
          {ACTION_OPTS.map(o => <option key={o} value={o}>{ACTION_LABELS[o]}</option>)}
        </select>
        <button onClick={onRemove} className="text-game-muted hover:text-red-400 px-1 text-[10px] ml-auto flex-shrink-0">✕</button>
      </div>
      <Field label="delay (s)">
        <input className={inp + ' w-16'} type="number" min={0} step={0.1} value={a.delay} onChange={e => onChange({ delay: +e.target.value })} />
      </Field>
      {needsActor && (
        <Field label={a.type === 'COMPLETE_OBJECTIVE' ? 'obj id' : 'actor id'}>
          <input className={inp + ' w-16'} type="number" min={0} value={a.actorId} onChange={e => onChange({ actorId: +e.target.value })} />
          {onRequestActorLink && a.type !== 'COMPLETE_OBJECTIVE' && (
            <button
              title="Pick actor from canvas"
              onClick={() => onRequestActorLink({ label: `action actor`, onLinked: (id) => onChange({ actorId: id }) })}
              className="text-[10px] hover:text-game-primary transition-colors flex-shrink-0"
            >🎯</button>
          )}
        </Field>
      )}
      {needsOutcome && (
        <Field label="outcome">
          <select className={sel} value={a.paramU8} onChange={e => onChange({ paramU8: +e.target.value })}>
            <option value={0}>neutral</option>
            <option value={1}>win</option>
            <option value={2}>lose</option>
          </select>
        </Field>
      )}
      {needsXY && (
        <>
          <Field label="X">
            <input className={inp + ' w-16'} type="number" value={a.paramX} onChange={e => onChange({ paramX: +e.target.value })} />
          </Field>
          <Field label="Y">
            <input className={inp + ' w-16'} type="number" value={a.paramY} onChange={e => onChange({ paramY: +e.target.value })} />
          </Field>
        </>
      )}
      {needsF && (
        <Field label={a.type === 'MOVE_ACTOR' ? 'duration' : a.type === 'SET_FLAG' ? 'value' : 'radius'}>
          {a.type === 'SET_FLAG' ? (
            <input className={inp + ' w-12'} type="checkbox" checked={a.paramF !== 0}
              onChange={e => onChange({ paramF: e.target.checked ? 1 : 0 })} />
          ) : (
            <input className={inp + ' w-16'} type="number" min={0} step={0.1} value={a.paramF} onChange={e => onChange({ paramF: +e.target.value })} />
          )}
        </Field>
      )}
      {a.type === 'SPAWN_ACTOR' && (
        <Field label="actor type">
          <select className={sel} value={a.paramU8} onChange={e => onChange({ paramU8: +e.target.value, actorId: 0 })}>
            <option value={11}>👤 Civilian</option>
            <option value={12}>💂 Guard</option>
            <option value={13}>🤖 Robot</option>
          </select>
        </Field>
      )}
      {a.type === 'SPAWN_ACTOR' && a.paramU8 === 12 && (
        <Field label="weapon">
          <select className={sel} value={a.actorId} onChange={e => onChange({ actorId: +e.target.value })}>
            <option value={0}>🔫 Blaster</option>
            <option value={1}>⚡ Laser</option>
            <option value={2}>🚀 Rocket</option>
          </select>
        </Field>
      )}
      {(a.type === 'SPAWN_ACTOR' && (a.paramU8 === 12 || a.paramU8 === 13)) && (
        <Field label="patrol">
          <input type="checkbox" checked={a.paramF !== 0} onChange={e => onChange({ paramF: e.target.checked ? 1 : 0 })} />
        </Field>
      )}
      {(a.type === 'APPLY_DAMAGE_IN_ZONE' || a.type === 'SET_FLAG') && (
        <Field label={a.type === 'SET_FLAG' ? 'flag id' : 'damage'}>
          <input className={inp + ' w-16'} type="number" min={0} max={255} value={a.paramU8} onChange={e => onChange({ paramU8: +e.target.value })} />
        </Field>
      )}
      {needsSound && (
        <Field label="sound">
          <select className={sel} value={a.sound} onChange={e => onChange({ sound: e.target.value })}>
            <option value="">— pick sound —</option>
            {SOUND_FILES.map(s => <option key={s} value={s}>{s}</option>)}
          </select>
        </Field>
      )}
      {needsMsg && (
        <Field label="message">
          <input className={inp + ' flex-1'} type="text" maxLength={127} value={a.message}
            placeholder={a.type === 'PAN_CAMERA' ? 'Location label (optional)…' : 'Objective text…'}
            onChange={e => onChange({ message: e.target.value })} />
        </Field>
      )}
    </div>
  );
}

// ── TriggerNodeRow ────────────────────────────────────────────────────────────

function TriggerNodeRow({ node, expanded, onToggle, onChange, onRemove, onRequestActorLink }: {
  node: TriggerNode;
  expanded: boolean;
  onToggle: () => void;
  onChange: (patch: Partial<TriggerNode>) => void;
  onRemove: () => void;
  onRequestActorLink?: (target: { label: string; onLinked: (matchid: number) => void }) => void;
}) {
  const patchCond = (i: number, patch: Partial<TriggerCondition>) => {
    const conditions = node.conditions.map((c, ci) => ci === i ? { ...c, ...patch } : c);
    onChange({ conditions });
  };
  const removeCond = (i: number) => onChange({ conditions: node.conditions.filter((_, ci) => ci !== i) });
  const addCond    = () => onChange({ conditions: [...node.conditions, emptyCondition()] });

  const patchAction = (i: number, patch: Partial<TriggerAction>) => {
    const actions = node.actions.map((a, ai) => ai === i ? { ...a, ...patch } : a);
    onChange({ actions });
  };
  const removeAction = (i: number) => onChange({ actions: node.actions.filter((_, ai) => ai !== i) });
  const addAction    = () => onChange({ actions: [...node.actions, emptyAction()] });

  const dragIdx = useRef<number | null>(null);
  const [dragOverIdx, setDragOverIdx] = useState<number | null>(null);
  const moveAction = (from: number, to: number) => {
    const next = [...node.actions];
    const [item] = next.splice(from, 1);
    next.splice(to, 0, item);
    onChange({ actions: next });
  };

  return (
    <div className="border-b border-game-border/30">
      {/* Header row */}
      <div
        className={`flex items-center gap-1.5 px-2 py-1.5 cursor-pointer select-none transition-colors ${expanded ? 'bg-[#1a2e1a]' : 'hover:bg-game-dark'}`}
        onClick={onToggle}
      >
        <span className="text-[9px] font-mono text-game-muted w-6 flex-shrink-0">#{node.id}</span>
        <span className="flex-1 text-[10px] font-mono text-game-text truncate">{EVENT_LABELS[node.triggerEvent]}</span>
        <span className={`text-[8px] px-1 rounded font-mono ${node.enabled ? 'text-game-primary' : 'text-game-muted line-through'}`}>
          {node.enabled ? 'ON' : 'OFF'}
        </span>
        {node.oneShot && <span className="text-[8px] text-[#c0a000] font-mono">1×</span>}
        <span className="text-game-muted text-[10px] ml-0.5">{expanded ? '▲' : '▼'}</span>
        <button
          onClick={e => { e.stopPropagation(); onRemove(); }}
          className="text-game-muted hover:text-red-400 text-[10px] px-0.5 flex-shrink-0"
        >✕</button>
      </div>

      {/* Expanded editor */}
      {expanded && (
        <div className="px-2 pb-2 flex flex-col gap-2 bg-[#0d1a0d]" onClick={e => e.stopPropagation()}>

          {/* Core fields */}
          <div className="flex flex-col gap-1 pt-1">
            <Field label="event">
              <select className={sel + ' flex-1'} value={node.triggerEvent}
                onChange={e => onChange({ triggerEvent: e.target.value as TriggerEventType })}>
                {EVENT_OPTS.map(o => <option key={o} value={o}>{EVENT_LABELS[o]}</option>)}
              </select>
            </Field>
            {node.triggerEvent !== 'GAME_START' && node.triggerEvent !== 'PLAYER_SPAWN' && node.triggerEvent !== 'ALL_PLAYERS_DIED' && node.triggerEvent !== 'NONE' && (
              <Field label="actor id">
                <input className={inp + ' w-16'} type="number" min={0} value={node.actorId}
                  onChange={e => onChange({ actorId: +e.target.value })}
                  title="0 = any actor" />
                <span className="text-[8px] text-game-muted font-mono">0=any</span>
                {onRequestActorLink && (
                  <button
                    title="Pick actor from canvas"
                    onClick={() => onRequestActorLink({ label: `trigger actor`, onLinked: (id) => onChange({ actorId: id }) })}
                    className="text-[10px] hover:text-game-primary transition-colors flex-shrink-0"
                  >🎯</button>
                )}
              </Field>
            )}
            {node.triggerEvent === 'TIMER_EXPIRED' && (
              <Field label="secs">
                <input className={inp + ' w-16'} type="number" min={0.1} step={0.1} value={node.timerSeconds}
                  onChange={e => onChange({ timerSeconds: +e.target.value })} />
              </Field>
            )}
            <div className="flex gap-3 pl-[74px]">
              <label className="flex items-center gap-1 cursor-pointer select-none">
                <input type="checkbox" checked={node.enabled}
                  onChange={e => onChange({ enabled: e.target.checked })}
                  className="accent-green-500" />
                <span className="text-[9px] font-mono text-game-textDim">enabled</span>
              </label>
              <label className="flex items-center gap-1 cursor-pointer select-none">
                <input type="checkbox" checked={node.oneShot}
                  onChange={e => onChange({ oneShot: e.target.checked })}
                  className="accent-yellow-500" />
                <span className="text-[9px] font-mono text-game-textDim">one-shot</span>
              </label>
            </div>
          </div>

          {/* Conditions */}
          <div>
            <div className="flex items-center gap-2 mb-1">
              <span className="text-[9px] font-mono text-game-muted tracking-widest">CONDITIONS</span>
              <select className={sel} value={node.conditionLogic}
                onChange={e => onChange({ conditionLogic: e.target.value as ConditionLogic })}>
                <option value="ALL_OF">ALL OF</option>
                <option value="ANY_OF">ANY OF</option>
              </select>
              <button onClick={addCond}
                className="ml-auto text-[9px] font-mono text-game-primary border border-game-primary/40 rounded px-1 hover:bg-game-primary/10 transition-colors">
                + COND
              </button>
            </div>
            {node.conditions.length === 0 && (
              <div className="text-[9px] text-game-muted font-mono pl-2">No conditions (always fires)</div>
            )}
            {node.conditions.map((c, i) => (
              <ConditionRow key={i} c={c} onChange={p => patchCond(i, p)} onRemove={() => removeCond(i)} />
            ))}
          </div>

          {/* Actions */}
          <div>
            <div className="flex items-center gap-2 mb-1">
              <span className="text-[9px] font-mono text-game-muted tracking-widest">ACTIONS</span>
              <button onClick={addAction}
                className="ml-auto text-[9px] font-mono text-game-primary border border-game-primary/40 rounded px-1 hover:bg-game-primary/10 transition-colors">
                + ACTION
              </button>
            </div>
            {node.actions.length === 0 && (
              <div className="text-[9px] text-game-muted font-mono pl-2">No actions</div>
            )}
            {node.actions.map((a, i) => (
              <div
                key={i}
                draggable
                onDragStart={() => { dragIdx.current = i; }}
                onDragOver={e => { e.preventDefault(); setDragOverIdx(i); }}
                onDrop={e => {
                  e.preventDefault();
                  if (dragIdx.current !== null && dragIdx.current !== i) moveAction(dragIdx.current, i);
                  dragIdx.current = null; setDragOverIdx(null);
                }}
                onDragEnd={() => { dragIdx.current = null; setDragOverIdx(null); }}
                className={dragOverIdx === i && dragIdx.current !== i ? 'border-t-2 border-game-primary' : ''}
              >
                <ActionRow
                  key={i} a={a}
                  onChange={p => patchAction(i, p)}
                  onRemove={() => removeAction(i)}
                  onRequestActorLink={onRequestActorLink}
                  dragHandle={
                    <span className="cursor-grab text-game-muted text-[10px] select-none flex-shrink-0 px-0.5" title="Drag to reorder">⠿</span>
                  }
                />
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

// ── Main component ────────────────────────────────────────────────────────────

interface Props {
  triggers: TriggerNode[];
  objectives: ObjectiveDef[];
  zones: TriggerZone[];
  onSetTriggers: (t: TriggerNode[]) => void;
  onSetObjectives: (o: ObjectiveDef[]) => void;
  onSetZones: (z: TriggerZone[]) => void;
  onRequestActorLink?: (target: { label: string; onLinked: (matchid: number) => void }) => void;
}

export default function TriggerPanel({ triggers, objectives, zones, onSetTriggers, onSetObjectives, onSetZones, onRequestActorLink }: Props) {
  const [expandedId, setExpandedId] = useState<number | null>(null);
  const [objSection, setObjSection] = useState(true);
  const [zoneSection, setZoneSection] = useState(true);

  const addTrigger = () => {
    const node = emptyNode(nextId(triggers));
    onSetTriggers([...triggers, node]);
    setExpandedId(node.id);
  };

  const removeTrigger = (id: number) => {
    onSetTriggers(triggers.filter(t => t.id !== id));
    if (expandedId === id) setExpandedId(null);
  };

  const patchTrigger = (id: number, patch: Partial<TriggerNode>) => {
    onSetTriggers(triggers.map(t => t.id === id ? { ...t, ...patch } : t));
  };

  const addObjective = () => {
    const obj: ObjectiveDef = { id: nextId(objectives), required: true, text: '' };
    onSetObjectives([...objectives, obj]);
  };

  const removeObjective = (id: number) => onSetObjectives(objectives.filter(o => o.id !== id));
  const patchObjective  = (id: number, patch: Partial<ObjectiveDef>) =>
    onSetObjectives(objectives.map(o => o.id === id ? { ...o, ...patch } : o));

  const addZone = () => {
    const id = zones.length ? Math.max(...zones.map(z => z.id)) + 1 : 1;
    onSetZones([...zones, { id, label: `Zone ${id}`, x1: 0, y1: 0, x2: 256, y2: 256 }]);
  };
  const removeZone = (id: number) => onSetZones(zones.filter(z => z.id !== id));
  const patchZone  = (id: number, patch: Partial<TriggerZone>) =>
    onSetZones(zones.map(z => z.id === id ? { ...z, ...patch } : z));

  return (
    <div className="flex-1 min-h-0 flex flex-col overflow-hidden">

      {/* Triggers section */}
      <div className="flex items-center px-3 py-1.5 border-b border-game-border/50 flex-shrink-0">
        <span className="text-[10px] font-mono text-game-textDim tracking-widest flex-1">
          {triggers.length} TRIGGER{triggers.length !== 1 ? 'S' : ''}
        </span>
        <button onClick={addTrigger}
          className="text-[9px] font-mono text-game-primary border border-game-primary/40 rounded px-1.5 py-0.5 hover:bg-game-primary/10 transition-colors">
          + ADD
        </button>
      </div>

      <div className="flex-1 min-h-0 overflow-y-auto">
        {triggers.length === 0 && (
          <div className="px-3 py-4 text-[11px] text-game-muted font-mono text-center">
            No triggers.<br />Click + ADD to create one.
          </div>
        )}
        {triggers.map(node => (
          <TriggerNodeRow
            key={node.id}
            node={node}
            expanded={expandedId === node.id}
            onToggle={() => setExpandedId(prev => prev === node.id ? null : node.id)}
            onChange={patch => patchTrigger(node.id, patch)}
            onRemove={() => removeTrigger(node.id)}
            onRequestActorLink={onRequestActorLink}
          />
        ))}

        {/* Objectives section */}
        <div className="border-t border-game-border/50 mt-1">
          <button
            className="w-full flex items-center px-3 py-1.5 hover:bg-game-dark transition-colors"
            onClick={() => setObjSection(v => !v)}
          >
            <span className="text-[10px] font-mono text-game-textDim tracking-widest flex-1">
              {objectives.length} OBJECTIVE{objectives.length !== 1 ? 'S' : ''}
            </span>
            <span className="text-[9px] text-game-muted mr-2">{objSection ? '▲' : '▼'}</span>
            <span
              role="button"
              onClick={e => { e.stopPropagation(); addObjective(); }}
              className="text-[9px] font-mono text-game-primary border border-game-primary/40 rounded px-1.5 py-0.5 hover:bg-game-primary/10 transition-colors"
            >+ ADD</span>
          </button>

          {objSection && (
            <>
              {objectives.length === 0 && (
                <div className="px-3 py-2 text-[10px] text-game-muted font-mono">No objectives defined.</div>
              )}
              {objectives.map(obj => (
                <div key={obj.id} className="flex flex-col gap-1 px-3 py-1.5 border-b border-game-border/30">
                  <div className="flex items-center gap-1.5">
                    <span className="text-[9px] font-mono text-game-muted w-5">#{obj.id}</span>
                    <label className="flex items-center gap-1 flex-shrink-0">
                      <input type="checkbox" checked={obj.required}
                        onChange={e => patchObjective(obj.id, { required: e.target.checked })}
                        className="accent-yellow-500" />
                      <span className="text-[9px] font-mono text-game-muted">req</span>
                    </label>
                    <button onClick={() => removeObjective(obj.id)}
                      className="ml-auto text-game-muted hover:text-red-400 text-[10px] px-0.5">✕</button>
                  </div>
                  <input
                    type="text"
                    maxLength={127}
                    value={obj.text}
                    placeholder="Objective text…"
                    onChange={e => patchObjective(obj.id, { text: e.target.value })}
                    className={inp + ' w-full'}
                  />
                </div>
              ))}
            </>
          )}
        </div>

        {/* Zones section */}
        <div className="border-t border-game-border/50 mt-1">
          <button
            className="w-full flex items-center px-3 py-1.5 hover:bg-game-dark transition-colors"
            onClick={() => setZoneSection(v => !v)}
          >
            <span className="text-[10px] font-mono text-game-textDim tracking-widest flex-1">
              {zones.length} ZONE{zones.length !== 1 ? 'S' : ''}
            </span>
            <span className="text-[9px] text-game-muted mr-2">{zoneSection ? '▲' : '▼'}</span>
            <span
              role="button"
              onClick={e => { e.stopPropagation(); addZone(); }}
              className="text-[9px] font-mono text-game-primary border border-game-primary/40 rounded px-1.5 py-0.5 hover:bg-game-primary/10 transition-colors"
            >+ ADD</span>
          </button>

          {zoneSection && (
            <>
              {zones.length === 0 && (
                <div className="px-3 py-2 text-[10px] text-game-muted font-mono">No zones. Draw with TRIGGER ZONE tool or + ADD.</div>
              )}
              {zones.map(zone => (
                <div key={zone.id} className="flex flex-col gap-1 px-3 py-1.5 border-b border-game-border/30">
                  <div className="flex items-center gap-1.5">
                    <span className="text-[9px] font-mono text-[#00dcc8] w-6">#{zone.id}</span>
                    <input
                      type="text"
                      value={zone.label}
                      placeholder={`Zone ${zone.id}`}
                      onChange={e => patchZone(zone.id, { label: e.target.value })}
                      className={inp + ' flex-1 min-w-0'}
                    />
                    <button onClick={() => removeZone(zone.id)}
                      className="ml-auto text-game-muted hover:text-red-400 text-[10px] px-0.5 flex-shrink-0">✕</button>
                  </div>
                  <div className="flex gap-1 flex-wrap pl-6">
                    <span className="text-[8px] font-mono text-game-muted self-center">x1</span>
                    <input className={inp + ' w-14'} type="number" value={zone.x1} onChange={e => patchZone(zone.id, { x1: +e.target.value })} />
                    <span className="text-[8px] font-mono text-game-muted self-center">y1</span>
                    <input className={inp + ' w-14'} type="number" value={zone.y1} onChange={e => patchZone(zone.id, { y1: +e.target.value })} />
                    <span className="text-[8px] font-mono text-game-muted self-center">x2</span>
                    <input className={inp + ' w-14'} type="number" value={zone.x2} onChange={e => patchZone(zone.id, { x2: +e.target.value })} />
                    <span className="text-[8px] font-mono text-game-muted self-center">y2</span>
                    <input className={inp + ' w-14'} type="number" value={zone.y2} onChange={e => patchZone(zone.id, { y2: +e.target.value })} />
                  </div>
                </div>
              ))}
            </>
          )}
        </div>
      </div>
    </div>
  );
}
