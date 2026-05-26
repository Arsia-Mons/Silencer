'use client';
/**
 * C7: Actor properties panel — HP, speed, faction, footstep overrides, etc.
 * The save is handled by the parent (ActorEditorPage) via onChange.
 */
import { useEffect, useRef, useState, useMemo } from 'react';
import { type ActorDef, type ActorFootstepOverride } from '../../../lib/api';

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

/** Searchable cue picker matching the style from physics-materials and AnimationTab. */
function CuePicker({ value, cues, placeholder = '— material default —', onChange }: {
  value: string | undefined;
  cues: string[];
  placeholder?: string;
  onChange: (v: string | undefined) => void;
}) {
  const [open, setOpen] = useState(false);
  const [filter, setFilter] = useState('');
  const ref = useRef<HTMLDivElement>(null);
  const filtered = useMemo(
    () => cues.filter(c => c.toLowerCase().includes(filter.toLowerCase())),
    [cues, filter]
  );
  useEffect(() => {
    if (!open) return;
    function onDown(e: MouseEvent) {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false);
    }
    document.addEventListener('mousedown', onDown);
    return () => document.removeEventListener('mousedown', onDown);
  }, [open]);
  return (
    <div className="relative" ref={ref}>
      <button
        type="button"
        className="w-44 bg-game-bg border border-game-border px-2 py-1 text-xs font-mono text-left truncate hover:border-game-text"
        onClick={() => { setOpen(o => !o); setFilter(''); }}
        title={value}
      >
        {value
          ? <span className="text-game-primary">{value}</span>
          : <span className="text-game-textDim">{placeholder}</span>}
      </button>
      {open && (
        <div className="absolute z-50 top-full left-0 mt-0.5 w-56 bg-[#050a05] border border-game-border shadow-lg flex flex-col">
          <input
            autoFocus
            type="text"
            placeholder="filter..."
            className="bg-game-bg border-b border-game-border px-2 py-1 text-xs font-mono"
            value={filter}
            onChange={e => setFilter(e.target.value)}
          />
          <div className="overflow-y-auto max-h-56">
            <button
              type="button"
              className="w-full text-left px-2 py-1 text-xs text-game-textDim hover:bg-game-border/30"
              onClick={() => { onChange(undefined); setOpen(false); }}
            >{placeholder}</button>
            {filtered.map(c => (
              <button
                key={c}
                type="button"
                className={`w-full text-left px-2 py-1 text-xs font-mono hover:bg-game-border/30 ${c === value ? 'text-game-primary' : 'text-game-primary/70'}`}
                onClick={() => { onChange(c); setOpen(false); }}
              >{c}</button>
            ))}
          </div>
        </div>
      )}
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
  const footsteps: Record<string, ActorFootstepOverride> = (def.footsteps as Record<string, ActorFootstepOverride>) ?? {};

  const [cues, setCues] = useState<string[]>([]);
  const [materials, setMaterials] = useState<string[]>([]);
  const [addingMat, setAddingMat] = useState(false);
  const [matFilter, setMatFilter] = useState('');
  const addRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    fetch('/api/sound-cues')
      .then(r => r.ok ? r.json() : [])
      .then((data: unknown) => Array.isArray(data) ? setCues(data as string[]) : setCues([]))
      .catch(() => setCues([]));
    fetch('/api/physics-materials')
      .then(r => r.ok ? r.json() : [])
      .then((data: unknown) => {
        if (Array.isArray(data)) setMaterials((data as { id: string }[]).map(m => m.id));
      })
      .catch(() => setMaterials([]));
  }, []);

  useEffect(() => {
    if (!addingMat) return;
    function onDown(e: MouseEvent) {
      if (addRef.current && !addRef.current.contains(e.target as Node)) setAddingMat(false);
    }
    document.addEventListener('mousedown', onDown);
    return () => document.removeEventListener('mousedown', onDown);
  }, [addingMat]);

  function update(patch: Partial<ActorProps>) {
    onChange({ props: { ...props, ...patch } as import('../../../lib/api').ActorProps });
  }

  function updateOverride(mat: string, field: 'walkL' | 'walkR', value: string | undefined) {
    const ov = { ...footsteps[mat], [field]: value || undefined };
    if (!ov.walkL && !ov.walkR) {
      const { [mat]: _, ...rest } = footsteps;
      onChange({ footsteps: Object.keys(rest).length ? rest : undefined });
    } else {
      onChange({ footsteps: { ...footsteps, [mat]: ov } });
    }
  }

  function addMaterial(mat: string) {
    if (!footsteps[mat]) onChange({ footsteps: { ...footsteps, [mat]: {} } });
    setAddingMat(false);
    setMatFilter('');
  }

  function removeMaterial(mat: string) {
    const { [mat]: _, ...rest } = footsteps;
    onChange({ footsteps: Object.keys(rest).length ? rest : undefined });
  }

  const activeMaterials = Object.keys(footsteps);
  const unusedMaterials = materials.filter(m => !footsteps[m]);
  const filteredUnused = unusedMaterials.filter(m => m.toLowerCase().includes(matFilter.toLowerCase()));

  const seqCount = Object.keys((def.sequences as Record<string, unknown>) ?? {}).length;
  const totalFrames = Object.values((def.sequences as Record<string, { frames: unknown[] }>) ?? {})
    .reduce((s, seq) => s + (seq.frames?.length ?? 0), 0);

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
        Override footstep cues per physics material. Leave blank to use the material's default.
      </p>
      <div className="bg-game-bgCard border border-game-border p-6 mb-8 space-y-4">
        {activeMaterials.length === 0 && (
          <p className="text-xs text-game-textDim">No overrides set — all materials use their defaults.</p>
        )}
        {activeMaterials.map(mat => (
          <div key={mat} className="border border-game-border/50 p-3 space-y-2">
            <div className="flex items-center justify-between mb-1">
              <span className="text-xs font-mono text-game-primary tracking-widest">{mat}</span>
              <button
                type="button"
                className="text-xs text-game-textDim hover:text-red-400"
                onClick={() => removeMaterial(mat)}
              >✕ remove</button>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-xs text-game-textDim w-20">WALK LEFT</span>
              <CuePicker cues={cues} value={footsteps[mat]?.walkL} onChange={v => updateOverride(mat, 'walkL', v)} />
            </div>
            <div className="flex items-center justify-between">
              <span className="text-xs text-game-textDim w-20">WALK RIGHT</span>
              <CuePicker cues={cues} value={footsteps[mat]?.walkR} onChange={v => updateOverride(mat, 'walkR', v)} />
            </div>
          </div>
        ))}

        <div className="relative" ref={addRef}>
          <button
            type="button"
            className="text-xs text-game-textDim hover:text-game-primary border border-dashed border-game-border/50 px-3 py-1.5 w-full"
            onClick={() => { setAddingMat(o => !o); setMatFilter(''); }}
          >+ add material override</button>
          {addingMat && (
            <div className="absolute z-50 top-full left-0 mt-0.5 w-full bg-[#050a05] border border-game-border shadow-lg flex flex-col">
              <input
                autoFocus
                type="text"
                placeholder="filter materials..."
                className="bg-game-bg border-b border-game-border px-2 py-1 text-xs font-mono"
                value={matFilter}
                onChange={e => setMatFilter(e.target.value)}
              />
              <div className="overflow-y-auto max-h-48">
                {filteredUnused.length === 0 && (
                  <div className="px-2 py-1 text-xs text-game-textDim">All materials overridden</div>
                )}
                {filteredUnused.map(m => (
                  <button
                    key={m}
                    type="button"
                    className="w-full text-left px-2 py-1 text-xs font-mono text-game-primary/70 hover:bg-game-border/30"
                    onClick={() => addMaterial(m)}
                  >{m}</button>
                ))}
              </div>
            </div>
          )}
        </div>
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
