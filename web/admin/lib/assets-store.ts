// Module-level singleton — survives client-side navigation.
// Holds raw binary ArrayBuffers from shared/assets/ (PALETTE.BIN, BIN_SPR.DAT,
// BIN_TIL.DAT, and per-bank SPR_*.BIN / TIL_*.BIN). Sprites page reads/writes here.

interface StoredTabAssets {
  datBuf:    ArrayBuffer;
  bankFiles: Map<number, ArrayBuffer>;
}

interface StoredAssets {
  folderName: string;
  paletteBuf: ArrayBuffer | null;
  sprites:    StoredTabAssets | null;
  tiles:      StoredTabAssets | null;
}

let _data: StoredAssets | null = null;

export function isLoaded(): boolean { return _data !== null; }

export function get(): StoredAssets | null { return _data; }

export function set(data: StoredAssets): void { _data = data; }

export function clear(): void { _data = null; }
