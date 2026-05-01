// In-memory store for VFX presets — load from local JSON file, edit, download.
// Same pattern as gas-store: no server persistence, all local.

export interface VFXPreset {
  id: string;
  name: string;
  description?: string;
  effectType: 'particles' | 'sprite-flash' | 'screen-shake';

  // Particle emitter params (effectType === 'particles')
  emissionRate: number;    // particles per second (continuous mode)
  burstCount: number;      // >0 = one-shot burst of N particles, 0 = continuous
  particleLifetime: number; // seconds each particle lives
  startSize: number;       // px at birth
  endSize: number;         // px at death
  startColor: string;      // hex e.g. '#ff8800'
  endColor: string;
  startAlpha: number;      // 0-1
  endAlpha: number;
  speed: number;           // px/sec base
  speedVariance: number;   // ± random added to speed
  spread: number;          // degrees (360 = omnidirectional, 0 = single direction)
  angle: number;           // base emit angle degrees (0 = up)
  gravity: number;         // px/sec² downward acceleration
  duration: number;        // total effect run time seconds (0 = infinite/looping)

  // Sprite flash params (effectType === 'sprite-flash')
  flashColor?: string;
  flashDuration?: number;  // seconds

  // Screen shake params (effectType === 'screen-shake')
  shakeIntensity?: number;
  shakeDuration?: number;
}

export const DEFAULT_PRESET: Omit<VFXPreset, 'id' | 'name'> = {
  effectType: 'particles',
  emissionRate: 30,
  burstCount: 0,
  particleLifetime: 0.6,
  startSize: 6,
  endSize: 1,
  startColor: '#ff8800',
  endColor: '#440000',
  startAlpha: 1.0,
  endAlpha: 0.0,
  speed: 80,
  speedVariance: 30,
  spread: 360,
  angle: 0,
  gravity: 40,
  duration: 0,
};

interface Store {
  folderName: string | null;
  presets: VFXPreset[];
}

const store: Store = { folderName: null, presets: [] };

export function isLoaded(): boolean { return store.folderName !== null; }
export function getFolderName(): string | null { return store.folderName; }
export function listAll(): VFXPreset[] { return store.presets; }
export function getById(id: string): VFXPreset | undefined {
  return store.presets.find(p => p.id === id);
}

export function loadFromJson(folderName: string, json: string): void {
  const data = JSON.parse(json) as { presets?: VFXPreset[] };
  store.folderName = folderName;
  store.presets = data.presets ?? [];
}

export function clear(): void {
  store.folderName = null;
  store.presets = [];
}

export function setPreset(preset: VFXPreset): void {
  const idx = store.presets.findIndex(p => p.id === preset.id);
  if (idx >= 0) store.presets[idx] = preset;
  else store.presets.push(preset);
}

export function removePreset(id: string): void {
  store.presets = store.presets.filter(p => p.id !== id);
}

export function addPreset(preset: VFXPreset): void {
  store.presets.push(preset);
}

export function toJson(): string {
  return JSON.stringify({ _comment: 'VFX presets — Silencer admin tool', presets: store.presets }, null, 2);
}

export function downloadJson(filename = 'vfx-presets.json'): void {
  const blob = new Blob([toJson()], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = filename; a.click();
  URL.revokeObjectURL(url);
}
