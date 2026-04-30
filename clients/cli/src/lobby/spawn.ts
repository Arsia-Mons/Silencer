import { mkdirSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { resolveLobbydDir, socketPath } from "./paths.ts";

const POLL_INTERVAL_MS = 50;
const POLL_TIMEOUT_MS = 5_000;

async function probe(sock: string): Promise<boolean> {
  try {
    await Bun.connect({
      unix: sock,
      socket: {
        open(s) {
          s.end();
        },
        data() {},
        close() {},
        error() {},
      },
    });
    return true;
  } catch {
    return false;
  }
}

/** Returns the resolved socket path, spawning the daemon if not yet listening. */
export async function ensureDaemon(): Promise<string> {
  const dir = resolveLobbydDir();
  mkdirSync(dir, { recursive: true });
  const sock = socketPath(dir);
  if (await probe(sock)) return sock;

  // Daemon entry lives next to this file at runtime.
  const here = fileURLToPath(import.meta.url);
  const daemonEntry = resolve(dirname(here), "daemon.ts");

  Bun.spawn({
    cmd: ["bun", daemonEntry],
    stdio: ["ignore", "ignore", "ignore"],
    // Detach so the parent CLI can exit while the daemon stays up.
    // unref() lets node-style event-loop drain even if we forget to wait.
  }).unref();

  const deadline = Date.now() + POLL_TIMEOUT_MS;
  while (Date.now() < deadline) {
    if (await probe(sock)) return sock;
    await new Promise((r) => setTimeout(r, POLL_INTERVAL_MS));
  }
  throw new Error(`lobbyd did not start within ${POLL_TIMEOUT_MS}ms (socket: ${sock})`);
}
