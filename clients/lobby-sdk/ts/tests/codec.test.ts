// Golden-vector tests for the TS lobby SDK codec.
//
// Loads shared/lobby-protocol/vectors.json and verifies that:
//   - decode(hex)   produces the expected struct
//   - encode(struct) reproduces the hex (unless the vector is in
//                   the skip-encode set)
// Same vectors are consumed by the C++ test suite — drift on either
// side fails its tests.

import { describe, expect, test } from "bun:test";
import {
    decodeAuthReply,
    decodeChannel,
    decodeChatPush,
    decodeDelGame,
    decodeLobbyGame,
    decodeMotd,
    decodeNewGame,
    decodePresence,
    decodeUserInfo,
    decodeVersionReply,
    encodeAuthReply,
    encodeAuthRequest,
    encodeChat,
    encodeChatPush,
    encodeLobbyGame,
    encodeMotdChunk,
    encodePingAck,
    encodePresence,
    encodeRegisterStats,
    encodeSetGame,
    encodeUpgradeStat,
    encodeUserInfoBody,
    encodeUserInfoRequest,
    encodeVersionReply,
    encodeVersionRequest,
    frameEncode,
    frameTryDecode,
    Op,
    Platform,
    Reader,
    SecurityLevel,
    GameStatus,
    sha1,
    Writer,
    type LobbyGame,
    type MatchStats,
    type UserInfo,
} from "../src/index.ts";
import { emptyMatchStats } from "../src/types.ts";

interface Vector {
    name: string;
    kind: string;
    hex: string;
    value?: unknown;
    _skip_encode?: string;
}

const vectorsFile = JSON.parse(
    await Bun.file(import.meta.dir + "/../../../../shared/lobby-protocol/vectors.json").text(),
) as { vectors: Vector[] };

const vectors: Map<string, Vector> = new Map(vectorsFile.vectors.map((v) => [v.name, v]));

function fromHex(hex: string): Uint8Array {
    const out = new Uint8Array(hex.length / 2);
    for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.substr(i * 2, 2), 16);
    return out;
}
function toHex(b: Uint8Array): string {
    let s = "";
    for (let i = 0; i < b.length; i++) s += b[i]!.toString(16).padStart(2, "0");
    return s;
}

function unframe(hex: string): Uint8Array {
    const wire = fromHex(hex);
    const f = frameTryDecode(wire);
    expect(f).not.toBeNull();
    expect(f!.consumed).toBe(wire.length);
    return f!.payload;
}

function framedHex(payload: Uint8Array): string {
    return toHex(frameEncode(payload));
}

function need(name: string): Vector {
    const v = vectors.get(name);
    if (!v) throw new Error(`missing vector ${name} in vectors.json`);
    return v;
}

describe("framing", () => {
    test("rejects zero-length frame", () => {
        expect(() => frameTryDecode(new Uint8Array([0]))).toThrow();
    });
    test("returns null when buffer is short", () => {
        expect(frameTryDecode(new Uint8Array([5, 1, 2]))).toBeNull();
    });
    test("rejects payload too large", () => {
        expect(() => frameEncode(new Uint8Array(256))).toThrow();
    });
    test("rejects empty payload", () => {
        expect(() => frameEncode(new Uint8Array(0))).toThrow();
    });
});

describe("sha1", () => {
    test("known answer 'abc'", () => {
        expect(toHex(sha1("abc"))).toBe("a9993e364706816aba3e25717850c26c9cd0d89d");
    });
    test("empty input", () => {
        expect(toHex(sha1(""))).toBe("da39a3ee5e6b4b0d3255bfef95601890afd80709");
    });
    test("long input", () => {
        expect(toHex(sha1("a".repeat(1000)))).toBe("291e9a6c66994949b57ba5e650361e98fc36b1ba");
    });
});

