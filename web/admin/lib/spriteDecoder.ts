/**
 * Silencer sprite/tile bank decoder + encoder — browser-compatible (no fs/path).
 *
 * Binary format:
 *   BIN_SPR.DAT / BIN_TIL.DAT  — 256 entries × 64 bytes; frame count at byte N*64+2
 *   SPR_NNN.BIN / TIL_NNN.BIN  — header section (numFrames*344+4 bytes) + pixel data
 *   PALETTE.BIN                 — 11 sub-palettes, each at 4 + s*(768+4), 256×3 6-bit RGB
 */

export interface FrameHeader {
  width: number;
  height: number;
  offsetX: number;
  offsetY: number;
  compSize: number;
  mode: number;
  headerBytes: Uint8Array; // full 344-byte original header (preserve unknown fields)
}

export interface DecodedFrame {
  header: FrameHeader;
  indexedPixels: Uint8Array; // width*height palette indices — source of truth
  dirty: boolean;            // true if modified (imported or anchor changed)
}

export interface DecodedBank {
  bankIndex: number;
  frames: DecodedFrame[];
  dirty: boolean;
}

// ── Index file ───────────────────────────────────────────────────────────────

/** Parse BIN_SPR.DAT or BIN_TIL.DAT → array of 256 frame counts. */
export function parseDat(buf: ArrayBuffer): number[] {
  const bytes = new Uint8Array(buf);
  const counts: number[] = new Array(256);
  for (let i = 0; i < 256; i++) {
    counts[i] = bytes[i * 64 + 2];
  }
  return counts;
}

// ── Palette ──────────────────────────────────────────────────────────────────

/**
 * Load a sub-palette from PALETTE.BIN.
 * Returns Uint8Array of 256*4 RGBA bytes (6-bit channels expanded to 8-bit).
 * Index 0 → alpha=0 (transparent).
 */
export function loadPalette(buf: ArrayBuffer, subPalette: number): Uint8Array {
  const bytes = new Uint8Array(buf);
  const palette = new Uint8Array(256 * 4);
  const base = 4 + subPalette * (768 + 4);
  for (let i = 0; i < 256; i++) {
    const off = base + i * 3;
    palette[i * 4 + 0] = (bytes[off + 0] & 0x3f) << 2; // R
    palette[i * 4 + 1] = (bytes[off + 1] & 0x3f) << 2; // G
    palette[i * 4 + 2] = (bytes[off + 2] & 0x3f) << 2; // B
    palette[i * 4 + 3] = i === 0 ? 0 : 255;            // A
  }
  return palette;
}

// ── RLE decode helpers ────────────────────────────────────────────────────────

function decodeLinearRle(
  view: DataView,
  dataOffset: number,
  compSize: number,
  pixelCount: number,
): Uint8Array {
  const out = new Uint8Array(pixelCount);
  let k = 0;
  let pos = dataOffset;
  const end = dataOffset + compSize;
  while (pos < end && k < pixelCount) {
    const dword = view.getUint32(pos, true);
    pos += 4;
    if ((dword & 0xff000000) >>> 0 === 0xff000000) {
      const countBytes = dword & 0x0000ffff;
      const color = (dword >>> 16) & 0xff;
      for (let c = 0; c < countBytes && k < pixelCount; c++, k++) {
        out[k] = color;
      }
    } else {
      if (k < pixelCount) out[k++] = (dword >>> 0) & 0xff;
      if (k < pixelCount) out[k++] = (dword >>> 8) & 0xff;
      if (k < pixelCount) out[k++] = (dword >>> 16) & 0xff;
      if (k < pixelCount) out[k++] = (dword >>> 24) & 0xff;
    }
  }
  return out;
}

