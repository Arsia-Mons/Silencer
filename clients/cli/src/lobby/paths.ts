// Resolves the co-located directory holding lobbyd's Unix socket and log
// file. One env override (SILENCER_LOBBYD_DIR) wins on every platform.
//
// macOS sun_path is 104 bytes; 100-byte cap leaves a 3-byte cushion for the
// trailing NUL.

import { join, win32 } from "node:path";

export const MAX_SOCKET_PATH = 100;

export function resolveLobbydDir(): string {
  if (process.env.SILENCER_LOBBYD_DIR) return process.env.SILENCER_LOBBYD_DIR;
  switch (process.platform) {
    case "linux": {
      const xdg = process.env.XDG_RUNTIME_DIR;
      return xdg ? join(xdg, "silencer") : "/tmp/silencer";
    }
    case "darwin": {
      const tmp = process.env.TMPDIR || "/tmp";
      return join(tmp, "silencer");
    }
    case "win32": {
      const local = process.env.LOCALAPPDATA;
      if (!local) throw new Error("LOCALAPPDATA not set");
      return win32.join(local, "Silencer", "lobbyd");
    }
    default:
      return "/tmp/silencer";
  }
}

export function socketPath(dir: string): string {
  const p = join(dir, "lobbyd.sock");
  if (process.platform === "darwin" && p.length > MAX_SOCKET_PATH) {
    throw new Error(
      `socket path "${p}" (${p.length} bytes) exceeds macOS sun_path limit (${MAX_SOCKET_PATH}). ` +
        `Override with SILENCER_LOBBYD_DIR=<shorter dir>.`,
    );
  }
  return p;
}

export function logPath(dir: string): string {
  return join(dir, "lobbyd.log");
}
