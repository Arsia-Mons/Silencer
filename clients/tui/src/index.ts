#!/usr/bin/env bun
// silencer-tui: spawns a Silencer headless engine in TUI mode and renders the
// resulting framebuffer in a terminal. Three TCP channels:
//
//   1. Frame socket   (engine → host, binary)   — palette + framebuffer.
//   2. Input socket   (host → engine, binary)   — packed snapshots, latest-wins.
//   3. Control socket (host ↔ engine, JSON RPC) — menu keys, automation ops.
//
// Each channel matches the shape of its data: streaming framebuffers, lossy
// state replication, and ordered RPC, respectively. Earlier the per-tick input
// snapshot rode on the control socket; that piled up behind the request/reply
// pump and produced multi-second input lag.
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
import { KittyRasterizer } from './raster_kitty';
import { ControlClient } from './control_client';
import { InputClient } from './input_client';
import { TerminalInput } from './input';
import * as term from './term';

interface Rasterizer {
  reset(): void;
  render(
    frame: { w: number; h: number; pixels: Uint8Array },
    palette: Palette,
    target: { cols: number; rows: number },
  ): Uint8Array;
}

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
  // Control + input ports: engine binds, we're the client. Reserve free port
  // numbers and close immediately so the engine can take them. Brief race
  // window between close and engine's bind; haven't hit it.
  const controlPort = await reserveFreePort();
  const inputPort = await reserveFreePort();

  const inputs = new TerminalInput();
  let rasterizer: Rasterizer = new HalfBlockRasterizer();
  const parser = new FrameStreamParser();
  let lastPalette: Palette | null = null;
  let pendingFrame: { w: number; h: number; pixels: Uint8Array } | null = null;
  // Buffered for stderr-after-exit (so log lines don't trample the alt screen
  // mid-render). Lifted above the spawn so the kitty probe diagnostic can
  // also use it.
  const stderrChunks: Uint8Array[] = [];

  // Cleanup state declared up-front: the frame socket's close handler and the
  // control/input retry-failure paths can all invoke cleanup() before later
  // setup completes. Keeping `cleaned` and `function cleanup` at the top
  // avoids a TDZ ReferenceError on those early call sites; child / control /
  // inputClient stay nullable until their respective spawn / connect lands.
  let cleaned = false;
  let child: Subprocess | null = null;
  let control: ControlClient | null = null;
  let inputClient: InputClient | null = null;
  function cleanup(): void {
    if (cleaned) return;
    cleaned = true;
    try {
      control?.close();
    } catch {}
    try {
      inputClient?.close();
    } catch {}
    try {
      child?.kill();
    } catch {}
    try {
      frameListener.server.close();
    } catch {}
    // stderr flush deliberately lives in the process.on('exit') hook below:
    // it must run AFTER term.ts's restore handler exits the alt screen,
    // otherwise the writes land in the alt-screen buffer and disappear when
    // the primary screen is restored.
  }

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

  // Probe for kitty support (graphics + keyboard) BEFORE attaching the
  // long-lived stdin input handler — the probes need unobstructed access to
  // the response bytes and would race with input.feed() otherwise. Bypass
  // either probe via env var.
  const supportsKitty = await term.probeKittyGraphics();
  let cellPixelSize: { width: number; height: number } | null = null;
  if (supportsKitty) {
    // Measure cell pixel dims so the kitty rasterizer can compute exact
    // aspect-correct placement. Probe runs after the kitty probe — each
    // consumes its own DA1 sentinel so they don't interleave.
    cellPixelSize = await term.probeCellSize();
    rasterizer = new KittyRasterizer({ cellPixelSize });
    term.markKittyInUse();
  }
  const supportsKittyKbd = await term.probeKittyKeyboard();
  if (supportsKittyKbd) {
    term.enableKittyKeyboard();
    inputs.setKittyKeyboard(true);
  }
  if (process.env.SILENCER_TUI_DEBUG === '1') {
    const { cols, rows } = term.size();
    const cellTxt = cellPixelSize
      ? `${cellPixelSize.width}x${cellPixelSize.height}`
      : 'unknown';
    stderrChunks.push(
      new TextEncoder().encode(
        `[silencer-tui] rasterizer=${supportsKitty ? 'kitty' : 'half-block'}` +
          ` keyboard=${supportsKittyKbd ? 'kitty' : 'legacy'}` +
          ` terminal=${cols}x${rows} cell-px=${cellTxt}\n`,
      ),
    );
  }

  child = spawn({
    cmd: [
      bin,
      '--tui',
      '--control-port',
      String(controlPort),
      '--tui-input-port',
      String(inputPort),
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
  // as an orphan with audio looping and CPU burning in SDL_Delay(33). Also
  // flush any buffered stderr (kitty probe diag, engine warnings) so signal
  // exits don't swallow them.
  process.on('exit', () => {
    try {
      child?.kill();
    } catch {}
    if (stderrChunks.length > 0) {
      try {
        process.stderr.write(
          Buffer.concat(stderrChunks.map((c) => Buffer.from(c))),
        );
        stderrChunks.length = 0;
      } catch {}
    }
  });

  // Stream the engine's stderr into the same buffered sink. Flushed at exit
  // so log lines don't trample the alt screen mid-render.
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
  // pushing input. Connection can take ~1-2s while assets load. Race
  // against child.exited so a crash during asset load surfaces as a clear
  // error instead of hanging silently with no terminal output.
  const frameOrExit = await Promise.race([
    frameConnected.then(() => 'connected' as const),
    child.exited.then((code) => ({ exitCode: code })),
  ]);
  if (typeof frameOrExit === 'object' && 'exitCode' in frameOrExit) {
    if (stderrChunks.length > 0) {
      process.stderr.write(
        Buffer.concat(stderrChunks.map((c) => Buffer.from(c))),
      );
    }
    console.error(
      `silencer-tui: engine exited (code ${frameOrExit.exitCode}) before connecting to frame port`,
    );
    process.exit(4);
  }

  // Connect the control socket. Retry briefly because the C++ side starts
  // its control server only after Load() finishes (asset load).
  control = new ControlClient();
  inputClient = new InputClient();
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
  // The engine starts the input server in the same Load() phase as the
  // control server, so a successful control connect implies the input
  // listener is up. Still retry briefly to cover the small ordering gap.
  let inputConnected = false;
  for (let attempt = 0; attempt < 50; attempt++) {
    try {
      await inputClient.connect('127.0.0.1', inputPort);
      inputConnected = true;
      break;
    } catch {
      await Bun.sleep(100);
    }
  }
  if (!inputConnected) {
    cleanup();
    console.error(
      `silencer-tui: failed to connect to engine input port ${inputPort} — input will not work`,
    );
    process.exit(3);
  }

  const trace = process.env.SILENCER_TUI_INPUT_TRACE === '1';
  process.stdin.on('data', (chunk: Buffer) => {
    if (trace) {
      stderrChunks.push(
        new TextEncoder().encode(`[trace] in: ${chunk.toString('hex')}\n`),
      );
    }
    const events = inputs.feed(chunk);
    if (trace && events.length > 0) {
      stderrChunks.push(
        new TextEncoder().encode(
          `[trace] events: ${JSON.stringify(events)}\n`,
        ),
      );
    }
    for (const ev of events) {
      if (ev.kind === 'name') {
        control?.sendNoReply('key', { key: ev.name });
      } else {
        control?.sendNoReply('key', { ascii: ev.ascii });
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
    inputClient?.sendScancodes(inputs.snapshot());
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
