/**
 * In-memory store for actor definition JSON files loaded via
 * <input webkitdirectory> — works over plain HTTP (unlike the
 * File System Access API which requires HTTPS/localhost).
 *
 * Workflow:
 *   1. User clicks OPEN FOLDER → hidden input triggers directory picker
 *   2. All .json files are read into `store`
 *   3. Editor loads/saves from `store`
 *   4. SAVE also prompts a save-file dialog (HTTPS) or downloads the file
 *      so the user can drop it into the repo and commit
 */

import type { ActorDef } from './api';

const store = new Map<string, ActorDef>();
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
          try { store.set(f.name.slice(0, -5), JSON.parse(text) as ActorDef); }
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

export function readFromStore(id: string): ActorDef | null {
  return store.get(id) ?? null;
}

export function writeToStore(id: string, data: ActorDef): void {
  store.set(id, data);
}

export function deleteFromStore(id: string): void {
  store.delete(id);
}

/**
 * Save JSON to disk. On HTTPS (admin.arsiamons.com) uses showSaveFilePicker
 * so the user can choose the exact location (e.g. straight into the repo).
 * Falls back to a plain <a download> on plain HTTP.
 */
export async function downloadJson(id: string, data: unknown): Promise<void> {
  const json = JSON.stringify(data, null, 2);
  if (
    typeof window !== 'undefined' &&
    'showSaveFilePicker' in window
  ) {
    try {
      const handle = await (window as unknown as {
        showSaveFilePicker(opts: unknown): Promise<FileSystemFileHandle>;
      }).showSaveFilePicker({
        suggestedName: `${id}.json`,
        types: [{ description: 'JSON', accept: { 'application/json': ['.json'] } }],
      });
      const writable = await handle.createWritable();
      await writable.write(json);
      await writable.close();
      return;
    } catch {
      // User cancelled — don't fall through to auto-download
      return;
    }
  }
  // HTTP fallback — goes to default downloads folder
  const blob = new Blob([json], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `${id}.json`;
  a.click();
  URL.revokeObjectURL(url);
}
