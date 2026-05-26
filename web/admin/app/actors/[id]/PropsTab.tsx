'use client';
/**
 * C7: Actor properties panel — HP, speed, faction, footstep overrides, etc.
 * The save is handled by the parent (ActorEditorPage) via onChange.
 */
import { useEffect, useState } from 'react';
import { type ActorDef, type ActorFootsteps } from '../../../lib/api';

interface ActorProps {
  hp?: number;
  shield?: number;
  speed?: number;
  faction?: string;
  spawnWeight?: number;
}

function getProps(def: ActorDef): ActorProps {
  return (def.props as ActorProps) ?? {};
}

function NumField({
  label, value, onChange, min, max,
}: {
  label: string; value: number | undefined; onChange: (v: number) => void; min?: number; max?: number;
}) {
  return (
    <div className="flex items-center justify-between">
      <label className="text-xs text-game-textDim tracking-widest w-28">{label}</label>
      <input
        type="number"
        min={min}
        max={max}
        className="w-28 bg-game-bg border border-game-border px-3 py-1.5 text-sm font-mono text-right focus:outline-none focus:border-game-primary"
        value={value ?? ''}
        onChange={e => onChange(+e.target.value)}
      />
    </div>
  );
}

function TextField({
  label, value, onChange,
}: {
  label: string; value: string | undefined; onChange: (v: string) => void;
}) {
  return (
    <div className="flex items-center justify-between">
      <label className="text-xs text-game-textDim tracking-widest w-28">{label}</label>
      <input
        type="text"
        className="w-44 bg-game-bg border border-game-border px-3 py-1.5 text-sm font-mono focus:outline-none focus:border-game-primary"
        value={value ?? ''}
        onChange={e => onChange(e.target.value)}
      />
    </div>
  );
}

export default function PropsTab({
  def, onChange,
}: {
  def: ActorDef;
  onChange: (patch: Partial<ActorDef>) => void;
}) {
  const props = getProps(def);
  const footsteps: ActorFootsteps = (def.footsteps as ActorFootsteps) ?? {};

  const [cues, setCues] = useState<string[]>([]);
  useEffect(() => {
    fetch('/api/sound-cues')
      .then(r => r.ok ? r.json() : [])
      .then((data: unknown) => Array.isArray(data) ? setCues(data as string[]) : setCues([]))
      .catch(() => setCues([]));
  }, []);

  function update(patch: Partial<ActorProps>) {
    onChange({ props: { ...props, ...patch } as import('../../../lib/api').ActorProps });
  }

  function updateFootstep(key: keyof ActorFootsteps, value: string) {
    onChange({ footsteps: { ...footsteps, [key]: value || undefined } });
  }

  const seqCount = Object.keys((def.sequences as Record<string, unknown>) ?? {}).length;
  const totalFrames = Object.values((def.sequences as Record<string, { frames: unknown[] }>) ?? {})
    .reduce((s, seq) => s + (seq.frames?.length ?? 0), 0);

  const FOOTSTEP_FIELDS: { key: keyof ActorFootsteps; label: string }[] = [
    { key: 'walkL',   label: 'WALK LEFT'    },
    { key: 'walkR',   label: 'WALK RIGHT'   },
    { key: 'crouchL', label: 'CROUCH LEFT'  },
    { key: 'crouchR', label: 'CROUCH RIGHT' },
    { key: 'stairL',  label: 'STAIR LEFT'   },
    { key: 'stairR',  label: 'STAIR RIGHT'  },
  ];

  return (
    <div className="p-8 max-w-2xl">
      <h2 className="text-sm font-bold tracking-widest text-game-primary mb-6">ACTOR PROPERTIES</h2>

      <div className="bg-game-bgCard border border-game-border p-6 space-y-4 mb-8">
        <NumField label="HP"           value={props.hp}          onChange={v => update({ hp: v })}          min={1} />
        <NumField label="SHIELD"       value={props.shield}      onChange={v => update({ shield: v })}      min={0} />
        <NumField label="SPEED"        value={props.speed}       onChange={v => update({ speed: v })}       min={0} />
        <NumField label="SPAWN WEIGHT" value={props.spawnWeight} onChange={v => update({ spawnWeight: v })} min={0} max={100} />
        <TextField label="FACTION"    value={props.faction}     onChange={v => update({ faction: v })} />
      </div>

      <h2 className="text-sm font-bold tracking-widest text-game-primary mb-4">FOOTSTEP OVERRIDES</h2>
      <p className="text-xs text-game-textDim mb-4">
        Override physics material footstep sounds for this actor. Leave blank to use the material default.
      </p>
      <div className="bg-game-bgCard border border-game-border p-6 space-y-4 mb-8">
        {FOOTSTEP_FIELDS.map(({ key, label }) => (
          <div key={key} className="flex items-center justify-between">
            <label className="text-xs text-game-textDim tracking-widest w-28">{label}</label>
            <select
              className="w-52 bg-game-bg border border-game-border px-3 py-1.5 text-sm font-mono focus:outline-none focus:border-game-primary"
              value={footsteps[key] ?? ''}
              onChange={e => updateFootstep(key, e.target.value)}
            >
              <option value="">— material default —</option>
              {cues.map(cue => (
                <option key={cue} value={cue}>{cue}</option>
              ))}
              {footsteps[key] && !cues.includes(footsteps[key]!) && (
                <option value={footsteps[key]}>{footsteps[key]} ⚠ legacy</option>
              )}
            </select>
          </div>
        ))}
      </div>

      <h2 className="text-sm font-bold tracking-widest text-game-primary mb-4">SUMMARY</h2>
      <div className="bg-game-bgCard border border-game-border p-6 space-y-2 font-mono text-sm">
        <div className="flex justify-between">
          <span className="text-game-textDim">Actor ID</span>
          <span>{String(def.id ?? '—')}</span>
        </div>
        <div className="flex justify-between">
          <span className="text-game-textDim">Sequences</span>
          <span>{seqCount}</span>
        </div>
        <div className="flex justify-between">
          <span className="text-game-textDim">Total frames</span>
          <span>{totalFrames}</span>
        </div>
      </div>

      <div className="mt-6 text-xs text-game-textDim">
        <p>Changes are saved when you click SAVE in the top bar.</p>
        <p className="mt-1">The JSON is written to <code>shared/assets/actordefs/{String(def.id ?? 'actor')}.json</code> and loaded by the game client at startup.</p>
      </div>
    </div>
  );
}
