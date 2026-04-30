#!/usr/bin/env bun
// silencer-lobbyd — multiplexes N LobbyClient sessions for the agent CLI.
//
// Lifecycle:
//   1. Resolve dir/socket/log path.
//   2. mkdir -p dir.
//   3. Bind unix socket; if bind fails because a live daemon is already
//      listening, exit 0 (the CLI client will use that one).
//   4. Serve until idle (zero sessions AND zero connections) or SIGINT.

import { mkdirSync, openSync, writeSync } from "node:fs";
import { logPath, resolveLobbydDir, socketPath } from "./paths.ts";
import { startRpcServer } from "./rpc-server.ts";
import { realLobbyFactory, SessionManager } from "./session-manager.ts";

async function main(): Promise<void> {
  const dir = resolveLobbydDir();
  mkdirSync(dir, { recursive: true });
  const sock = socketPath(dir);
  const log = logPath(dir);

  // Append-only log file; one line per significant event.
  const logFd = openSync(log, "a");
  const ts = () => new Date().toISOString();
  const logLine = (msg: string) => writeSync(logFd, `${ts()} ${msg}\n`);

  // If a daemon is already up, defer to it. The CLI client's auto-spawn
  // probes first and only invokes us when no peer answered, but a race
  // between two parallel spawns is still possible.
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
    logLine("another lobbyd already listening; exiting");
    process.exit(0);
  } catch {
    /* expected — no peer listening */
  }

  const factory = await realLobbyFactory();
  const manager = new SessionManager(factory);
  let server: Awaited<ReturnType<typeof startRpcServer>>;

  const shutdown = async (reason: string) => {
    logLine(`shutdown: ${reason}`);
    await manager.killAll().catch(() => {});
    await server?.stop().catch(() => {});
    process.exit(0);
  };

  server = await startRpcServer({
    socketPath: sock,
    manager,
    onIdle: () => {
      void shutdown("idle");
    },
  });

  process.on("SIGINT", () => void shutdown("SIGINT"));
  process.on("SIGTERM", () => void shutdown("SIGTERM"));
  logLine(`listening on ${sock}`);
}

main().catch((e) => {
  process.stderr.write(
    `[lobbyd] fatal: ${e instanceof Error ? (e.stack ?? e.message) : String(e)}\n`,
  );
  process.exit(1);
});
