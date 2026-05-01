// Kitty graphics protocol rasterizer.
//
// Emits the engine's 640x480 indexed framebuffer as a single image per frame
// using the APC \x1b_G...\x1b\\ envelope. Uses image id=1 with placement id=1
// so each frame's transmission *replaces* the prior image data and placement
// in-place — no flicker, GPU swap on the terminal side. Compression is
// zlib/deflate (`o=z`) at fastest level; payload is base64 chunked at 4096
// bytes per APC message (kitty's per-message limit).
//
// Aspect-preserving fit: image is scaled to fill the terminal viewport while
// preserving 4:3, assuming a cell aspect of 1:CELL_ASPECT (w:h). Cursor is
// positioned to center the image, then C=1 keeps the cursor pinned so the
// next frame's positioning is well-defined.
//
// Size-change handling: on resize (or first frame), emit \x1b[2J + delete-all
// (a=d,d=A) before placing, so a stale image at the old c,r doesn't linger
// behind the new one. Steady state is just a cursor-move + one image
// transmission per frame.

import { deflateSync, constants as zconst } from 'node:zlib';
import { Buffer } from 'node:buffer';

import type { Frame, Palette } from './frame_parser';

export interface RasterTarget {
  cols: number;
  rows: number;
}

// Fallback cell aspect (height / width) when the terminal didn't answer the
// CSI 16t pixel-size query. Most monospace fonts land between 1.8 and 2.4;
// 2.0 is the safe middle. Override via SILENCER_TUI_CELL_ASPECT.
const DEFAULT_CELL_ASPECT = 2.0;

// Kitty graphics protocol caps each APC message payload at 4096 bytes.
const APC_CHUNK = 4096;

function fallbackCellAspect(): number {
  const env = process.env.SILENCER_TUI_CELL_ASPECT;
  if (!env) return DEFAULT_CELL_ASPECT;
  const v = parseFloat(env);
  return Number.isFinite(v) && v > 0 ? v : DEFAULT_CELL_ASPECT;
}

export interface KittyRasterizerOptions {
  /** Measured cell pixel dimensions from CSI 16t. When provided we use the
   *  exact aspect; when null we fall back to DEFAULT_CELL_ASPECT. Only
   *  consulted when stretchToFill is false. */
  cellPixelSize?: { width: number; height: number } | null;
  /** Stretch image to the full terminal viewport, ignoring source aspect.
   *  Default true — wide modern terminals letterbox the 4:3 framebuffer
   *  hard otherwise. Opt back into aspect preservation by passing false or
   *  setting SILENCER_TUI_KITTY_FILL=0. */
  stretchToFill?: boolean;
}

export class KittyRasterizer {
  private prevCols = 0;
  private prevRows = 0;
  private rgbBuf: Uint8Array | null = null;
  private rgbCap = 0;
  private readonly aspect: number;
  private readonly stretchToFill: boolean;
  /** Last computed placement (cells). Useful for diagnostics. */
  lastPlacement: {
    cols: number;
    rows: number;
    offsetCol: number;
    offsetRow: number;
  } | null = null;

  constructor(opts: KittyRasterizerOptions = {}) {
    const cps = opts.cellPixelSize ?? null;
    this.aspect =
      cps && cps.width > 0 && cps.height > 0
        ? cps.height / cps.width
        : fallbackCellAspect();
    // Default fill-to-viewport. Opt out via SILENCER_TUI_KITTY_FILL=0 (or
     // by passing { stretchToFill: false } from a test). Pixel art does get
     // mildly distorted on wide terminals — but that's vastly preferable to
     // the small letterboxed image users see at native 4:3.
    this.stretchToFill =
      opts.stretchToFill ?? process.env.SILENCER_TUI_KITTY_FILL !== '0';
  }

  reset(): void {
    // Force the next render to treat itself as a size-change: emit clear +
    // delete-all so any stale image at old (c,r) is cleared.
    this.prevCols = 0;
    this.prevRows = 0;
  }