describe("golden vectors", () => {
    test("auth_request", () => {
        const v = need("auth_request");
        const payload = unframe(v.hex);
        const r = new Reader(payload);
        expect(r.u8()).toBe(Op.Auth);
        expect(r.cstr(17)).toBe("alice");
        const hash = r.bytesN(20);
        expect(toHex(hash)).toBe("000102030405060708090a0b0c0d0e0f10111213");

        const out = framedHex(encodeAuthRequest("alice", hash));
        expect(out).toBe(v.hex);
    });

    test("auth_reply_success", () => {
        const v = need("auth_reply_success");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Auth);
        const a = decodeAuthReply(r);
        expect(a.ok).toBe(true);
        expect(a.accountId).toBe(0x80000000);

        const w = new Writer(); w.u8(Op.Auth); encodeAuthReply(w, a);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("auth_reply_failure", () => {
        const v = need("auth_reply_failure");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Auth);
        const a = decodeAuthReply(r);
        expect(a.ok).toBe(false);
        expect(a.error).toBe("Incorrect password for bob");

        const w = new Writer(); w.u8(Op.Auth); encodeAuthReply(w, a);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("version_request", () => {
        const v = need("version_request");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Version);
        expect(r.cstr(64)).toBe("1.2.3");
        expect(r.u8()).toBe(Platform.MacOSARM64);

        expect(framedHex(encodeVersionRequest("1.2.3", Platform.MacOSARM64))).toBe(v.hex);
    });

    test("version_reply_ok", () => {
        const v = need("version_reply_ok");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Version);
        const reply = decodeVersionReply(r);
        expect(reply.ok).toBe(true);
        expect(reply.updateUrl).toBe("");

        const w = new Writer(); w.u8(Op.Version); encodeVersionReply(w, reply);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("version_reply_reject_with_update", () => {
        const v = need("version_reply_reject_with_update");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Version);
        const reply = decodeVersionReply(r);
        expect(reply.ok).toBe(false);
        expect(reply.updateUrl).toBe("https://example.com/silencer.dmg");
        for (let i = 0; i < 32; i++) expect(reply.sha256[i]).toBe(0xaa);

        const w = new Writer(); w.u8(Op.Version); encodeVersionReply(w, reply);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("version_reply_reject_bare", () => {
        const v = need("version_reply_reject_bare");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Version);
        const reply = decodeVersionReply(r);
        expect(reply.ok).toBe(false);
        expect(reply.updateUrl).toBe("");

        const w = new Writer(); w.u8(Op.Version); encodeVersionReply(w, reply);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("motd_chunk", () => {
        const v = need("motd_chunk");
        const payload = unframe(v.hex);
        const r = new Reader(payload);
        expect(r.u8()).toBe(Op.MOTD);
        const c = decodeMotd(r, payload.length);
        expect(c.terminator).toBe(false);
        expect(c.text).toBe("Hello");

        const w = new Writer(); w.u8(Op.MOTD); encodeMotdChunk(w, c);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("motd_terminator", () => {
        const v = need("motd_terminator");
        const payload = unframe(v.hex);
        const r = new Reader(payload);
        expect(r.u8()).toBe(Op.MOTD);
        const c = decodeMotd(r, payload.length);
        expect(c.terminator).toBe(true);

        const w = new Writer(); w.u8(Op.MOTD); encodeMotdChunk(w, c);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("chat_request", () => {
        const v = need("chat_request");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Chat);
        expect(r.cstr(64)).toBe("Lobby");
        expect(r.cstr(255)).toBe("hi!");

        expect(framedHex(encodeChat("Lobby", "hi!"))).toBe(v.hex);
    });

    test("chat_push", () => {
        const v = need("chat_push");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Chat);
        const m = decodeChatPush(r);
        expect(m).toEqual({ channel: "room1", text: "hi there!", color: 255, brightness: 127 });

        const w = new Writer(); w.u8(Op.Chat); encodeChatPush(w, m);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("newgame_push", () => {
        const v = need("newgame_push");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.NewGame);
        const ev = decodeNewGame(r);
        expect(ev.status).toBe(1);
        expect(ev.game.id).toBe(100);
        expect(ev.game.accountId).toBe(10);
        expect(ev.game.name).toBe("Test");
        expect(ev.game.hostname).toBe("123.456.789.0,5000");
        expect(ev.game.mapName).toBe("TestServ");
        expect(ev.game.port).toBe(5000);
        expect(ev.game.securityLevel).toBe(SecurityLevel.Medium);
        expect(toHex(ev.game.mapHash)).toBe("deadbeef".repeat(5));

        const w = new Writer(); w.u8(Op.NewGame); w.u8(ev.status); encodeLobbyGame(w, ev.game);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("delgame_push", () => {
        const v = need("delgame_push");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.DelGame);
        expect(decodeDelGame(r)).toBe(100);
    });

    test("channel_push", () => {
        const v = need("channel_push");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Channel);
        expect(decodeChannel(r)).toBe("Home");
    });

    test("userinfo_request", () => {
        const v = need("userinfo_request");
        expect(framedHex(encodeUserInfoRequest(200))).toBe(v.hex);
    });

    test("userinfo_reply", () => {
        const v = need("userinfo_reply");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.UserInfo);
        const u = decodeUserInfo(r);
        expect(u.accountId).toBe(200);
        expect(u.name).toBe("admin");
        expect(u.agencies[0]!.wins).toBe(10);
        expect(u.agencies[1]!.level).toBe(13);

        const w = new Writer(); w.u8(Op.UserInfo); encodeUserInfoBody(w, u);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("ping_push", () => {
        const v = need("ping_push");
        const payload = unframe(v.hex);
        expect(payload.length).toBe(1);
        expect(payload[0]).toBe(Op.Ping);
    });

    test("ping_ack", () => {
        const v = need("ping_ack");
        expect(framedHex(encodePingAck())).toBe(v.hex);
    });

    test("upgradestat_request", () => {
        const v = need("upgradestat_request");
        expect(framedHex(encodeUpgradeStat(2, 3))).toBe(v.hex);
    });

    test("upgradestat_reply", () => {
        const v = need("upgradestat_reply");
        const payload = unframe(v.hex);
        expect(payload.length).toBe(1);
        expect(payload[0]).toBe(Op.UpgradeStat);
    });

    test("presence_add", () => {
        const v = need("presence_add");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Presence);
        const p = decodePresence(r);
        expect(p).toEqual({
            removed: false, accountId: 42, gameId: 0, status: GameStatus.Lobby, name: "alice",
        });
        const w = new Writer(); w.u8(Op.Presence); encodePresence(w, p);
        expect(framedHex(w.bytes())).toBe(v.hex);
    });

    test("presence_remove", () => {
        const v = need("presence_remove");
        const r = new Reader(unframe(v.hex));
        expect(r.u8()).toBe(Op.Presence);
        const p = decodePresence(r);
        expect(p.removed).toBe(true);
        expect(p.accountId).toBe(42);
        expect(p.gameId).toBe(100);
        expect(p.status).toBe(GameStatus.Playing);
        expect(p.name).toBe("kim");
    });

    test("setgame_request", () => {
        const v = need("setgame_request");
        expect(framedHex(encodeSetGame(100, GameStatus.Pregame))).toBe(v.hex);
    });
});

describe("register_stats roundtrip", () => {
    test("encodes 192-byte payload + length prefix", () => {
        const s: MatchStats = emptyMatchStats();
        s.weapons[0]!.fires = 100;
        s.kills = 3;
        s.creditsEarned = 0xdeadbeef;
        const enc = frameEncode(encodeRegisterStats(7, 1, 9, 2, true, 1234, s));
        // 44 × u32 = 176 bytes stats. Payload = 1 + 4 + 1 + 4 + 1 + 1 + 4 + 176 = 192.
        // Wire = 1 + 192 = 193.
        expect(enc.length).toBe(193);
        expect(enc[0]).toBe(192);
        expect(enc[1]).toBe(Op.RegisterStats);
    });
});
