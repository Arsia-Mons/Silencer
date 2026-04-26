/**
 * Silencer sprite binary decoder.
 *
 * Decodes the game's custom RLE-compressed sprite format from:
 *   BIN_SPR.DAT   — index of sprite banks (256 × 64-byte entries)
 *   bin_spr/SPR_XXX.BIN — per-bank sprite data
 *   PALETTE.BIN   — 11 palettes × 256 RGB entries (6-bit channels)
 *
 * Returns raw RGBA pixel buffers suitable for PNG encoding.
 */

import { readFileSync } from 'fs';
import { join } from 'path';

let cachedPalette = null;
let cachedBankIndex = null; // array[256] of frame counts

/** Load palette 0 (default) from PALETTE.BIN.  Returns Uint8Array of 256*4 RGBA bytes. */
function loadPalette(assetsDir) {
  if (cachedPalette) return cachedPalette;
  const buf = readFileSync(join(assetsDir, 'PALETTE.BIN'));
  // Each palette starts at (paletteIndex * (768+4)) + 4
  // Palette 0 → offset 4
  const palette = new Uint8Array(256 * 4);
  const base = 4; // palette 0 offset
  for (let i = 0; i < 256; i++) {
    const off = base + i * 3;
    palette[i * 4 + 0] = (buf[off + 0] & 0x3f) << 2; // R
    palette[i * 4 + 1] = (buf[off + 1] & 0x3f) << 2; // G
    palette[i * 4 + 2] = (buf[off + 2] & 0x3f) << 2; // B
    palette[i * 4 + 3] = i === 0 ? 0 : 255;           // A (color 0 = transparent)
  }
  cachedPalette = palette;
  return palette;
}

/** Load and cache the bank index from BIN_SPR.DAT. Returns array[256] of frame counts. */
function loadBankIndex(assetsDir) {
  if (cachedBankIndex) return cachedBankIndex;
  const buf = readFileSync(join(assetsDir, 'BIN_SPR.DAT'));
  const counts = new Array(256);
  for (let i = 0; i < 256; i++) {
    counts[i] = buf[i * 64 + 2]; // byte 2 of 64-byte header = frame count
  }
  cachedBankIndex = counts;
  return counts;
}

/** Metadata for all frames in a bank. */
export function getBankMetadata(assetsDir, bank) {
  const counts = loadBankIndex(assetsDir);
  if (bank < 0 || bank >= 256) throw new Error(`Bank ${bank} out of range`);
  const numFrames = counts[bank];
  if (numFrames === 0) return [];

  const path = join(assetsDir, 'bin_spr', `SPR_${String(bank).padStart(3, '0')}.BIN`);
  const buf = readFileSync(path);

  const frames = [];
  for (let j = 0; j < numFrames; j++) {
    const base = j * 344;
    const width   = buf.readUInt16LE(base + 0);
    const height  = buf.readUInt16LE(base + 2);
    const offsetX = buf.readInt16LE(base + 4);
    const offsetY = buf.readInt16LE(base + 6);
    frames.push({ frame: j, width, height, offsetX, offsetY });
  }
  return frames;
}

/**
 * Decode a single frame from a bank and return RGBA pixel data.
 * @returns {{ width, height, offsetX, offsetY, rgba: Uint8Array }}
 */
