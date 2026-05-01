'use client';
import { useState } from 'react';
import * as vfxStore from '../../lib/vfx-store';

export interface VFXTrigger {
  id: string;
  presetId: string;
  event: 'onMapStart' | 'onEnter' | 'onExit' | 'onTimer' | 'onPlayerDeath';
  x: number;
  y: number;
  radius: number;
  delay: number;
  loop: boolean;
  loopInterval: number;
  oneShot: boolean;
  label?: string;
}

const EVENT_LABELS: Record<VFXTrigger['event'], string> = {
  onMapStart:    'Map Start',
  onEnter:       'Player Enters Zone',
  onExit:        'Player Exits Zone',
  onTimer:       'Timer',
  onPlayerDeath: 'Player Death',
};

const INPUT = 'bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 w-full focus:border-[#00a328] outline-none';
const LABEL = 'text-[9px] font-mono text-[#4a7a4a] tracking-widest uppercase';
const SELECT = INPUT + ' cursor-pointer';

function newTrigger(): VFXTrigger {
  return {
    id: `trig-${Date.now().toString(36)}`,
    presetId: '',
    event: 'onMapStart',
    x: 0, y: 0, radius: 64,
    delay: 0,
    loop: false,
    loopInterval: 1,
    oneShot: true,
    label: '',
  };
}

interface Props {
  triggers: VFXTrigger[];
  onChange: (triggers: VFXTrigger[]) => void;
  onDownload: () => void;
  mapLoaded: boolean;
}

