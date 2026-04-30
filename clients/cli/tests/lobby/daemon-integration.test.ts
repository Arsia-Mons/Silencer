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

  // Regression for Devin-flagged JSON round-trip bug: `new Uint8Array(20)` in
  // commands.ts serialized to {"0":0,...,"19":0} (object keys, not an array)
  // and `g.mapHash.length` came out undefined on the daemon side, causing the
  // SDK's encodeLobbyGame to throw before any bytes hit the wire. The fix is
  // to send mapHash as a regular array; this test asserts the wire shape
  // round-trips with a usable .length.
  test("game_create payload survives JSON round-trip with usable mapHash", async () => {
    let receivedHash: unknown;
    class CapturingLobby extends FakeLobby {
      override createGame(g: any) {
        receivedHash = g.mapHash;
        super.createGame(g);
      }
    }
    const mgr = new SessionManager(() => new CapturingLobby());
    const localSock = join(tmp, "lobbyd-cap.sock");
    const localServer = await startRpcServer({ socketPath: localSock, manager: mgr });
    try {
      await rpcCall(localSock, {
        id: 1,
        op: "spawn",
        args: {
          name: "alice",
          host: "h",
          port: 1,
          version: "v",
          platform: 0,
          user: "u",
          pass: "p",
        },
      });
      const r = await rpcCall(localSock, {
        id: 2,
        op: "game_create",
        args: {
          name: "alice",
          // Mirrors what commands.ts builds; the regression here is whether
          // an array of 20 zeros survives the JSON round-trip with .length === 20.
          game: {
            id: 0,
            name: "TEST",
            password: "",
            mapName: "",
            maxPlayers: 8,
            maxTeams: 2,
            minLevel: 0,
            maxLevel: 0,
            securityLevel: 0,
            extra: 0,
            players: 0,
            state: 0,
            accountId: 0,
            hostname: "",
            mapHash: new Array(20).fill(0),
            port: 0,
          },
        },
      });
      expect(r.ok).toBe(true);
      // The bug class: if mapHash was sent as a Uint8Array, JSON serialization
      // would drop .length on the daemon side. We assert .length resolves and
      // every element is iterable as a number.
      expect((receivedHash as unknown[])?.length).toBe(20);
      for (let i = 0; i < 20; i++) expect((receivedHash as number[])[i]).toBe(0);
    } finally {
      await localServer.stop();
    }
  });
});
