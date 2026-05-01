#!/usr/bin/env bun
// Smoke test: spawn silencer --tui, capture the first frame + palette, write
// /tmp/silencer-tui-frame.ppm. Verifies engine-side frame streaming.

import { spawn } from 'bun';
import { createServer } from 'node:net';
import { writeFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';

import { FrameStreamParser } from '../src/frame_parser';
import type { Frame, Palette } from '../src/frame_parser';

function findBinary(): string {
  if (process.env.SILENCER_BIN) return process.env.SILENCER_BIN;
  const candidates = [
    resolve(
      import.meta.dir,
      '../../silencer/build/Silencer.app/Contents/MacOS/Silencer',
    ),
    resolve(import.meta.dir, '../../silencer/build/silencer'),
  ];
  for (const c of candidates) if (existsSync(c)) return c;
  throw new Error('silencer binary not found');
}

async function main(): Promise<void> {
  const server = createServer();
  await new Promise<void>((res, rej) => {
    server.once('error', rej);
    server.listen(0, '127.0.0.1', () => res());
  });
  const addr = server.address();
  if (!addr || typeof addr === 'string') throw new Error('bind failed');
  const port = addr.port;

  let lastFrame: Frame | null = null;
  let lastPalette: Palette | null = null;
  let frameCount = 0;
  const parser = new FrameStreamParser();
  // Capture after the engine has stabilized in MAINMENU (lobby load + palette
  // fade-in finish within ~3 wall clock seconds at the engine's 24 fps cadence).
  const SETTLE_MS = 5000;
  let resolved = false;

  const done = new Promise<void>((resolveDone) => {
    server.on('connection', (sock) => {
      console.error(`[smoke] engine connected from frame port ${port}`);
      const start = performance.now();
      sock.on('data', (chunk: Buffer) => {
        const msgs = parser.push(new Uint8Array(chunk));
        for (const m of msgs) {
          if (m.type === 'palette') {
            lastPalette = m.palette;
          } else if (m.type === 'frame') {
            lastFrame = m.frame;
            frameCount++;
          }
        }
        if (
          lastPalette &&
          lastFrame &&
          performance.now() - start >= SETTLE_MS &&
          !resolved
        ) {
          resolved = true;
          resolveDone();
        }
      });
    });
  });

  // Open a control listener too so we can ask the engine what state it's in.
  const ctrlServer = createServer();
  await new Promise<void>((res) => {
    ctrlServer.listen(0, '127.0.0.1', () => res());
  });
  const ctrlAddr = ctrlServer.address();
  if (!ctrlAddr || typeof ctrlAddr === 'string') throw new Error('bind failed');
  const ctrlPort = ctrlAddr.port;
  ctrlServer.close();

  const bin = findBinary();
  console.error(`[smoke] spawning ${bin}`);
  const child = spawn({
    cmd: [bin, '--tui', '--control-port', String(ctrlPort)],
    env: {
      ...process.env,
      SILENCER_TUI_FRAME_HOST: '127.0.0.1',
      SILENCER_TUI_FRAME_PORT: String(port),
    },
    stdout: 'inherit',
    stderr: 'inherit',
    stdin: 'ignore',
  });

  // Poll engine state via control.
  setTimeout(async () => {
    const { ControlClient } = await import('../src/control_client');
    const c = new ControlClient();
    for (let i = 0; i < 40; i++) {
      try {
        await c.connect('127.0.0.1', ctrlPort);
        break;
      } catch {
        await Bun.sleep(100);
      }
    }
    const r = await c.send('state').catch((e: unknown) => ({ err: String(e) }));
    console.error(`[smoke] engine state: ${JSON.stringify(r)}`);
    c.close();
  }, 5000);

  const timeout = new Promise<void>((_, rej) =>
    setTimeout(() => rej(new Error('timeout waiting for frame')), 30000),
  );
  await Promise.race([done, timeout]);

  if (!lastFrame || !lastPalette) throw new Error('no frame captured');
  const frame: Frame = lastFrame;
  const palette: Palette = lastPalette;

  // Diagnostics: pixel-index histogram + a few palette entries.
  const hist = new Int32Array(256);
  for (const p of frame.pixels) hist[p]!++;
  const used = Array.from(hist.entries())
    .filter(([, n]) => n > 0)
    .sort((a, b) => b[1] - a[1])
    .slice(0, 10);
  console.error(`[smoke] frames=${frameCount}, top indices in last frame:`);
  for (const [idx, n] of used) {
    const r = palette[idx * 4]!;
    const g = palette[idx * 4 + 1]!;
    const b = palette[idx * 4 + 2]!;
    console.error(`  idx ${idx}: ${n} px, rgb(${r},${g},${b})`);
  }

  // Write PPM (P6 binary).
  const { w, h, pixels } = frame;
  const header = `P6\n${w} ${h}\n255\n`;
  const data = Buffer.alloc(header.length + w * h * 3);
  data.write(header, 0);
  let off = header.length;
  for (let i = 0; i < pixels.length; i++) {
    const idx = pixels[i]! * 4;
    data[off++] = palette[idx]!;
    data[off++] = palette[idx + 1]!;
    data[off++] = palette[idx + 2]!;
  }
  const out = '/tmp/silencer-tui-frame.ppm';
  writeFileSync(out, data);
  console.error(`[smoke] wrote ${out} (${w}x${h})`);

  child.kill();
  server.close();
  process.exit(0);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
