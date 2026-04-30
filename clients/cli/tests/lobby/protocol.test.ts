import { describe, expect, test } from "bun:test";
import { encodeFrame, parseFrames, type Reply, type Request } from "../../src/lobby/protocol.ts";

describe("encodeFrame", () => {
  test("appends a single trailing newline", () => {
    const out = encodeFrame({ id: 1, ok: true, final: true });
    expect(out.endsWith("\n")).toBe(true);
    expect(out.split("\n").filter(Boolean).length).toBe(1);
  });
  test("round-trips through JSON.parse", () => {
    const r: Reply = { id: 7, ok: false, error: "x", code: "Y", final: true };
    expect(JSON.parse(encodeFrame(r).trim())).toEqual(r);
  });
});

describe("parseFrames", () => {
  test("returns parsed objects and the unconsumed remainder", () => {
    const a: Request = { id: 1, op: "ls", args: {} };
    const b: Request = { id: 2, op: "kill", args: { name: "alice" } };
    const buf = encodeFrame(a) + encodeFrame(b) + '{"id":3,"op":"x"';
    const { frames, rest } = parseFrames<Request>(buf);
    expect(frames).toEqual([a, b]);
    expect(rest).toBe('{"id":3,"op":"x"');
  });
  test("empty input → empty frames, empty rest", () => {
    expect(parseFrames("")).toEqual({ frames: [], rest: "" });
  });
  test("malformed JSON throws with the offending line", () => {
    expect(() => parseFrames("{not json}\n")).toThrow(/{not json}/);
  });
});
