/**
 * In-memory store for behavior tree JSON files loaded via
 * <input webkitdirectory> — works over plain HTTP (unlike the
 * File System Access API which requires HTTPS/localhost).
 *
 * Workflow:
 *   1. User clicks OPEN FOLDER → hidden input triggers directory picker
 *   2. All .json files are read into `store`
 *   3. Editor loads/saves from `store`
 *   4. SAVE also downloads the file so the user can drop it into the
 *      repo and commit
 */

import type { BehaviorTree } from './api';

const store = new Map<string, BehaviorTree>();
let _folderName: string | null = null;

export function getFolderName(): string | null { return _folderName; }
export function isFolderLoaded(): boolean { return store.size > 0; }

export async function loadFilesIntoStore(files: FileList): Promise<void> {
  store.clear();
  const first = files[0] as { webkitRelativePath?: string } & File | undefined;
  if (first?.webkitRelativePath) {
    _folderName = first.webkitRelativePath.split('/')[0];
  } else if (first) {
    _folderName = 'local';
  }

  await Promise.all(
    Array.from(files)
      .filter(f => f.name.endsWith('.json'))
      .map(f =>
        f.text().then(text => {
          try { store.set(f.name.slice(0, -5), JSON.parse(text) as BehaviorTree); }
          catch { /* skip invalid JSON */ }
        })
      )
  );
}

export function clearStore(): void {
  store.clear();
  _folderName = null;
}

export function listIds(): string[] {
  return [...store.keys()].sort();
}

export function readFromStore(id: string): BehaviorTree | null {
  return store.get(id) ?? null;
}

export function writeToStore(id: string, data: BehaviorTree): void {
  store.set(id, data);
}

export function deleteFromStore(id: string): void {
  store.delete(id);
}

export function downloadJson(id: string, data: unknown): void {
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `${id}.json`;
  a.click();
  URL.revokeObjectURL(url);
}
