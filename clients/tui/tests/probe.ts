#!/usr/bin/env bun
// Spawn silencer --tui, query state, palette index 0 & 100, dump first few
// pixels. Standalone — drives the smoke probe with no UI.

import { spawn } from 'bun';
import { createServer } from 'node:net';
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';

import { FrameStreamParser } from '../src/frame_parser';
import { ControlClient } from '../src/control_client';

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

async function listenPort(): Promise<number> {
  const s = createServer();
  await new Promise<void>((res) => s.listen(0, '127.0.0.1', () => res()));
  const a = s.address();
  if (!a || typeof a === 'string') throw new Error('bind');
  const p = a.port;
  s.close();
  return p;
}

async function main(): Promise<void> {
  const ctrlPort = await listenPort();
  const frameSrv = createServer();
  await new Promise<void>((res) => frameSrv.listen(0, '127.0.0.1', () => res()));
  const fa = frameSrv.address();
  if (!fa || typeof fa === 'string') throw new Error('bind');
  const framePort = fa.port;

  const parser = new FrameStreamParser();
  let frameCount = 0;

  frameSrv.on('connection', (sock) => {
    sock.on('data', (chunk: Buffer) => {
      const msgs = parser.push(new Uint8Array(chunk));
      for (const m of msgs) {
        if (m.type === 'frame') frameCount++;
      }
    });
  });

  const child = spawn({
    cmd: [findBinary(), '--tui', '--control-port', String(ctrlPort)],
    env: {
      ...process.env,
      SILENCER_TUI_FRAME_HOST: '127.0.0.1',
      SILENCER_TUI_FRAME_PORT: String(framePort),
    },
    stdout: 'inherit',
    stderr: 'inherit',
    stdin: 'ignore',
  });

  const ctrl = new ControlClient();
  for (let i = 0; i < 80; i++) {
    try {
      await ctrl.connect('127.0.0.1', ctrlPort);
      break;
    } catch {
      await Bun.sleep(150);
    }
  }
  console.error('[probe] control connected');

  // Probe state every second for 12 seconds.
  for (let i = 0; i < 12; i++) {
    await Bun.sleep(1000);
    try {
      const r = await ctrl.send('state');
      console.error(`[probe] t=${i + 1}s frames=${frameCount} state=${JSON.stringify(r)}`);
    } catch (e) {
      console.error(`[probe] t=${i + 1}s err=${e}`);
    }
  }

  ctrl.close();
  child.kill();
  frameSrv.close();
  process.exit(0);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
