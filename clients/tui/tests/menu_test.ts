#!/usr/bin/env bun
// Drive menu navigation via the new `key` op. Verifies that arrow keys move
// focus and Enter activates buttons. Reports state changes so we can see the
// engine actually responding to keyboard input.

import { spawn } from 'bun';
import { createServer } from 'node:net';
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';

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
  frameSrv.on('connection', (s) => s.on('data', () => {}));

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
  console.error('[menu] control connected');

  // Wait for MAINMENU.
  for (let i = 0; i < 40; i++) {
    const r = (await ctrl.send('state').catch(() => null)) as
      | { state: string; current_interface_id: number }
      | null;
    if (r && r.state === 'MAINMENU') {
      console.error(`[menu] reached MAINMENU after ${i * 200} ms`);
      break;
    }
    await Bun.sleep(200);
  }

  async function inspect(): Promise<string> {
    const r = (await ctrl.send('inspect')) as {
      widgets: Array<{ kind: string; label?: string; id: number }>;
    };
    return r.widgets
      .filter((w) => w.kind === 'button')
      .map((w) => `[${w.id}]${w.label}`)
      .join(', ');
  }

  console.error(`[menu] buttons: ${await inspect()}`);
  const stateBefore = (await ctrl.send('state')) as { state: string };
  console.error(`[menu] state before: ${stateBefore.state}`);

  // Press DOWN a few times to move focus, then ENTER.
  for (let i = 0; i < 2; i++) {
    await ctrl.send('key', { key: 'down' });
    await Bun.sleep(100);
  }
  console.error('[menu] sent 2x DOWN, sending ENTER');
  await ctrl.send('key', { key: 'enter' });

  // Watch state change.
  for (let i = 0; i < 20; i++) {
    await Bun.sleep(200);
    const r = (await ctrl.send('state').catch(() => null)) as
      | { state: string }
      | null;
    if (r && r.state !== stateBefore.state) {
      console.error(`[menu] state changed to ${r.state} after ${i * 200} ms`);
      break;
    }
  }
  const stateAfter = (await ctrl.send('state')) as { state: string };
  console.error(`[menu] state after: ${stateAfter.state}`);
  console.error(
    stateAfter.state !== stateBefore.state
      ? '[menu] PASS: keyboard navigation works'
      : '[menu] FAIL: state unchanged after DOWN+ENTER',
  );

  ctrl.close();
  child.kill();
  frameSrv.close();
  process.exit(stateAfter.state !== stateBefore.state ? 0 : 1);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
