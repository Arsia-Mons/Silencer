// Module-level singleton — survives client-side navigation.
// Holds the sound list and refs so navigating back to Sound Studio
// skips the API reload and the "upload sound.bin" prompt.

interface SoundEntry {
  name: string;
  storedLength: number | null;
  adpcmBytes: number | null;
  durationSec?: number;
  size?: number;
  source: 'bin' | 'staged';
  pendingDelete: boolean;
  pendingRenameTo: string | null;
}

interface SoundRef {
  inBin: boolean;
  cpp: boolean;
  actordefs: string[];
  role: string | null;
  loop: boolean;
  category: string | null;
  volumeCalls: { ctx: string; vol: number | string }[];
  fadeoutMs: number | null;
  soundSet: string | null;
}

interface SoundStudioData {
  sounds: SoundEntry[];
  refs: Record<string, SoundRef>;
}

let _data: SoundStudioData | null = null;

export function isLoaded(): boolean { return _data !== null; }

export function get(): SoundStudioData | null { return _data; }

export function set(data: SoundStudioData): void { _data = data; }

export function clear(): void { _data = null; }