export function decodeSpriteFrame(assetsDir, bank, frame) {
  const counts = loadBankIndex(assetsDir);
  if (bank < 0 || bank >= 256) throw new Error(`Bank ${bank} out of range`);
  const numFrames = counts[bank];
  if (numFrames === 0 || frame < 0 || frame >= numFrames) {
    throw new Error(`Frame ${frame} out of range for bank ${bank} (${numFrames} frames)`);
  }

  const path = join(assetsDir, 'bin_spr', `SPR_${String(bank).padStart(3, '0')}.BIN`);
  const buf = readFileSync(path);

  // Parse per-frame header (344 bytes each)
  const headerBase = frame * 344;
  const width    = buf.readUInt16LE(headerBase + 0);
  const height   = buf.readUInt16LE(headerBase + 2);
  const offsetX  = buf.readInt16LE(headerBase + 4);
  const offsetY  = buf.readInt16LE(headerBase + 6);
  const size     = buf.readUInt32LE(headerBase + 12); // (j*86+3)*4 = j*344+12
  const useOffsets = buf[headerBase + 20] !== 0;

  // Pixel data begins after ALL frame headers
  const headerTotal = numFrames * 344 + 4;

  // Seek to this frame's pixel data: sum preceding frames' compressed sizes
  let dataOffset = headerTotal;
  for (let k = 0; k < frame; k++) {
    const kSize = buf.readUInt32LE(k * 344 + 12);
    dataOffset += kSize;
  }

  // Decompress pixels (8-bit palette indices, width*height output)
  const decompressed = new Uint8Array(width * height);

  if (useOffsets) {
    // Block-based RLE (64×64 tiles)
    let pos = dataOffset;
    for (let tileY = 0; tileY < Math.ceil(height / 64); tileY++) {
      for (let tileX = 0; tileX < Math.ceil(width / 64); tileX++) {
        const yMax = Math.min((tileY + 1) * 64, height);
        const xMax = Math.min((tileX + 1) * 64, width);
        let count = 0;
        let tempValue = 0;
        for (let y = tileY * 64; y < yMax; y++) {
          for (let x = tileX * 64; x < xMax; x += 4) {
            if (count > 0) {
              for (let b = 0; b < 4 && x + b < xMax; b++) {
                decompressed[y * width + x + b] = (tempValue >> (b * 8)) & 0xff;
              }
              count -= 4;
            } else {
              tempValue = buf.readUInt32LE(pos);
              pos += 4;
              if (tempValue >= 0xFF000000) {
                count = (tempValue & 0x0000FFFF);
                const colorByte = (tempValue >> 16) & 0xff;
                tempValue = colorByte | (colorByte << 8) | (colorByte << 16) | (colorByte << 24);
                count -= 4;
              }
              for (let b = 0; b < 4 && x + b < xMax; b++) {
                decompressed[y * width + x + b] = (tempValue >> (b * 8)) & 0xff;
              }
            }
          }
        }
      }
    }
  } else {
    // Linear RLE — each encoded Uint32 is either:
    //   >= 0xFF000000: RLE run (count = low 16 bits in bytes, color = bits 16-23)
    //   otherwise:     4 raw palette bytes
    let k = 0;
    let pos = dataOffset;
    const end = dataOffset + size;
    while (pos < end && k < width * height) {
      const tempValue = buf.readUInt32LE(pos);
      pos += 4;
      if (tempValue >= 0xFF000000) {
        const countBytes = tempValue & 0x0000FFFF;
        const colorByte = (tempValue >> 16) & 0xff;
        for (let c = 0; c < countBytes && k < width * height; c++, k++) {
          decompressed[k] = colorByte;
        }
      } else {
        if (k < width * height) decompressed[k++] = (tempValue >> 0) & 0xff;
        if (k < width * height) decompressed[k++] = (tempValue >> 8) & 0xff;
        if (k < width * height) decompressed[k++] = (tempValue >> 16) & 0xff;
        if (k < width * height) decompressed[k++] = (tempValue >> 24) & 0xff;
      }
    }
  }

  // Convert 8-bit indexed → RGBA using palette
  const palette = loadPalette(assetsDir);
  const rgba = new Uint8Array(width * height * 4);
  for (let i = 0; i < width * height; i++) {
    const idx = decompressed[i];
    rgba[i * 4 + 0] = palette[idx * 4 + 0];
    rgba[i * 4 + 1] = palette[idx * 4 + 1];
    rgba[i * 4 + 2] = palette[idx * 4 + 2];
    rgba[i * 4 + 3] = palette[idx * 4 + 3];
  }

  return { width, height, offsetX, offsetY, rgba };
}

/** Summary of all banks: array of { bank, frames } for banks with at least 1 frame. */
export function getAllBanks(assetsDir) {
  const counts = loadBankIndex(assetsDir);
  return counts.map((frames, bank) => ({ bank, frames })).filter(b => b.frames > 0);
}
