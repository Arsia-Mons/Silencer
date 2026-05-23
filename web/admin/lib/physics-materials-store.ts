/**
 * Module-level singleton for physics_materials.json data.
 * Survives client-side navigation so the user only needs to open the file once.
 */

let _text: string | null = null;
let _fileName: string | null = null;

export function isLoaded(): boolean { return _text !== null; }
export function getFileName(): string | null { return _fileName; }
export function getText(): string | null { return _text; }

export function load(fileName: string, text: string): void {
  _fileName = fileName;
  _text = text;
}

export function setText(text: string): void { _text = text; }

export function clear(): void {
  _text = null;
  _fileName = null;
}
