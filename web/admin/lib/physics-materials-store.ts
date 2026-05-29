/**
 * Module-level singleton for physics_materials/ folder data.
 * Survives client-side navigation so the user only needs to open the folder once.
 */

const _files = new Map<string, string>(); // material id → raw JSON text
let _folderName: string | null = null;

export function isLoaded(): boolean { return _folderName !== null; }
export function getFolderName(): string | null { return _folderName; }

export function loadFolder(name: string, fileMap: Record<string, string>): void {
  _folderName = name;
  _files.clear();
  for (const [k, v] of Object.entries(fileMap)) _files.set(k, v);
}

export function getFile(id: string): string | null { return _files.get(id) ?? null; }
export function setFile(id: string, text: string): void { _files.set(id, text); }
export function getAllFiles(): Record<string, string> { return Object.fromEntries(_files); }

export function clear(): void {
  _files.clear();
  _folderName = null;
}
