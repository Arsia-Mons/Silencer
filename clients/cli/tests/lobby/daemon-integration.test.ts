// Drives the real daemon via spawn.ensureDaemon, against an in-memory fake
// SessionManager? No — easier: hit the daemon's RPC server directly with a
// SessionManager that uses our fake LobbyClient. That keeps this test
// hermetic (no real lobby server required).
//
// Strategy: instead of forking a child process, build the same wiring
// (manager + server) in-process and exercise it through the rpc-client.
// The detached child fork is exercised manually in Task 10's smoke step.

import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { rpcCall, rpcStream } from "../../src/lobby/rpc-client.ts";
import { startRpcServer, type RpcServer } from "../../src/lobby/rpc-server.ts";
import { SessionManager } from "../../src/lobby/session-manager.ts";

class FakeLobby {
  state: any = "disconnected";
  accountId = 0;
  lastError = "";
  ls: Record<string, Set<any>> = {};
  on(e: string, fn: any) {
    (this.ls[e] ??= new Set()).add(fn);
    return () => this.ls[e]?.delete(fn);
  }
  emit(e: string, ...a: any[]) {
    for (const fn of this.ls[e] ?? []) fn(...a);
  }
  async connect() {
    queueMicrotask(() => {
      this.state = "authenticated";
      this.accountId = 99;
      this.emit("stateChanged", "authenticated");
    });
  }
  async disconnect() {
    this.state = "disconnected";
    this.emit("stateChanged", "disconnected");
  }
  sendVersion() {}
  sendCredentials() {}
  sendChat(channel: string, text: string) {
    this.emit("chat", { channel, text, color: 0, brightness: 0 });
  }
  joinChannel(c: string) {
    this.emit("channel", c);
  }
  createGame(g: any) {
    this.emit("newGame", { status: 1, game: { ...g, id: 12345 } });
  }
  setGame() {}
}

let tmp: string;
let sock: string;
let server: RpcServer;
beforeEach(async () => {
  tmp = mkdtempSync(join(tmpdir(), "lobbyd-int-"));
  sock = join(tmp, "lobbyd.sock");
  const mgr = new SessionManager(() => new FakeLobby());
  server = await startRpcServer({ socketPath: sock, manager: mgr });
});
afterEach(async () => {
  await server.stop();
  rmSync(tmp, { recursive: true, force: true });
});

describe("daemon integration", () => {
  test("spawn → chat → tail flow", async () => {
    const spawn = await rpcCall(sock, {
      id: 1,
      op: "spawn",
      args: { name: "alice", host: "h", port: 1, version: "v", platform: 0, user: "u", pass: "p" },
    });
    expect(spawn.ok).toBe(true);

    // Open a tail before sending chat so we observe the emitted event.
    const iter = rpcStream(sock, {
      id: 2,
      op: "tail",
      args: { name: "alice" },
      stream: true,
    })[Symbol.asyncIterator]();
    // Wait for the tail to ack registration before sending events through it.
    const ack = await iter.next();
    expect((ack.value as any).result).toEqual({ event: "registered" });

    const events: any[] = [];
    const consumer = (async () => {
      for (;;) {
        const r = await iter.next();
        if (r.done) break;
        if (r.value.final) break;
        events.push(r.value.result);
      }
    })();

    const chat = await rpcCall(sock, {
      id: 3,
      op: "chat",
      args: { name: "alice", channel: "main", text: "hi" },
    });
    expect(chat.ok).toBe(true);

    // Game create should resolve once the fake echoes a newGame event.
    const create = await rpcCall(sock, {
      id: 4,
      op: "game_create",
      args: { name: "alice", game: { id: 0 } },
    });
    expect(create.ok).toBe(true);
    expect((create.result as any).gameId).toBe(12345);

    // Trigger end-of-tail by killing the session.
    await rpcCall(sock, { id: 5, op: "kill", args: { name: "alice" } });
    await consumer;

    const seenChat = events.some((e) => e.event === "chat" && e.data.text === "hi");
    expect(seenChat).toBe(true);
  });
});