export default function VFXTriggersPanel({ triggers, onChange, onDownload, mapLoaded }: Props) {
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const presets = vfxStore.listAll();
  const vfxLoaded = vfxStore.isLoaded();

  const selected = triggers.find(t => t.id === selectedId) ?? null;

  function add() {
    const t = newTrigger();
    if (presets.length > 0) t.presetId = presets[0].id;
    onChange([...triggers, t]);
    setSelectedId(t.id);
  }

  function remove(id: string) {
    onChange(triggers.filter(t => t.id !== id));
    if (selectedId === id) setSelectedId(triggers.find(t => t.id !== id)?.id ?? null);
  }

  function patch(partial: Partial<VFXTrigger>) {
    if (!selected) return;
    onChange(triggers.map(t => t.id === selected.id ? { ...t, ...partial } : t));
  }

  function patchNum(key: keyof VFXTrigger, val: string) {
    patch({ [key]: val === '' ? 0 : parseFloat(val) } as Partial<VFXTrigger>);
  }

  if (!mapLoaded) {
    return (
      <div className="flex-1 flex items-center justify-center p-4">
        <span className="text-[10px] font-mono text-[#2a4a2a] text-center">Open a map to manage VFX triggers</span>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full overflow-hidden text-[#d1fad7]">
      {/* Header */}
      <div className="px-3 py-2 border-b border-[#1a2e1a] flex items-center justify-between shrink-0">
        <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">VFX TRIGGERS ({triggers.length})</span>
        <div className="flex gap-1">
          <button onClick={add}
            className="text-[10px] font-mono px-1.5 py-0.5 border border-[#1a2e1a] text-[#4a7a4a] hover:text-[#00a328] hover:border-[#00a328] transition-colors">
            + ADD
          </button>
          <button onClick={onDownload}
            className="text-[10px] font-mono px-1.5 py-0.5 border border-[#1a2e1a] text-[#4a7a4a] hover:text-[#00a328] hover:border-[#00a328] transition-colors">
            ↓ SAVE
          </button>
        </div>
      </div>

      {!vfxLoaded && (
        <div className="px-3 py-2 border-b border-[#f59e0b]/30 shrink-0">
          <p className="text-[9px] font-mono text-[#f59e0b]">
            ⚠ Open vfx-presets.json in the VFX Editor to link presets
          </p>
        </div>
      )}

      {/* Trigger list */}
      <div className="flex-1 overflow-y-auto">
        {triggers.length === 0 ? (
          <div className="p-4 text-[10px] font-mono text-[#2a4a2a]">
            No triggers. Click + ADD to create one.
          </div>
        ) : (
          triggers.map(t => (
            <button key={t.id}
              onClick={() => setSelectedId(t.id === selectedId ? null : t.id)}
              className={`w-full text-left px-3 py-2 border-b border-[#1a2e1a] transition-colors ${
                t.id === selectedId ? 'bg-[#00a328] text-black' : 'hover:bg-[#0a180a]'
              }`}>
              <div className="text-xs font-mono truncate">{t.label || t.id}</div>
              <div className={`text-[10px] font-mono ${t.id === selectedId ? 'text-black/60' : 'text-[#4a7a4a]'}`}>
                {EVENT_LABELS[t.event]} · {t.presetId || '(no preset)'}
              </div>
            </button>
          ))
        )}
      </div>

      {/* Editor for selected trigger */}
      {selected && (
        <div className="border-t border-[#1a2e1a] p-3 flex flex-col gap-3 overflow-y-auto max-h-[55%] shrink-0">
          <div className="flex items-center justify-between">
            <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">EDIT TRIGGER</span>
            <button onClick={() => remove(selected.id)}
              className="text-[9px] font-mono text-[#4a7a4a] hover:text-red-400 border border-[#1a2e1a] hover:border-red-400 px-1.5 py-0.5 transition-colors">
              ✕ DELETE
            </button>
          </div>

          <label className="flex flex-col gap-1">
            <span className={LABEL}>LABEL</span>
            <input type="text" className={INPUT} value={selected.label ?? ''}
              onChange={e => patch({ label: e.target.value })} placeholder={selected.id} />
          </label>

          <label className="flex flex-col gap-1">
            <span className={LABEL}>VFX PRESET</span>
            <select className={SELECT} value={selected.presetId}
              onChange={e => patch({ presetId: e.target.value })}>
              <option value="">— select preset —</option>
              {presets.map(p => (
                <option key={p.id} value={p.id}>{p.name} ({p.id})</option>
              ))}
              {!vfxLoaded && selected.presetId && (
                <option value={selected.presetId}>{selected.presetId} (preset file not loaded)</option>
              )}
            </select>
          </label>

          <label className="flex flex-col gap-1">
            <span className={LABEL}>EVENT</span>
            <select className={SELECT} value={selected.event}
              onChange={e => patch({ event: e.target.value as VFXTrigger['event'] })}>
              {(Object.entries(EVENT_LABELS) as [VFXTrigger['event'], string][]).map(([v, l]) => (
                <option key={v} value={v}>{l}</option>
              ))}
            </select>
          </label>

          {/* Zone coords — only relevant for zone events */}
          {(selected.event === 'onEnter' || selected.event === 'onExit') && (
            <div className="flex flex-col gap-2">
              <span className={LABEL}>ZONE (world px)</span>
              <div className="grid grid-cols-3 gap-2">
                <label className="flex flex-col gap-1">
                  <span className={LABEL}>X</span>
                  <input type="number" className={INPUT} value={selected.x}
                    onChange={e => patchNum('x', e.target.value)} />
                </label>
                <label className="flex flex-col gap-1">
                  <span className={LABEL}>Y</span>
                  <input type="number" className={INPUT} value={selected.y}
                    onChange={e => patchNum('y', e.target.value)} />
                </label>
                <label className="flex flex-col gap-1">
                  <span className={LABEL}>RADIUS</span>
                  <input type="number" className={INPUT} value={selected.radius}
                    onChange={e => patchNum('radius', e.target.value)} />
                </label>
              </div>
            </div>
          )}

          <div className="grid grid-cols-2 gap-2">
            <label className="flex flex-col gap-1">
              <span className={LABEL}>DELAY (s)</span>
              <input type="number" step="0.1" className={INPUT} value={selected.delay}
                onChange={e => patchNum('delay', e.target.value)} />
            </label>
            {selected.loop && (
              <label className="flex flex-col gap-1">
                <span className={LABEL}>LOOP INTERVAL (s)</span>
                <input type="number" step="0.1" className={INPUT} value={selected.loopInterval}
                  onChange={e => patchNum('loopInterval', e.target.value)} />
              </label>
            )}
          </div>

          <div className="flex gap-4">
            <label className="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" checked={selected.loop}
                onChange={e => patch({ loop: e.target.checked })}
                className="accent-[#00a328]" />
              <span className="text-[10px] font-mono text-[#4a7a4a]">Loop</span>
            </label>
            <label className="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" checked={selected.oneShot}
                onChange={e => patch({ oneShot: e.target.checked })}
                className="accent-[#00a328]" />
              <span className="text-[10px] font-mono text-[#4a7a4a]">One-shot</span>
            </label>
          </div>

          <div className="border-t border-[#1a2e1a] pt-2">
            <p className="text-[9px] font-mono text-[#2a4a2a]">
              ℹ Triggers are saved to a companion <code className="text-[#4a7a4a]">.sil.meta.json</code> sidecar file.
              Game client support requires C++ integration (tracked separately).
            </p>
          </div>
        </div>
      )}
    </div>
  );
}