  /** Render a frame. Returns the bytes to write to the terminal. */
  render(frame: Frame, palette: Palette, target: RasterTarget): Uint8Array {
    const { cols, rows } = target;
    if (cols <= 0 || rows <= 0) return new Uint8Array(0);

    const fw = frame.w;
    const fh = frame.h;
    const npix = fw * fh;
    const need = npix * 3;
    if (!this.rgbBuf || this.rgbCap < need) {
      this.rgbBuf = new Uint8Array(need);
      this.rgbCap = need;
    }
    const rgb = this.rgbBuf;
    const px = frame.pixels;
    const pal = palette;
    for (let i = 0, o = 0; i < npix; i++, o += 3) {
      const base = px[i]! * 4;
      rgb[o] = pal[base]!;
      rgb[o + 1] = pal[base + 1]!;
      rgb[o + 2] = pal[base + 2]!;
    }

    // Pick the cell box. Two modes:
    //   - stretchToFill: c=cols, r=rows. Image fills viewport, distorted.
    //   - aspect-preserve (default): pick c, r so the box matches the source
    //     aspect in pixel space. cell_aspect = cell_h / cell_w. Image pixel
    //     aspect = fw/fh. In cell space, an aspect-correct image is
    //     (fw / fh) * cell_aspect cells wide per cell tall. Try fill-width
    //     first; fall back to fill-height if that overflows.
    let placeC: number;
    let placeR: number;
    if (this.stretchToFill) {
      placeC = cols;
      placeR = rows;
    } else {
      placeC = cols;
      placeR = Math.max(1, Math.floor((cols * fh) / (fw * this.aspect)));
      if (placeR > rows) {
        placeR = rows;
        placeC = Math.max(1, Math.floor((rows * fw * this.aspect) / fh));
        if (placeC > cols) placeC = cols;
      }
    }
    const offsetC = ((cols - placeC) / 2) | 0;
    const offsetR = ((rows - placeR) / 2) | 0;
    this.lastPlacement = { cols: placeC, rows: placeR, offsetCol: offsetC, offsetRow: offsetR };

    const parts: Uint8Array[] = [];
    const enc = new TextEncoder();
    const sizeChanged = cols !== this.prevCols || rows !== this.prevRows;

    if (sizeChanged) {
      // Clear screen + delete all stored images. Without the delete, a stale
      // placement at the old (c,r) sits behind the new one until the terminal
      // happens to scroll it away.
      parts.push(enc.encode('\x1b[2J\x1b[H\x1b_Ga=d,d=A,q=2;\x1b\\'));
    }

    // Position cursor for centering. CSI is 1-based.
    parts.push(enc.encode(`\x1b[${offsetR + 1};${offsetC + 1}H`));

    // Compress + base64. Level 1 (fastest) keeps per-frame CPU well under our
    // 42 ms tick budget on M-class hardware while still cutting wire bytes
    // ~5-10x for the engine's paletted, blocky graphics.
    const deflated = deflateSync(rgb, { level: zconst.Z_BEST_SPEED });
    const b64 = Buffer.from(
      deflated.buffer,
      deflated.byteOffset,
      deflated.byteLength,
    ).toString('base64');

    let pos = 0;
    let first = true;
    while (pos < b64.length || first) {
      const end = Math.min(pos + APC_CHUNK, b64.length);
      const slice = b64.slice(pos, end);
      const more = end < b64.length ? 1 : 0;
      let controls: string;
      if (first) {
        // a=T transmit+display, f=24 RGB, s/v source pixel dims, c/r cell
        // box (kitty scales source to fit), i=1 image id, p=1 placement id
        // (replaces prior placement), C=1 don't move cursor, q=2 silence
        // responses, o=z payload is zlib-deflated, m=1 more chunks coming.
        controls =
          `a=T,q=2,f=24,o=z,s=${fw},v=${fh},c=${placeC},r=${placeR},` +
          `i=1,p=1,C=1,m=${more}`;
        first = false;
      } else {
        controls = `m=${more}`;
      }
      parts.push(enc.encode(`\x1b_G${controls};${slice}\x1b\\`));
      pos = end;
      if (more === 0) break;
    }

    this.prevCols = cols;
    this.prevRows = rows;

    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let off = 0;
    for (const p of parts) {
      out.set(p, off);
      off += p.length;
    }
    return out;
  }
}

/** Sequence to clean up any kitty graphics state on shutdown. Safe no-op on
 *  terminals without graphics support — APC envelopes are silently swallowed
 *  by xterm-compatible parsers. */
export const KITTY_CLEANUP = '\x1b_Ga=d,d=A,q=2;\x1b\\';
