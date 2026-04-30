// Terminal lifecycle: enter/leave alt screen, raw mode, hide cursor.
// Idempotent restore on exit signals so a crash doesn't strand a broken
// terminal. The same restore path runs in process exit handlers.

const ENTER = '\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H';
const LEAVE = '\x1b[?25h\x1b[?1049l\x1b[0m';

let restored = false;

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
