import { describe, expect, test, beforeEach, afterEach } from "bun:test";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { rpcCall, rpcStream } from "../../src/lobby/rpc-client.ts";
import { encodeFrame, parseFrames, type Reply, type Request } from "../../src/lobby/protocol.ts";

let tmp: string;
let sock: string;
let server: ReturnType<typeof Bun.listen>;

beforeEach(() => {
  tmp = mkdtempSync(join(tmpdir(), "rpcclient-"));
  sock = join(tmp, "s.sock");
});
afterEach(() => {
  server?.stop(true);
  rmSync(tmp, { recursive: true, force: true });
});

function startEcho(reply: (req: Request) => Reply | Reply[]): void {
  server = Bun.listen({
    unix: sock,
    socket: {
      open(s) {
        (s as any).__b = "";
      },
      data(s, chunk) {
        (s as any).__b += new TextDecoder().decode(chunk);
        const { frames, rest } = parseFrames<Request>((s as any).__b);
        (s as any).__b = rest;
        for (const f of frames) {
          const out = reply(f);
          for (const r of Array.isArray(out) ? out : [out]) s.write(encodeFrame(r));
        }
      },
      close() {},
      error() {},
    },
  });
}

describe("rpcCall", () => {
  test("returns the single final reply", async () => {
    startEcho((req) => ({ id: req.id, ok: true, result: { echoed: req.op }, final: true }));
    const r = await rpcCall(sock, { id: 1, op: "ping", args: {} });
    expect(r).toEqual({ id: 1, ok: true, result: { echoed: "ping" }, final: true });
  });

  test("rejects when daemon socket is missing", async () => {
    const missing = join(tmp, "nope.sock");
    await expect(rpcCall(missing, { id: 1, op: "x", args: {} })).rejects.toThrow();
  });
});

describe("rpcStream", () => {
  test("yields frames until final", async () => {
    startEcho((req) => [
      { id: req.id, ok: true, result: { event: "a" }, final: false },
      { id: req.id, ok: true, result: { event: "b" }, final: false },
      { id: req.id, ok: true, result: {}, final: true },
    ]);
    const out: Reply[] = [];
    for await (const r of rpcStream(sock, { id: 1, op: "tail", args: {}, stream: true }))
      out.push(r);
    expect(out.map((r) => (r.result as any).event ?? "<final>")).toEqual(["a", "b", "<final>"]);
  });
});
