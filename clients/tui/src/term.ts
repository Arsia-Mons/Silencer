// Terminal lifecycle: enter/leave alt screen, raw mode, hide cursor.
// Idempotent restore on exit signals so a crash doesn't strand a broken
// terminal. The same restore path runs in process exit handlers.

// 1000 = button-event tracking (press + release). 1002 adds drag motion
// while a button is held. 1003 adds any-motion (hover without button) —
// matches native SDL which dispatches MOUSE_MOTION on every move, not just
// drag. 1006 = SGR coordinate encoding (lifts the 223-cell limit and
// avoids the encoding ambiguity of the legacy X10 form).
const ENTER =
  '\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H\x1b[?1000h\x1b[?1002h\x1b[?1003h\x1b[?1006h';
const LEAVE =
  '\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l\x1b[?25h\x1b[?1049l\x1b[0m';

// Kitty graphics delete-all. Sent on shutdown if the kitty rasterizer was
// in use so any free-floating images are cleared. APC envelope; ignored by
// terminals that don't speak the protocol.
const KITTY_CLEANUP = '\x1b_Ga=d,d=A,q=2;\x1b\\';

// Kitty keyboard protocol: pop our entry from the stack to restore prior
// keyboard mode. Paired with `CSI > <flags> u` from enableKittyKeyboard().
const KITTY_KBD_DISABLE = '\x1b[<u';

let restored = false;
let cleanupKittyOnExit = false;
let kittyKbdEnabled = false;

/** Mark that we've placed kitty graphics images this session, so the restore
 *  path knows to delete them. Idempotent. */
export function markKittyInUse(): void {
  cleanupKittyOnExit = true;
}

export function setup(): void {
  process.stdout.write(ENTER);
  if (process.stdin.isTTY && process.stdin.setRawMode) {
    process.stdin.setRawMode(true);
  }
  process.stdin.resume();

  const restore = () => {
    if (restored) return;
    restored = true;
    if (process.stdin.isTTY && process.stdin.setRawMode) {
      try {
        process.stdin.setRawMode(false);
      } catch {}
    }
    if (kittyKbdEnabled) process.stdout.write(KITTY_KBD_DISABLE);
    if (cleanupKittyOnExit) process.stdout.write(KITTY_CLEANUP);
    process.stdout.write(LEAVE);
  };

  process.on('exit', restore);
  for (const sig of ['SIGINT', 'SIGTERM', 'SIGHUP'] as const) {
    process.on(sig, () => {
      restore();
      process.exit(0);
    });
  }
  process.on('uncaughtException', (err) => {
    restore();
    console.error(err);
    process.exit(1);
  });
}

export function size(): { cols: number; rows: number } {
  return {
    cols: process.stdout.columns ?? 80,
    rows: process.stdout.rows ?? 24,
  };
}

export function onResize(cb: () => void): void {
  process.stdout.on('resize', cb);
}

/**
 * Probe the terminal for kitty graphics protocol support.
 *
 * Sends a tiny query image (a=q, 1x1 px) followed by the standard primary
 * device attributes request (\x1b[c). DA1 always replies on a real terminal,
 * so it acts as a sentinel: by the time we see DA1's reply, we know any
 * kitty response would also have arrived. If we saw \x1b_G in the buffered
 * stdin bytes, kitty graphics is supported.
 *
 * Must be called AFTER setup() (raw mode required to capture the response)
 * and BEFORE attaching the long-lived stdin data handler in index.ts. Any
 * keypresses arriving during the probe window are dropped.
 *
 * SILENCER_TUI_FORCE_HALFBLOCK=1 → returns false without probing.
 * SILENCER_TUI_FORCE_KITTY=1     → returns true without probing.
 */
