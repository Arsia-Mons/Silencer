// Module-level singleton — survives client-side navigation.
// Caches decoded AudioBuffer objects so sounds played in Sound Studio or
// the Weapons tool don't need to be re-fetched and re-decoded on re-mount.

const _decoded = new Map<string, AudioBuffer>();

export function has(name: string): boolean { return _decoded.has(name); }

export function get(name: string): AudioBuffer | undefined { return _decoded.get(name); }

export function set(name: string, buf: AudioBuffer): void { _decoded.set(name, buf); }

export function clear(): void { _decoded.clear(); }