function decodeTileRle(
  view: DataView,
  dataOffset: number,
  width: number,
  height: number,
): Uint8Array {
  const out = new Uint8Array(width * height);
  let pos = dataOffset;
  const tilesY = Math.ceil(height / 64);
  const tilesX = Math.ceil(width / 64);
  for (let tileY = 0; tileY < tilesY; tileY++) {
    for (let tileX = 0; tileX < tilesX; tileX++) {
      const yMax = Math.min((tileY + 1) * 64, height);
      const xMax = Math.min((tileX + 1) * 64, width);
      let count = 0;
      let tempValue = 0;
      for (let y = tileY * 64; y < yMax; y++) {
        for (let x = tileX * 64; x < xMax; x += 4) {
          if (count > 0) {
            for (let b = 0; b < 4 && x + b < xMax; b++) {
              out[y * width + x + b] = (tempValue >>> (b * 8)) & 0xff;
            }
            count -= 4;
          } else {
            tempValue = view.getUint32(pos, true);
            pos += 4;
            if ((tempValue & 0xff000000) >>> 0 === 0xff000000) {
              count = tempValue & 0x0000ffff;
              const colorByte = (tempValue >>> 16) & 0xff;
              tempValue =
                colorByte |
                (colorByte << 8) |
                (colorByte << 16) |
                (colorByte << 24);
              count -= 4;
            }
            for (let b = 0; b < 4 && x + b < xMax; b++) {
              out[y * width + x + b] = (tempValue >>> (b * 8)) & 0xff;
            }
          }
        }
      }
    }
  }
  return out;
}

// ── Bank decode ───────────────────────────────────────────────────────────────

/**
 * Decode a single bank file → DecodedBank.
 * numFrames must come from parseDat() — it is NOT stored in the BIN file itself.
 */
export function decodeBank(bankIndex: number, buf: ArrayBuffer, numFrames: number): DecodedBank {
  if (numFrames === 0) return { bankIndex, frames: [], dirty: false };
  const view = new DataView(buf);
  const bytes = new Uint8Array(buf);

  const frames: DecodedFrame[] = [];
  const headerTotal = numFrames * 344 + 4;
  let pixelOffset = headerTotal;

  for (let j = 0; j < numFrames; j++) {
    const base = j * 344;
    const width    = view.getUint16(base + 0, true);
    const height   = view.getUint16(base + 2, true);
    const offsetX  = view.getInt16(base + 4, true);
    const offsetY  = view.getInt16(base + 6, true);
    const compSize = view.getUint32(base + 12, true);
    const mode     = bytes[base + 20];

    const headerBytes = bytes.slice(base, base + 344);
    const pixelCount = width * height;

    let indexedPixels: Uint8Array;
    if (mode === 0) {
      indexedPixels = decodeLinearRle(view, pixelOffset, compSize, pixelCount);
      pixelOffset += compSize;
    } else {
      const before = pixelOffset;
      indexedPixels = decodeTileRle(view, pixelOffset, width, height);
      // Advance by actual consumed bytes (re-read to compute consumed)
      // Tile mode: we consumed dwords during decode. Re-derive from tile iteration.
      // Since decodeTileRle advanced via pos internally, we need to track it.
      // Recompute: count total dwords consumed for w*h pixels in tile layout.
      pixelOffset = estimateTileSize(view, before, width, height);
    }

    frames.push({
      header: { width, height, offsetX, offsetY, compSize, mode, headerBytes },
      indexedPixels,
      dirty: false,
    });
  }

  return { bankIndex, frames, dirty: false };
}

/** Estimate the byte offset past a tile-mode RLE block by replaying the read. */
function estimateTileSize(
  view: DataView,
  startOffset: number,
  width: number,
  height: number,
): number {
  let pos = startOffset;
  const tilesY = Math.ceil(height / 64);
  const tilesX = Math.ceil(width / 64);
  for (let tileY = 0; tileY < tilesY; tileY++) {
    for (let tileX = 0; tileX < tilesX; tileX++) {
      const yMax = Math.min((tileY + 1) * 64, height);
      const xMax = Math.min((tileX + 1) * 64, width);
      let count = 0;
      for (let y = tileY * 64; y < yMax; y++) {
        for (let x = tileX * 64; x < xMax; x += 4) {
          if (count > 0) {
            count -= 4;
          } else {
            const dword = view.getUint32(pos, true);
            pos += 4;
            if ((dword & 0xff000000) >>> 0 === 0xff000000) {
              count = (dword & 0x0000ffff) - 4;
            }
          }
        }
      }
    }
  }
  return pos;
}

