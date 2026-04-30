#!/usr/bin/env bun
// silencer-tui: spawns a Silencer headless engine in TUI mode and renders the
// resulting framebuffer in a terminal. End-to-end shape:
//
//   1. Allocate two free TCP ports: one for control (JSON-line, existing) and
//      one for framebuffer streaming (binary, new).
//   2. Listen on both. Spawn the silencer binary with --tui --control-port P1
//      and SILENCER_TUI_FRAME_HOST/PORT pointing at P2.
//   3. The engine's TUIBackend connects back to the frame port and starts
//      writing palette + frame messages. We rasterize each into the terminal.
//   4. stdin keypresses → InputState → JSON "input" op shipped over the
//      control socket every tick (24 Hz).
//
// Exit cleanly on Ctrl-C / SIGINT — the term.ts handlers restore the cursor +
// alt screen so the user's terminal isn't stranded.

import { spawn, type Subprocess } from 'bun';
import { createServer, type Server } from 'node:net';
import { once } from 'node:events';
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';

import { FrameStreamParser } from './frame_parser';
import type { Palette } from './frame_parser';
import { HalfBlockRasterizer } from './raster_halfblock';
import { ControlClient } from './control_client';
import { TerminalInput } from './input';
import * as term from './term';

const TICK_MS = 42; // engine runs at 24 fps; match it for input cadence.

async function listenRandomPort(): Promise<{ server: Server; port: number }> {
  const server = createServer();
  await new Promise<void>((res, rej) => {
    server.once('error', rej);
    server.listen(0, '127.0.0.1', () => {
      server.removeListener('error', rej);
      res();
    });
  });
  const addr = server.address();
  if (!addr || typeof addr === 'string') {
    throw new Error('failed to bind random port');
  }
  return { server, port: addr.port };
}

async function reserveFreePort(): Promise<number> {
  const { server, port } = await listenRandomPort();
  await new Promise<void>((res) => server.close(() => res()));
  return port;
}

function findBinary(): string {
  if (process.env.SILENCER_BIN) return process.env.SILENCER_BIN;
  const candidates = [
    resolve(
      import.meta.dir,
      '../../silencer/build/Silencer.app/Contents/MacOS/Silencer',
    ),
    resolve(import.meta.dir, '../../silencer/build/silencer'),
    resolve(import.meta.dir, '../../silencer/build/Silencer.exe'),
  ];
  for (const c of candidates) if (existsSync(c)) return c;
  throw new Error(
    'silencer binary not found (set SILENCER_BIN or build clients/silencer)',
  );
}