export async function probeKittyGraphics(timeoutMs = 250): Promise<boolean> {
  if (process.env.SILENCER_TUI_FORCE_HALFBLOCK === '1') return false;
  if (process.env.SILENCER_TUI_FORCE_KITTY === '1') return true;
  if (!process.stdin.isTTY) return false;

  // i=31 is an arbitrary non-1 id we won't reuse. s=v=1 minimal image.
  // a=q queries support without keeping the image. t=d direct payload.
  // f=24 RGB; payload "AAAA" (b64) = 3 zero bytes = one black pixel.
  const probe = '\x1b_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\x1b\\\x1b[c';

  return new Promise<boolean>((resolve) => {
    let buf = '';
    let done = false;
    let graceTimer: ReturnType<typeof setTimeout> | null = null;
    const finish = (result: boolean) => {
      if (done) return;
      done = true;
      process.stdin.removeListener('data', onData);
      clearTimeout(timer);
      if (graceTimer) clearTimeout(graceTimer);
      resolve(result);
    };
    const onData = (chunk: Buffer) => {
      buf += chunk.toString('binary');
      // Two possible orderings:
      //   1. Kitty response arrives, then DA1 — match \x1b_G now, finish
      //      immediately as supported.
      //   2. DA1 arrives first, kitty response trailing — see DA1, start a
      //      short grace window, then check the buffer for \x1b_G.
      // The grace window matters in practice on Ghostty, where DA1 can land
      // slightly ahead of the graphics query reply.
      if (/\x1b_G/.test(buf)) {
        finish(true);
        return;
      }
      if (graceTimer === null && /\x1b\[\?[\d;]*c/.test(buf)) {
        graceTimer = setTimeout(() => finish(/\x1b_G/.test(buf)), 60);
      }
    };
    process.stdin.on('data', onData);
    const timer = setTimeout(() => finish(false), timeoutMs);
    process.stdout.write(probe);
  });
}

/**
 * Probe the terminal for kitty keyboard protocol support.
 *
 * Sends `CSI ? u` (query progressive enhancement flags) followed by DA1 as a
 * sentinel. Supporting terminals (Kitty, Ghostty, WezTerm, foot, alacritty
 * 0.13+, …) reply with `CSI ? <flags> u`; legacy terminals just answer DA1.
 *
 * Same constraints as probeKittyGraphics: must run after setup() and before
 * the long-lived stdin handler is attached.
 *
 * SILENCER_TUI_FORCE_NO_KITTY_KBD=1 → returns false without probing.
 */
export async function probeKittyKeyboard(timeoutMs = 250): Promise<boolean> {
  if (process.env.SILENCER_TUI_FORCE_NO_KITTY_KBD === '1') return false;
  if (!process.stdin.isTTY) return false;

  const probe = '\x1b[?u\x1b[c';
  return new Promise<boolean>((resolve) => {
    let buf = '';
    let done = false;
    let graceTimer: ReturnType<typeof setTimeout> | null = null;
    const finish = (result: boolean) => {
      if (done) return;
      done = true;
      process.stdin.removeListener('data', onData);
      clearTimeout(timer);
      if (graceTimer) clearTimeout(graceTimer);
      resolve(result);
    };
    const onData = (chunk: Buffer) => {
      buf += chunk.toString('binary');
      if (/\x1b\[\?\d+u/.test(buf)) {
        finish(true);
        return;
      }
      if (graceTimer === null && /\x1b\[\?[\d;]*c/.test(buf)) {
        graceTimer = setTimeout(() => finish(/\x1b\[\?\d+u/.test(buf)), 60);
      }
    };
    process.stdin.on('data', onData);
    const timer = setTimeout(() => finish(false), timeoutMs);
    process.stdout.write(probe);
  });
}

/**
 * Enable kitty keyboard progressive enhancement. Flags 0xB:
 *   0x1 disambiguate escape codes
 *   0x2 report event types (press / repeat / release)
 *   0x8 report all keys as escape codes — without this, unmodified ASCII
 *       (Enter, Tab, plain letters) still ships as bare bytes and we lose
 *       release events for held WASD.
 * Push semantics (`CSI > N u`) so the restore path can `CSI < u` cleanly.
 *
 * After this, ALL keys arrive as `CSI <code> [;<mods>[:<event>]] <suffix>`.
 * event_type 1=press, 2=repeat, 3=release. Letter/control keys use suffix
 * `u`; arrow keys keep their legacy A/B/C/D suffixes.
 */
export function enableKittyKeyboard(): void {
  if (kittyKbdEnabled) return;
  kittyKbdEnabled = true;
  process.stdout.write('\x1b[>11u');
}

/**
 * Query terminal cell pixel dimensions via the xterm CSI 16 t extension.
 * Reply shape: \x1b[6;<height>;<width>t. Used by the kitty rasterizer to
 * compute aspect-correct image placement — without this, we'd guess at a
 * cell aspect ratio and the rendered image winds up letterboxed harder
 * than necessary on terminals whose cells aren't 1:2.
 *
 * Returns { width, height } in pixels, or null if the terminal doesn't
 * support the query (timed out or replied only to DA1).
 *
 * Same call-site constraints as the other probes: after setup(), before
 * the long-lived stdin handler is attached.
 */
export async function probeCellSize(
  timeoutMs = 250,
): Promise<{ width: number; height: number } | null> {
  if (!process.stdin.isTTY) return null;

  const probe = '\x1b[16t\x1b[c';
  return new Promise<{ width: number; height: number } | null>((resolve) => {
    let buf = '';
    let done = false;
    let graceTimer: ReturnType<typeof setTimeout> | null = null;
    const finish = () => {
      if (done) return;
      done = true;
      process.stdin.removeListener('data', onData);
      clearTimeout(timer);
      if (graceTimer) clearTimeout(graceTimer);
      const m = buf.match(/\x1b\[6;(\d+);(\d+)t/);
      resolve(
        m
          ? { height: parseInt(m[1]!, 10), width: parseInt(m[2]!, 10) }
          : null,
      );
    };
    const onData = (chunk: Buffer) => {
      buf += chunk.toString('binary');
      if (graceTimer === null && /\x1b\[\?[\d;]*c/.test(buf)) {
        // DA1 has replied; give the cell-size response a moment in case
        // the terminal emitted it after DA1.
        graceTimer = setTimeout(finish, 60);
      }
    };
    process.stdin.on('data', onData);
    const timer = setTimeout(finish, timeoutMs);
    process.stdout.write(probe);
  });
}
