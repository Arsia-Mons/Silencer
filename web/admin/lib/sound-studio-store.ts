// Module-level singleton — survives client-side navigation.
// Holds sound list, refs, and the parsed sound.bin so navigating back to
// Sound Studio restores state without re-picking the folder.

export interface SoundEntry {
  name: string;
  storedLength: number | null;
  adpcmBytes: number | null;
  durationSec?: number;
  size?: number;
  source: 'bin' | 'staged';
  pendingDelete: boolean;
  pendingRenameTo: string | null;
}

export interface SoundRef {
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

export interface ParsedBinEntry {
  name: string;
  offset: number;
  storedLength: number;
}

export interface StoredParsedBin {
  entries: ParsedBinEntry[];
  buf: ArrayBuffer;
  dataBase: number;
}

interface SoundStudioData {
  folderName: string;
  sounds: SoundEntry[];
  refs: Record<string, SoundRef>;
  parsedBin: StoredParsedBin | null;
}

let _data: SoundStudioData | null = null;

export function isLoaded(): boolean { return _data !== null; }

export function get(): SoundStudioData | null { return _data; }

export function set(data: SoundStudioData): void { _data = data; }

export function clear(): void { _data = null; }

export function getFolderName(): string | null { return _data?.folderName ?? null; }
