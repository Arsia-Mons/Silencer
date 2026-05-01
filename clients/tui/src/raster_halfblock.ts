// Half-block + truecolor rasterizer.
//
// Each terminal cell renders two vertical game pixels via the upper-half-block
// glyph U+2580 ('▀'): foreground = top pixel, background = bottom pixel.
// Source frame dims are nearest-neighbor downsampled to (cols, rows*2).
//
// Differential redraw: maintains the previously emitted (fg,bg) per cell and
// only repaints cells that changed. Cuts steady-state bandwidth from full-frame
// (~5 MB/frame) to whatever's actually moving.

import type { Frame, Palette } from './frame_parser';

const UPPER_HALF = '▀';

export interface RasterTarget {
  cols: number;
  rows: number;
}

export class HalfBlockRasterizer {
  private prevFg: Int32Array | null = null;
  private prevBg: Int32Array | null = null;
  private prevCols = 0;
  private prevRows = 0;

  reset(): void {
    this.prevFg = null;
    this.prevBg = null;
    this.prevCols = 0;
    this.prevRows = 0;
  }

  /** Render a frame. Returns the bytes to write to the terminal. */
  render(frame: Frame, palette: Palette, target: RasterTarget): Uint8Array {
    const { cols, rows } = target;
    if (cols <= 0 || rows <= 0) return new Uint8Array(0);

    const total = cols * rows;
    const fg = new Int32Array(total);
    const bg = new Int32Array(total);

    const fw = frame.w;
    const fh = frame.h;
    const px = frame.pixels;
    const pal = palette;

    // Precompute source X for each terminal column.
    const srcX = new Int32Array(cols);
    for (let cx = 0; cx < cols; cx++) {
      srcX[cx] = Math.min(fw - 1, Math.floor((cx * fw) / cols));
    }

    for (let cy = 0; cy < rows; cy++) {
      const srcYTop = Math.min(fh - 1, Math.floor((cy * 2 * fh) / (rows * 2)));
      const srcYBot = Math.min(
        fh - 1,
        Math.floor(((cy * 2 + 1) * fh) / (rows * 2)),
      );
      const rowBase = cy * cols;
      const topRow = srcYTop * fw;
      const botRow = srcYBot * fw;
      for (let cx = 0; cx < cols; cx++) {
        const sx = srcX[cx]!;
        const topIdx = px[topRow + sx]!;
        const botIdx = px[botRow + sx]!;
        // pal layout: [r,g,b,a] per index, 4 bytes each.
        const tBase = topIdx * 4;
        const bBase = botIdx * 4;
        const tColor =
          (pal[tBase]! << 16) | (pal[tBase + 1]! << 8) | pal[tBase + 2]!;
        const bColor =
          (pal[bBase]! << 16) | (pal[bBase + 1]! << 8) | pal[bBase + 2]!;
        fg[rowBase + cx] = tColor;
        bg[rowBase + cx] = bColor;
      }
    }

    // Build the diff-emit buffer.
    const parts: string[] = [];
    const sizeChanged =
      cols !== this.prevCols || rows !== this.prevRows || !this.prevFg;

    if (sizeChanged) {
      // Clear screen + home cursor when terminal dims change or first frame.
      parts.push('\x1b[2J\x1b[H');
    }

    let lastFg = -1;
    let lastBg = -1;
    let cursorAt = -1; // cell index where cursor is, or -1 if unknown
    let lastEmittedAt = -2;

    for (let i = 0; i < total; i++) {
      const f = fg[i]!;
      const b = bg[i]!;
      const prevFg = !sizeChanged ? this.prevFg![i]! : -1;
      const prevBg = !sizeChanged ? this.prevBg![i]! : -1;
      if (!sizeChanged && f === prevFg && b === prevBg) continue;

      // Need to position cursor if there's a gap.
      if (i !== lastEmittedAt + 1 || cursorAt !== i) {
        const cy = (i / cols) | 0;
        const cx = i - cy * cols;
        // CSI rows;cols H — 1-based.
        parts.push(`\x1b[${cy + 1};${cx + 1}H`);
        cursorAt = i;
      }

      if (f !== lastFg) {
        parts.push(
          `\x1b[38;2;${(f >> 16) & 0xff};${(f >> 8) & 0xff};${f & 0xff}m`,
        );
        lastFg = f;
      }
      if (b !== lastBg) {
        parts.push(
          `\x1b[48;2;${(b >> 16) & 0xff};${(b >> 8) & 0xff};${b & 0xff}m`,
        );
        lastBg = b;
      }
      parts.push(UPPER_HALF);
      lastEmittedAt = i;
      cursorAt = i + 1;
    }

    if (parts.length > 0) {
      // Reset SGR so any subsequent terminal output isn't tinted.
      parts.push('\x1b[0m');
    }

    this.prevFg = fg;
    this.prevBg = bg;
    this.prevCols = cols;
    this.prevRows = rows;

    return new TextEncoder().encode(parts.join(''));
  }
}