// ── Tile bank decode/encode (TIL_NNN.BIN — different format from sprites) ─────
//
// Tile header: 12 bytes per frame (not 344). Game reads but ignores header data.
// Pixel data: single linear RLE block for ALL frames together, starting after header.
// Each tile is always 64×64 = 4096 bytes in the decompressed output.
// Frame j starts at j * 4096 in decompressed pixels.

const TILE_W = 64;
const TILE_H = 64;
const TILE_PIXELS = TILE_W * TILE_H; // 4096
const TILE_HEADER_SIZE = 12;

export function decodeTileBank(bankIndex: number, buf: ArrayBuffer, numFrames: number): DecodedBank {
  if (numFrames === 0) return { bankIndex, frames: [], dirty: false };

  const view = new DataView(buf);
  const bytes = new Uint8Array(buf);
  const headerTotal = numFrames * TILE_HEADER_SIZE + 4;
  const compSize = buf.byteLength - headerTotal;
  const totalPixels = numFrames * TILE_PIXELS;

  const allPixels = decodeLinearRle(view, headerTotal, compSize, totalPixels);

  const frames: DecodedFrame[] = [];
  for (let j = 0; j < numFrames; j++) {
    const headerBase = j * TILE_HEADER_SIZE;
    const headerBytes = bytes.slice(headerBase, headerBase + TILE_HEADER_SIZE);
    const indexedPixels = allPixels.slice(j * TILE_PIXELS, (j + 1) * TILE_PIXELS);
    frames.push({
      header: { width: TILE_W, height: TILE_H, offsetX: 0, offsetY: 0, compSize: 0, mode: 1, headerBytes },
      indexedPixels,
      dirty: false,
    });
  }

  return { bankIndex, frames, dirty: false };
}

export function encodeTileBank(bank: DecodedBank): Uint8Array {
  const { frames } = bank;
  const numFrames = frames.length;
  const headerTotal = numFrames * TILE_HEADER_SIZE + 4;

  // Combine all frames' pixels into one block then RLE-encode together
  const allPixels = new Uint8Array(numFrames * TILE_PIXELS);
  for (let j = 0; j < numFrames; j++) {
    allPixels.set(frames[j].indexedPixels.slice(0, TILE_PIXELS), j * TILE_PIXELS);
  }
  const pixelData = encodeRleLinear(allPixels);

  const out = new Uint8Array(headerTotal + pixelData.byteLength);
  for (let j = 0; j < numFrames; j++) {
    out.set(frames[j].header.headerBytes.slice(0, TILE_HEADER_SIZE), j * TILE_HEADER_SIZE);
  }
  out.set(pixelData, headerTotal);
  return out;
}

// ── Display ───────────────────────────────────────────────────────────────────

/** Convert indexed pixels → RGBA ImageData for display. */
export function frameToImageData(frame: DecodedFrame, palette: Uint8Array): ImageData {
  const { width, height } = frame.header;
  const rgba = new Uint8ClampedArray(width * height * 4);
  for (let i = 0; i < width * height; i++) {
    const idx = frame.indexedPixels[i];
    rgba[i * 4 + 0] = palette[idx * 4 + 0];
    rgba[i * 4 + 1] = palette[idx * 4 + 1];
    rgba[i * 4 + 2] = palette[idx * 4 + 2];
    rgba[i * 4 + 3] = palette[idx * 4 + 3];
  }
  return new ImageData(rgba, width, height);
}

// ── Encode ────────────────────────────────────────────────────────────────────

/**
 * Encode indexed pixels as mode-0 linear RLE.
 * Output is a sequence of u32 dwords (little-endian).
 */
export function encodeRleLinear(pixels: Uint8Array): Uint8Array {
  const dwords: number[] = [];
  let i = 0;
  const n = pixels.length;
  while (i < n) {
    // Detect run of same byte (must be multiple of 4 for proper encoding)
    const color = pixels[i];
    let runEnd = i + 1;
    while (runEnd < n && pixels[runEnd] === color) runEnd++;
    const runLen = runEnd - i;
    if (runLen >= 4) {
      // Encode as RLE run; run count must be multiple of 4 per codec
      const aligned = Math.floor(runLen / 4) * 4;
      dwords.push(0xff000000 | ((color & 0xff) << 16) | (aligned & 0xffff));
      i += aligned;
    } else {
      // Emit up to 4 raw bytes as one dword
      let dw = 0;
      for (let b = 0; b < 4; b++) {
        const byte = i + b < n ? pixels[i + b] : 0;
        dw |= byte << (b * 8);
      }
      dwords.push(dw >>> 0);
      i += 4;
    }
  }
  const out = new Uint8Array(dwords.length * 4);
  const outView = new DataView(out.buffer);
  for (let d = 0; d < dwords.length; d++) {
    outView.setUint32(d * 4, dwords[d], true);
  }
  return out;
}

