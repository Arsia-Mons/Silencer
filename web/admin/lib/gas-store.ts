/**
 * Module-level singleton for GAS folder data.
 *
 * Survives client-side navigation (browser memory) so the user only needs to
 * open the folder once — switching between /gas, /weapons, and any other GAS
 * tool does not require re-picking the folder.
 *
 * Pages interact via:
 *   - isLoaded() — check on mount, skip folder picker if true
 *   - loadFolder(name, fileMap) — call when folder picker runs
 *   - getFile(name) / setFile(name, text) — read/write individual files
 *   - clear() — call when the user explicitly closes the folder (✕)
 */

const _files = new Map<string, string>(); // filename (no .json) → raw JSON text
let _folderName: string | null = null;

export function isLoaded(): boolean { return _folderName !== null; }
export function getFolderName(): string | null { return _folderName; }

export function loadFolder(name: string, fileMap: Record<string, string>): void {
  _folderName = name;
  _files.clear();
  for (const [k, v] of Object.entries(fileMap)) _files.set(k, v);
}

export function getFile(name: string): string | null { return _files.get(name) ?? null; }
export function setFile(name: string, text: string): void { _files.set(name, text); }

export function getAllFiles(): Record<string, string> {
  return Object.fromEntries(_files);
}

export function clear(): void {
  _files.clear();
  _folderName = null;
}
