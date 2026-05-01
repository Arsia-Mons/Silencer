// Sprite Animation store — define frame sequences that map to how the game
// actually renders effects (sprite bank + frame index cycling).
// Same load/save pattern as other admin tools.

export interface SpriteAnimation {
  id: string;
  name: string;
  description?: string;
  bank: number;       // sprite bank index (matches game's res_bank)
  frames: number[];   // frame indices in playback order
  fps: number;        // playback speed in frames per second
  loop: boolean;
  pingPong: boolean;  // reverse at end before looping
  usedBy?: string;    // free-text note, e.g. "rocket explosion"
}

export const DEFAULT_ANIM: Omit<SpriteAnimation, 'id' | 'name'> = {
  bank: 0,
  frames: [0],
  fps: 12,
  loop: true,
  pingPong: false,
  usedBy: '',
};

interface Store {
  folderName: string | null;
  animations: SpriteAnimation[];
}

const store: Store = { folderName: null, animations: [] };

export function isLoaded(): boolean { return store.folderName !== null; }
export function getFolderName(): string | null { return store.folderName; }
export function listAll(): SpriteAnimation[] { return store.animations; }
export function getById(id: string): SpriteAnimation | undefined {
  return store.animations.find(a => a.id === id);
}

export function loadFromJson(folderName: string, json: string): void {
  const data = JSON.parse(json) as { animations?: SpriteAnimation[] };
  store.folderName = folderName;
  store.animations = data.animations ?? [];
}

export function clear(): void {
  store.folderName = null;
  store.animations = [];
}

export function setAnimation(anim: SpriteAnimation): void {
  const idx = store.animations.findIndex(a => a.id === anim.id);
  if (idx >= 0) store.animations[idx] = anim;
  else store.animations.push(anim);
}

export function removeAnimation(id: string): void {
  store.animations = store.animations.filter(a => a.id !== id);
}

export function addAnimation(anim: SpriteAnimation): void {
  store.animations.push(anim);
}

export function toJson(): string {
  return JSON.stringify({ _comment: 'Sprite animations — Silencer admin tool', animations: store.animations }, null, 2);
}

export function downloadJson(filename = 'sprite-animations.json'): void {
  const blob = new Blob([toJson()], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = filename; a.click();
  URL.revokeObjectURL(url);
}