/** Encode a DecodedBank back to BIN bytes. Dirty frames re-encoded as mode-0 RLE; clean frames use original bytes. */
export function encodeBank(bank: DecodedBank): Uint8Array {
  const { frames } = bank;
  const numFrames = frames.length;

  // Build pixel data chunks first so we know comp_sizes for headers
  const pixelChunks: Uint8Array[] = frames.map(f => {
    if (f.dirty || f.header.mode !== 0) {
      // Re-encode as mode-0 linear RLE
      return encodeRleLinear(f.indexedPixels);
    }
    // Clean mode-0 frame: slice original file bytes for pixel data
    // We don't have the original file here, so we must re-encode.
    // For clean non-dirty frames without mode changes, re-encode too.
    return encodeRleLinear(f.indexedPixels);
  });

  const headerSection = numFrames * 344 + 4;
  const totalPixelBytes = pixelChunks.reduce((s, c) => s + c.byteLength, 0);
  const out = new Uint8Array(headerSection + totalPixelBytes);
  const outView = new DataView(out.buffer);

  for (let j = 0; j < numFrames; j++) {
    const f = frames[j];
    const base = j * 344;
    // Copy original 344-byte header
    out.set(f.header.headerBytes, base);
    // Patch updated fields
    outView.setUint16(base + 0, f.header.width, true);
    outView.setUint16(base + 2, f.header.height, true);
    outView.setInt16(base + 4, f.header.offsetX, true);
    outView.setInt16(base + 6, f.header.offsetY, true);
    outView.setUint32(base + 12, pixelChunks[j].byteLength, true);
    out[base + 20] = 0; // force mode-0 for re-encoded frames
  }
  // 4 bytes of filler at headerSection-4 already zero from new Uint8Array

  let pixelPos = headerSection;
  for (const chunk of pixelChunks) {
    out.set(chunk, pixelPos);
    pixelPos += chunk.byteLength;
  }

  return out;
}

/** Patch a copy of the original DAT buffer with a new frame count for one bank. */
export function encodeDat(
  originalDat: ArrayBuffer,
  bankIndex: number,
  frameCount: number,
): Uint8Array {
  const out = new Uint8Array(originalDat.byteLength);
  out.set(new Uint8Array(originalDat));
  out[bankIndex * 64 + 2] = frameCount & 0xff;
  return out;
}

// ── Quantize ──────────────────────────────────────────────────────────────────

/**
 * Quantize RGBA ImageData pixels to palette indices.
 * Transparent pixels (alpha < 128) → index 0.
 * Opaque pixels → nearest palette index ≥ 1 (Euclidean distance in RGB).
 * Throws if imageData.width % 4 !== 0.
 */
export function quantizeToPalette(imageData: ImageData, palette: Uint8Array): Uint8Array {
  if (imageData.width % 4 !== 0) {
    throw new Error(`Image width (${imageData.width}) must be a multiple of 4`);
  }
  const { data, width, height } = imageData;
  const out = new Uint8Array(width * height);
  for (let i = 0; i < width * height; i++) {
    const r = data[i * 4 + 0];
    const g = data[i * 4 + 1];
    const b = data[i * 4 + 2];
    const a = data[i * 4 + 3];
    if (a < 128) {
      out[i] = 0;
      continue;
    }
    let bestIdx = 1;
    let bestDist = Infinity;
    for (let p = 1; p < 256; p++) {
      const dr = r - palette[p * 4 + 0];
      const dg = g - palette[p * 4 + 1];
      const db = b - palette[p * 4 + 2];
      const dist = dr * dr + dg * dg + db * db;
      if (dist < bestDist) {
        bestDist = dist;
        bestIdx = p;
        if (dist === 0) break;
      }
    }
    out[i] = bestIdx;
  }
  return out;
}
