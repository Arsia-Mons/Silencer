// Module-level singleton — survives client-side navigation.
// Holds the currently open SilMapData so navigating away from the
// Map Designer and back restores the same map without re-opening the file.

import type { SilMapData } from './types';

let _data: SilMapData | null = null;

export function isLoaded(): boolean { return _data !== null; }

export function get(): SilMapData | null { return _data; }

export function set(data: SilMapData): void { _data = data; }

export function clear(): void { _data = null; }
