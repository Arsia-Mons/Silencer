/**
 * Module-level singleton holding the user's locally-opened behavior tree
 * directory handle (File System Access API). Persists for the lifetime of
 * the browser tab; survives client-side navigation between list and editor.
 *
 * When a handle is set the editor reads/writes local files directly,
 * bypassing the admin-api entirely — changes land straight in the git repo.
 */

let handle: FileSystemDirectoryHandle | null = null;

export function getFolderHandle(): FileSystemDirectoryHandle | null {
  return handle;
}

export async function openFolder(): Promise<FileSystemDirectoryHandle | null> {
  if (typeof window === 'undefined' || !('showDirectoryPicker' in window)) return null;
  try {
    handle = await (window as unknown as { showDirectoryPicker(): Promise<FileSystemDirectoryHandle> }).showDirectoryPicker();
    return handle;
  } catch {
    // User cancelled
    return null;
  }
}

export function clearFolder(): void {
  handle = null;
}

export async function listJsonFiles(dir: FileSystemDirectoryHandle): Promise<string[]> {
  const ids: string[] = [];
  for await (const [name] of dir.entries()) {
    if (name.endsWith('.json')) ids.push(name.slice(0, -5));
  }
  return ids.sort();
}

export async function readJson<T>(dir: FileSystemDirectoryHandle, id: string): Promise<T> {
  const file = await dir.getFileHandle(`${id}.json`);
  const f = await file.getFile();
  return JSON.parse(await f.text()) as T;
}

export async function writeJson(dir: FileSystemDirectoryHandle, id: string, data: unknown): Promise<void> {
  const file = await dir.getFileHandle(`${id}.json`, { create: true });
  const writable = await file.createWritable();
  await writable.write(JSON.stringify(data, null, 2));
  await writable.close();
}

export async function deleteJson(dir: FileSystemDirectoryHandle, id: string): Promise<void> {
  await dir.removeEntry(`${id}.json`);
}
