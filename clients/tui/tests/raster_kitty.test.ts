import { test, expect } from 'bun:test';
import { inflateSync } from 'node:zlib';
import { Buffer } from 'node:buffer';

import { KittyRasterizer } from '../src/raster_kitty';

function makeFrame(w: number, h: number): {
  frame: { w: number; h: number; pixels: Uint8Array };
  palette: Uint8Array;
} {
  // Two-color test pattern: idx 0 = red, idx 1 = blue. Half/half by row.
  const palette = new Uint8Array(256 * 4);
  palette[0] = 255;
  palette[1] = 0;
  palette[2] = 0;
  palette[3] = 255;
  palette[4] = 0;
  palette[5] = 0;
  palette[6] = 255;
  palette[7] = 255;
  const pixels = new Uint8Array(w * h);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      pixels[y * w + x] = y < h / 2 ? 0 : 1;
    }
  }
  return { frame: { w, h, pixels }, palette };
}

function decodeOutput(out: Uint8Array): {
  preamble: string;
  cursor: { row: number; col: number } | null;
  controls: string;
  pixels: Uint8Array | null;
} {
  const text = new TextDecoder('latin1').decode(out);
  // Optional clear+delete preamble.
  let preamble = '';
  let rest = text;
  const clearMatch = rest.match(/^\x1b\[2J\x1b\[H\x1b_Ga=d,d=A,q=2;\x1b\\/);
  if (clearMatch) {
    preamble = clearMatch[0];
    rest = rest.slice(preamble.length);
  }
  // Cursor position.
  let cursor: { row: number; col: number } | null = null;
  const csiMatch = rest.match(/^\x1b\[(\d+);(\d+)H/);
  if (csiMatch) {
    cursor = { row: parseInt(csiMatch[1]!, 10), col: parseInt(csiMatch[2]!, 10) };
    rest = rest.slice(csiMatch[0].length);
  }
  // Walk APC chunks. Each is \x1b_G<controls>;<payload>\x1b\\
  let controls = '';
  let b64 = '';
  const apcRe = /\x1b_G([^;]*);([^\x1b]*)\x1b\\/g;
  let firstControls = true;
  let match: RegExpExecArray | null;
  while ((match = apcRe.exec(rest)) !== null) {
    if (firstControls) {
      controls = match[1]!;
      firstControls = false;
    }
    b64 += match[2]!;
  }
  if (!b64) return { preamble, cursor, controls, pixels: null };
  const deflated = Buffer.from(b64, 'base64');
  const raw = inflateSync(deflated);
  return { preamble, cursor, controls, pixels: new Uint8Array(raw) };
}

test('emits clear+delete preamble on first frame', () => {
  const r = new KittyRasterizer();
  const { frame, palette } = makeFrame(640, 480);
  const out = r.render(frame, palette, { cols: 200, rows: 60 });
  const decoded = decodeOutput(out);
  expect(decoded.preamble).toContain('\x1b[2J\x1b[H');
  expect(decoded.preamble).toContain('a=d,d=A');
});

test('omits clear preamble on subsequent same-size frames', () => {
  const r = new KittyRasterizer();
  const { frame, palette } = makeFrame(640, 480);
  r.render(frame, palette, { cols: 200, rows: 60 });
  const out = r.render(frame, palette, { cols: 200, rows: 60 });
  const decoded = decodeOutput(out);
  expect(decoded.preamble).toBe('');
});

test('re-emits preamble after reset()', () => {
  const r = new KittyRasterizer();
  const { frame, palette } = makeFrame(640, 480);
  r.render(frame, palette, { cols: 200, rows: 60 });
  r.reset();
  const out = r.render(frame, palette, { cols: 200, rows: 60 });
  const decoded = decodeOutput(out);
  expect(decoded.preamble).toContain('a=d,d=A');
});

test('controls advertise replace-image-and-placement (i=1, p=1) with C=1', () => {
  const r = new KittyRasterizer();
  const { frame, palette } = makeFrame(640, 480);
  const out = r.render(frame, palette, { cols: 200, rows: 60 });
  const decoded = decodeOutput(out);
  expect(decoded.controls).toContain('a=T');
  expect(decoded.controls).toContain('i=1');
  expect(decoded.controls).toContain('p=1');
  expect(decoded.controls).toContain('C=1');
  expect(decoded.controls).toContain('q=2');
  expect(decoded.controls).toContain('o=z');
  expect(decoded.controls).toContain('f=24');
  expect(decoded.controls).toContain('s=640');
  expect(decoded.controls).toContain('v=480');
});

test('payload decodes back to source RGB pixels', () => {
  const r = new KittyRasterizer();
  const { frame, palette } = makeFrame(640, 480);
  const out = r.render(frame, palette, { cols: 200, rows: 60 });
  const decoded = decodeOutput(out);
  expect(decoded.pixels).not.toBeNull();
  expect(decoded.pixels!.length).toBe(640 * 480 * 3);
  // Top-left pixel = idx 0 = red (255,0,0).
  expect(decoded.pixels![0]).toBe(255);
  expect(decoded.pixels![1]).toBe(0);
  expect(decoded.pixels![2]).toBe(0);
  // Bottom-left pixel = idx 1 = blue (0,0,255).
  const lastRowStart = 479 * 640 * 3;
  expect(decoded.pixels![lastRowStart]).toBe(0);
  expect(decoded.pixels![lastRowStart + 1]).toBe(0);
  expect(decoded.pixels![lastRowStart + 2]).toBe(255);
});

test('default stretch-to-fill: c, r match the full terminal viewport', () => {
  const r = new KittyRasterizer();
  const { frame, palette } = makeFrame(640, 480);
  const out = r.render(frame, palette, { cols: 200, rows: 60 });
  const decoded = decodeOutput(out);
  expect(decoded.controls).toContain('c=200');
  expect(decoded.controls).toContain('r=60');
  expect(decoded.cursor).toEqual({ row: 1, col: 1 });
});

test('aspect-preserving opt-in: 4:3 source in wide terminal letterboxes', () => {
  // stretchToFill: false → preserve aspect. 200 cols × 60 rows, cell 2:1.
  // Fill-width path: r = floor(200 * 480 / (640 * 2)) = 75 > 60, so use
  // fill-height: c = floor(60 * 640 * 2 / 480) = 160, r = 60.
  const r = new KittyRasterizer({ stretchToFill: false });
  const { frame, palette } = makeFrame(640, 480);
  const out = r.render(frame, palette, { cols: 200, rows: 60 });
  const decoded = decodeOutput(out);
  expect(decoded.controls).toContain('c=160');
  expect(decoded.controls).toContain('r=60');
  expect(decoded.cursor).toEqual({ row: 1, col: 21 });
});

test('chunks payloads larger than 4096 bytes into multiple APC messages', () => {
  const r = new KittyRasterizer();
  const { frame, palette } = makeFrame(640, 480);
  const out = r.render(frame, palette, { cols: 200, rows: 60 });
  const text = new TextDecoder('latin1').decode(out);
  const apcCount = (text.match(/\x1b_G/g) ?? []).length;
  // One delete-all preamble APC + N data APCs. Even with deflate, 640x480
  // RGB compresses to >>4096 base64 chars for non-trivial content.
  expect(apcCount).toBeGreaterThan(2);
  // No data chunk after the first carries the heavy controls.
  const heavyControls = text.match(/\x1b_Ga=T[^;]*;/g) ?? [];
  expect(heavyControls.length).toBe(1);
});

test('returns empty buffer for zero-sized terminal', () => {
  const r = new KittyRasterizer();
  const { frame, palette } = makeFrame(640, 480);
  expect(r.render(frame, palette, { cols: 0, rows: 60 }).length).toBe(0);
  expect(r.render(frame, palette, { cols: 200, rows: 0 }).length).toBe(0);
});
