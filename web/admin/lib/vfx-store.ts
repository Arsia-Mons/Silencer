// Effect definition store — mirrors the C++ EffectDef struct in gasloader.h.
// Admin tool for authoring shared/assets/gas/effects.json.
// Same load/save pattern as other GAS admin tools.

export interface EffectDef {
  id: string;
  name: string;
  description?: string;
  bank: number;       // sprite bank index (matches game's res_bank)
  frames: number[];   // frame indices in playback order
  fps: number;        // playback speed (informational — C++ integration pending)
  loop: boolean;
  pingPong: boolean;  // reverse at end before looping
}

export const DEFAULT_EFFECT: Omit<EffectDef, 'id' | 'name'> = {
  bank: 0,
  frames: [0],
  fps: 12,
  loop: false,
  pingPong: false,
};

interface Store {
  folderName: string | null;
  effects: EffectDef[];
}

const store: Store = { folderName: null, effects: [] };

export function isLoaded(): boolean { return store.folderName !== null; }
export function getFolderName(): string | null { return store.folderName; }
export function listAll(): EffectDef[] { return store.effects; }
export function getById(id: string): EffectDef | undefined {
  return store.effects.find(e => e.id === id);
}

export function loadFromJson(folderName: string, json: string): void {
  const data = JSON.parse(json) as { effects?: EffectDef[] };
  store.folderName = folderName;
  store.effects = data.effects ?? [];
}

export function clear(): void {
  store.folderName = null;
  store.effects = [];
}

export function setEffect(effect: EffectDef): void {
  const idx = store.effects.findIndex(e => e.id === effect.id);
  if (idx >= 0) store.effects[idx] = effect;
  else store.effects.push(effect);
}

export function removeEffect(id: string): void {
  store.effects = store.effects.filter(e => e.id !== id);
}

export function addEffect(effect: EffectDef): void {
  store.effects.push(effect);
}

export function toJson(): string {
  return JSON.stringify(
    { _comment: 'Effect definitions — Silencer GAS data. Mirrors C++ EffectDef in gasloader.h.', effects: store.effects },
    null,
    2
  );
}

export function downloadJson(filename = 'effects.json'): void {
  const blob = new Blob([toJson()], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = filename; a.click();
  URL.revokeObjectURL(url);
}