async function main(): Promise<void> {
  if (!process.stdout.isTTY) {
    console.error('silencer-tui requires a TTY');
    process.exit(2);
  }

  const bin = findBinary();
  // Frame listener stays open — we're the server, the engine connects in.
  const frameListener = await listenRandomPort();
  // Control port: we're the *client*, the engine binds the port. Reserve a
  // free port number, then close immediately so the engine can bind it.
  // (Brief race window between close and engine's bind; haven't hit it.)
  const controlPort = await reserveFreePort();

  const inputs = new TerminalInput();
  const rasterizer = new HalfBlockRasterizer();
  const parser = new FrameStreamParser();
  let lastPalette: Palette | null = null;
  let pendingFrame: { w: number; h: number; pixels: Uint8Array } | null = null;

  // Start the frame listener first so the spawned engine has somewhere to
  // connect when its TUIBackend::Init runs. Resolves once the engine connects.
  const frameConnected = (async () => {
    const [sock] = (await once(frameListener.server, 'connection')) as [
      import('node:net').Socket,
    ];
    sock.on('data', (chunk: Buffer) => {
      const msgs = parser.push(new Uint8Array(chunk));
      for (const m of msgs) {
        if (m.type === 'palette') lastPalette = m.palette;
        else if (m.type === 'frame') pendingFrame = m.frame;
      }
    });
    sock.on('close', () => {
      // Engine exited; tear down.
      cleanup();
      process.exit(0);
    });
    return sock;
  })();

  // Set up the terminal AFTER opening listeners but BEFORE spawning, so we
  // capture stdin from frame zero and any engine stderr lines aren't lost
  // into a partially-initialized alt screen.
  term.setup();

  const child: Subprocess = spawn({
    cmd: [
      bin,
      '--tui',
      '--control-port',
      String(controlPort),
    ],
    env: {
      ...process.env,
      SILENCER_TUI_FRAME_HOST: '127.0.0.1',
      SILENCER_TUI_FRAME_PORT: String(frameListener.port),
    },
    stdout: 'inherit',
    stderr: 'pipe',
    stdin: 'ignore',
  });

  // term.ts handlers for SIGINT/SIGTERM/SIGHUP call process.exit directly
  // to restore the cursor + alt screen ASAP. They bypass cleanup(), so
  // register an exit hook to kill the engine — otherwise it stays running
  // as an orphan with audio looping and CPU burning in SDL_Delay(33).
  process.on('exit', () => {
    try {
      child.kill();
    } catch {}
  });

  // Stream the engine's stderr to our own stderr but only after exit so it
  // doesn't trample the alt screen mid-render. We hold it in memory in case
  // the user wants it for debugging.
  const stderrChunks: Uint8Array[] = [];
  if (child.stderr) {
    const reader = (child.stderr as ReadableStream<Uint8Array>).getReader();
    (async () => {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) stderrChunks.push(value);
      }
    })();
  }

  // Wait for the engine to connect back on the frame port before we start
  // pushing input. Connection can take ~1-2s while assets load.
  await frameConnected;

  // Connect the control socket. Retry briefly because the C++ side starts
  // its control server only after Load() finishes (asset load).
  const control = new ControlClient();
  let controlConnected = false;
  for (let attempt = 0; attempt < 100; attempt++) {
    try {
      await control.connect('127.0.0.1', controlPort);
      controlConnected = true;
      break;
    } catch {
      await Bun.sleep(100);
    }
  }
  if (!controlConnected) {
    cleanup();
    console.error(
      `silencer-tui: failed to connect to engine control port ${controlPort} after 10s — input will not work`,
    );
    process.exit(3);
  }

  let cleaned = false;
  function cleanup(): void {
    if (cleaned) return;
    cleaned = true;
    try {
      control.close();
    } catch {}
    try {
      child.kill();
    } catch {}
    try {
      frameListener.server.close();
    } catch {}
    if (stderrChunks.length > 0) {
      const buf = Buffer.concat(stderrChunks.map((c) => Buffer.from(c)));
      process.stderr.write(buf);
    }
  }

  process.stdin.on('data', (chunk: Buffer) => {
    const events = inputs.feed(chunk);
    for (const ev of events) {
      if (ev.kind === 'name') {
        control.sendNoAwait('key', { key: ev.name });
      } else {
        control.sendNoAwait('key', { ascii: ev.ascii });
      }
    }
  });

  // Render loop — fires whenever a new frame arrives. Bundles the rasterizer
  // output into a single write to keep terminal updates atomic.
  let renderRequested = false;
  function scheduleRender(): void {
    if (renderRequested) return;
    renderRequested = true;
    setImmediate(() => {
      renderRequested = false;
      if (!pendingFrame || !lastPalette) return;
      const { cols, rows } = term.size();
      const buf = rasterizer.render(pendingFrame, lastPalette, { cols, rows });
      pendingFrame = null;
      if (buf.length > 0) process.stdout.write(buf);
    });
  }

  term.onResize(() => {
    rasterizer.reset();
    scheduleRender();
  });

  // Re-render whenever a new frame lands. We tap the parser indirectly by
  // polling; the data callback already updates pendingFrame, so we just check
  // each tick.
  const tick = setInterval(() => {
    if (inputs.quitRequested) {
      cleanup();
      process.exit(0);
    }
    inputs.decay();
    const snap = inputs.snapshot();
    control.sendNoAwait('input', snap as unknown as Record<string, unknown>);
    if (pendingFrame) scheduleRender();
  }, TICK_MS);

  // When the child exits, we exit too.
  child.exited.then(() => {
    clearInterval(tick);
    cleanup();
    process.exit(0);
  });
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
