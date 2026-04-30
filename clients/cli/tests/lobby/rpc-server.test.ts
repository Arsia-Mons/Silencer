import { describe, expect, test, beforeEach, afterEach } from "bun:test";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { SessionManager } from "../../src/lobby/session-manager.ts";
import { startRpcServer, type RpcServer } from "../../src/lobby/rpc-server.ts";
import { encodeFrame, parseFrames, type Reply, type Request } from "../../src/lobby/protocol.ts";

class FakeLobby {
  state: any = "disconnected";
  accountId = 0;
  lastError = "";
  private ls: Record<string, Set<any>> = {};
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
      this.accountId = 7;
      this.emit("stateChanged", "authenticated");
    });
  }
  async disconnect() {
    this.state = "disconnected";
    this.emit("stateChanged", "disconnected");
  }
  sendVersion() {}
  sendCredentials() {}
  sendChat() {}
  joinChannel() {}
  createGame(g: any) {
    queueMicrotask(() => this.emit("newGame", { game: { id: 999 } }));
  }
  setGame() {}
}

let tmp: string;
let sock: string;
let server: RpcServer;
let mgr: SessionManager;

beforeEach(async () => {
  tmp = mkdtempSync(join(tmpdir(), "lobbyd-test-"));
  sock = join(tmp, "lobbyd.sock");
  mgr = new SessionManager(() => new FakeLobby());
  server = await startRpcServer({ socketPath: sock, manager: mgr });
});
afterEach(async () => {
  await server.stop();
  rmSync(tmp, { recursive: true, force: true });
});

async function rpc(req: Request): Promise<Reply> {
  const replies = await rpcStream(req, 1);
  return replies[0]!;
}

async function rpcStream(req: Request, expected: number): Promise<Reply[]> {
  return new Promise((resolve, reject) => {
    const out: Reply[] = [];
    let buf = "";
    Bun.connect({
      unix: sock,
      socket: {
        open(s) {
          s.write(encodeFrame(req));
        },
        data(s, chunk) {
          buf += new TextDecoder().decode(chunk);
          const { frames, rest } = parseFrames<Reply>(buf);
          buf = rest;
          for (const f of frames) {
            out.push(f);
            if (out.length >= expected) {
              s.end();
              resolve(out);
              return;
            }
          }
        },
        close() {
          if (out.length < expected)
            reject(new Error(`closed early; got ${out.length}/${expected}`));
        },
        error(_s, e) {
          reject(e);
        },
      },
    });
  });
}

describe("rpc-server", () => {
  test("ls on empty manager returns empty list", async () => {
    const r = await rpc({ id: 1, op: "ls", args: {} });
    expect(r).toEqual({ id: 1, ok: true, result: { sessions: [] }, final: true });
  });

  test("spawn → ls round trip", async () => {
    const s = await rpc({
      id: 2,
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
    expect(s.ok).toBe(true);
    expect((s.result as any).accountId).toBe(7);
    const ls = await rpc({ id: 3, op: "ls", args: {} });
    expect((ls.result as any).sessions).toHaveLength(1);
    expect((ls.result as any).sessions[0].name).toBe("alice");
  });

  test("kill on missing session returns NO_SESSION", async () => {
    const r = await rpc({ id: 4, op: "kill", args: { name: "ghost" } });
    expect(r.ok).toBe(false);
    expect(r.code).toBe("NO_SESSION");
  });

  test("unknown op returns BAD_OP", async () => {
    const r = await rpc({ id: 5, op: "nope", args: {} });
    expect(r.ok).toBe(false);
    expect(r.code).toBe("BAD_OP");
  });

  test("malformed JSON frame returns BAD_FRAME and closes socket", async () => {
    // Direct send (bypassing rpc helper) to inject malformed bytes.
    const replies: Reply[] = [];
    let buf = "";
    await new Promise<void>((resolve, reject) => {
      Bun.connect({
        unix: sock,
        socket: {
          open(s) {
            s.write("{not json}\n");
          },
          data(_s, chunk) {
            buf += new TextDecoder().decode(chunk);
            const { frames, rest } = parseFrames<Reply>(buf);
            buf = rest;
            replies.push(...frames);
          },
          close() {
            resolve();
          },
          error(_s, e) {
            reject(e);
          },
        },
      });
    });
    expect(replies).toHaveLength(1);
    expect(replies[0]!.ok).toBe(false);
    expect(replies[0]!.code).toBe("BAD_FRAME");
  });

  test("game_create happy path resolves on echoed newGame", async () => {
    // Spawn alice first.
    const s = await rpc({
      id: 100,
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
    expect(s.ok).toBe(true);

    // FakeLobby.createGame emits newGame via queueMicrotask; the listener resolves.
    const r = await rpc({ id: 101, op: "game_create", args: { name: "alice", game: { id: 0 } } });
    expect(r.ok).toBe(true);
    expect((r.result as any).gameId).toBe(999);
  });

  test("second tail on the same connection returns ALREADY_TAILING", async () => {
    await rpc({
      id: 200,
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
    // Open a connection, send tail, then a second tail on the SAME connection,
    // and observe both replies.
    const replies: Reply[] = [];
    let buf = "";
    await new Promise<void>((resolve, reject) => {
      Bun.connect({
        unix: sock,
        socket: {
          open(s) {
            s.write(encodeFrame({ id: 201, op: "tail", args: { name: "alice" }, stream: true }));
            s.write(encodeFrame({ id: 202, op: "tail", args: { name: "alice" }, stream: true }));
          },
          data(s, chunk) {
            buf += new TextDecoder().decode(chunk);
            const { frames, rest } = parseFrames<Reply>(buf);
            buf = rest;
            replies.push(...frames);
            // Wait for the rejection of id 202 then close.
            if (replies.some((r) => r.id === 202 && r.final)) {
              s.end();
            }
          },
          close() {
            resolve();
          },
          error(_s, e) {
            reject(e);
          },
        },
      });
    });
    const second = replies.find((r) => r.id === 202);
    expect(second).toBeTruthy();
    expect(second!.ok).toBe(false);
    expect(second!.code).toBe("ALREADY_TAILING");
  });
});
