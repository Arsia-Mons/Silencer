// Module-level singleton — survives client-side navigation.
// Holds the processed Map Designer output (ImageBitmap maps) so navigating
// away and back skips the expensive processData progress-bar phase.

import type { SpriteEntry } from './types';

interface GameData {
  tileImages:     Map<number, ImageBitmap[]>;
  spriteImages:   Map<number, (SpriteEntry | null)[]>;
  tileBankCounts: Map<number, number>;
}

let _data: GameData | null = null;

export function isLoaded(): boolean { return _data !== null; }

export function get(): GameData | null { return _data; }

export function set(data: GameData): void { _data = data; }

export function clear(): void { _data = null; }
