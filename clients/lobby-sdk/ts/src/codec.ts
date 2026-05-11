// Wire-format codec for the Silencer lobby protocol.
// See ../../../shared/lobby-protocol/protocol.md.

import {
    type AgencyStats,
    type AuthResult,
    type ChatMessage,
    emptyAgency,
    type LobbyGame,
    type MatchStats,
    type MotdChunk,
    type NewGameEvent,
    Op,
    Platform,
    type PresenceUpdate,
    SecurityLevel,
    GameStatus,
    type UserInfo,
    type VersionResult,
    type WeaponStats,
} from "./types.ts";

export class CodecError extends Error {
    override name = "CodecError";
}

export const MAX_FRAME_PAYLOAD = 255;

// ---- Reader / Writer ----------------------------------------------------

export class Reader {
    private off = 0;
    constructor(public readonly bytes: Uint8Array) {}

    get remaining(): number {
        return this.bytes.length - this.off;
    }
    get offset(): number {
        return this.off;
    }

    u8(): number {
        if (this.off >= this.bytes.length) throw new CodecError("u8: short read");
        return this.bytes[this.off++]!;
    }

    u16Le(): number {
        if (this.off + 2 > this.bytes.length) throw new CodecError("u16: short read");
        const v = this.bytes[this.off]! | (this.bytes[this.off + 1]! << 8);
        this.off += 2;
        return v;
    }

    u32Le(): number {
        if (this.off + 4 > this.bytes.length) throw new CodecError("u32: short read");
        const v =
            this.bytes[this.off]! |
            (this.bytes[this.off + 1]! << 8) |
            (this.bytes[this.off + 2]! << 16) |
            (this.bytes[this.off + 3]! << 24);
        this.off += 4;
        // Force unsigned interpretation.
        return v >>> 0;
    }

    bytesN(n: number): Uint8Array {
        if (this.off + n > this.bytes.length) throw new CodecError("bytes: short read");
        const out = this.bytes.slice(this.off, this.off + n);
        this.off += n;
        return out;
    }

    cstr(maxLen: number): string {
        const limit = Math.min(this.bytes.length, this.off + maxLen);
        let end = this.off;
        while (end < limit && this.bytes[end] !== 0) end++;
        if (end >= limit) throw new CodecError("cstr: unterminated");
        const s = utf8Decode(this.bytes.subarray(this.off, end));
        this.off = end + 1;
        return s;
    }

    lenstr(): string {
        const n = this.u8();
        if (this.off + n > this.bytes.length) throw new CodecError("lenstr: short read");
        const s = utf8Decode(this.bytes.subarray(this.off, this.off + n));
        this.off += n;
        return s;
    }
}

export class Writer {
    private chunks: number[] = [];

    u8(v: number): void {
        this.chunks.push(v & 0xff);
    }
    u16Le(v: number): void {
        this.chunks.push(v & 0xff, (v >>> 8) & 0xff);
    }
    u32Le(v: number): void {
        this.chunks.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff);
    }
    write(src: Uint8Array): void {
        for (let i = 0; i < src.length; i++) this.chunks.push(src[i]!);
    }
    cstr(s: string): void {
        const b = utf8Encode(s);
        for (let i = 0; i < b.length; i++) this.chunks.push(b[i]!);
        this.chunks.push(0);
    }
    lenstr(s: string): void {
        const b = utf8Encode(s);
        const n = Math.min(b.length, 255);
        this.chunks.push(n);
        for (let i = 0; i < n; i++) this.chunks.push(b[i]!);
    }
    bytes(): Uint8Array {
        return new Uint8Array(this.chunks);
    }
}

// UTF-8 helpers (Bun has TextEncoder/TextDecoder globally).
const _enc = new TextEncoder();
const _dec = new TextDecoder("utf-8", { fatal: false });
const utf8Encode = (s: string) => _enc.encode(s);
const utf8Decode = (b: Uint8Array) => _dec.decode(b);

// ---- Framing ------------------------------------------------------------

export function frameEncode(payload: Uint8Array): Uint8Array {
    if (payload.length === 0 || payload.length > MAX_FRAME_PAYLOAD) {
        throw new CodecError("frameEncode: bad payload size");
    }
    const out = new Uint8Array(1 + payload.length);
    out[0] = payload.length;
    out.set(payload, 1);
    return out;
}

// Returns the consumed payload (opcode + body) and how many bytes of the
// input were consumed, or null if a full frame isn't yet available.
export function frameTryDecode(buf: Uint8Array): { payload: Uint8Array; consumed: number } | null {
    if (buf.length < 1) return null;
    const n = buf[0]!;
    if (n === 0) throw new CodecError("frameTryDecode: zero-length frame");
    if (buf.length < 1 + n) return null;
    return { payload: buf.slice(1, 1 + n), consumed: 1 + n };
}

// ---- LobbyGame ----------------------------------------------------------

export function encodeLobbyGame(w: Writer, g: LobbyGame): void {
    w.u32Le(g.id);
    w.u32Le(g.accountId);
    w.lenstr(g.name);
    w.lenstr(g.password);
    w.lenstr(g.hostname);
    w.lenstr(g.mapName);
    if (g.mapHash.length !== 20) throw new CodecError("mapHash must be 20 bytes");
    w.write(g.mapHash);
    w.u8(g.players);
    w.u8(g.state);
    w.u8(g.securityLevel);
    w.u8(g.minLevel);
    w.u8(g.maxLevel);
    w.u8(g.maxPlayers);
    w.u8(g.maxTeams);
    w.u8(g.extra);
    w.u16Le(g.port);
}

export function decodeLobbyGame(r: Reader): LobbyGame {
    const id = r.u32Le();
    const accountId = r.u32Le();
    const name = r.lenstr();
    const password = r.lenstr();
    const hostname = r.lenstr();
    const mapName = r.lenstr();
    const mapHash = r.bytesN(20);
    const players = r.u8();
    const state = r.u8();
    const securityLevel = r.u8() as SecurityLevel;
    const minLevel = r.u8();
    const maxLevel = r.u8();
    const maxPlayers = r.u8();
    const maxTeams = r.u8();
    const extra = r.u8();
    const port = r.u16Le();
    return {
        id, accountId, name, password, hostname, mapName, mapHash,
        players, state, securityLevel, minLevel, maxLevel,
        maxPlayers, maxTeams, extra, port,
    };
}

// ---- Per-opcode encoders ------------------------------------------------

export function encodeVersionRequest(version: string, platform: Platform): Uint8Array {
    const w = new Writer();
    w.u8(Op.Version);
    w.cstr(version);
    w.u8(platform);
    return w.bytes();
}

export function encodeAuthRequest(username: string, passwordSha1: Uint8Array): Uint8Array {
    if (passwordSha1.length !== 20) throw new CodecError("passwordSha1 must be 20 bytes");
    const w = new Writer();
    w.u8(Op.Auth);
    w.cstr(username);
    w.write(passwordSha1);
    return w.bytes();
}

export function encodeChat(channel: string, message: string): Uint8Array {
    const w = new Writer();
    w.u8(Op.Chat);
    w.cstr(channel);
    w.cstr(message);
    return w.bytes();
}

export function encodeJoinChannel(currentChannel: string, newChannel: string): Uint8Array {
    return encodeChat(currentChannel, "/join " + newChannel);
}

export function encodeNewGame(g: LobbyGame): Uint8Array {
    const w = new Writer();
    w.u8(Op.NewGame);
    encodeLobbyGame(w, g);
    return w.bytes();
}

export function encodeUserInfoRequest(accountId: number): Uint8Array {
    const w = new Writer();
    w.u8(Op.UserInfo);
    w.u32Le(accountId);
    return w.bytes();
}

export function encodePingAck(): Uint8Array {
    return new Uint8Array([Op.Ping, 1]);
}

export function encodeUpgradeStat(agencyIdx: number, statId: number): Uint8Array {
    return new Uint8Array([Op.UpgradeStat, agencyIdx & 0xff, statId & 0xff]);
}

export function encodeSetGame(gameId: number, status: GameStatus): Uint8Array {
    const w = new Writer();
    w.u8(Op.SetGame);
    w.u32Le(gameId);
    w.u8(status);
    return w.bytes();
}

export function encodeRegisterStats(
    gameId: number,
    teamNumber: number,
    accountId: number,
    statsAgency: number,
    won: boolean,
    xp: number,
    s: MatchStats,
): Uint8Array {
    const w = new Writer();
    w.u8(Op.RegisterStats);
    w.u32Le(gameId);
    w.u8(teamNumber);
    w.u32Le(accountId);
    w.u8(statsAgency);
    w.u8(won ? 1 : 0);
    w.u32Le(xp);
    for (let i = 0; i < 4; i++) {
        const wp = s.weapons[i]!;
        w.u32Le(wp.fires);
        w.u32Le(wp.hits);
        w.u32Le(wp.playerKills);
    }
    w.u32Le(s.civiliansKilled);
    w.u32Le(s.guardsKilled);
    w.u32Le(s.robotsKilled);
    w.u32Le(s.defenseKilled);
    w.u32Le(s.secretsPickedUp);
    w.u32Le(s.secretsReturned);
    w.u32Le(s.secretsStolen);
    w.u32Le(s.secretsDropped);
    w.u32Le(s.powerupsPickedUp);
    w.u32Le(s.deaths);
    w.u32Le(s.kills);
    w.u32Le(s.suicides);
    w.u32Le(s.poisons);
    w.u32Le(s.tractsPlanted);
    w.u32Le(s.grenadesThrown);
    w.u32Le(s.neutronsThrown);
    w.u32Le(s.empsThrown);
    w.u32Le(s.shapedThrown);
    w.u32Le(s.plasmasThrown);
    w.u32Le(s.flaresThrown);
    w.u32Le(s.poisonFlaresThrown);
    w.u32Le(s.healthPacksUsed);
    w.u32Le(s.fixedCannonsPlaced);
    w.u32Le(s.fixedCannonsDestroyed);
    w.u32Le(s.detsPlanted);
    w.u32Le(s.camerasPlanted);
    w.u32Le(s.virusesUsed);
    w.u32Le(s.filesHacked);
    w.u32Le(s.filesReturned);
    w.u32Le(s.creditsEarned);
    w.u32Le(s.creditsSpent);
    w.u32Le(s.healsDone);
    return w.bytes();
}

// ---- Body decoders ------------------------------------------------------

export function decodeVersionReply(r: Reader): VersionResult {
    const ok = r.u8() !== 0;
    let updateUrl = "";
    let sha256: Uint8Array = new Uint8Array(32);
    if (!ok && r.remaining >= 2 + 32) {
        const urlLen = r.u16Le();
        if (urlLen > 0 && urlLen < 512 && r.remaining >= urlLen + 32) {
            updateUrl = utf8Decode(r.bytesN(urlLen));
            sha256 = r.bytesN(32);
        }
    }
    return { ok, updateUrl, sha256 };
}

export function decodeAuthReply(r: Reader): AuthResult {
    const ok = r.u8() !== 0;
    if (ok) {
        return { ok: true, accountId: r.u32Le(), error: "" };
    }
    return { ok: false, accountId: 0, error: r.cstr(256) };
}

export function decodeChatPush(r: Reader): ChatMessage {
    const channel = r.cstr(64);
    const text = r.cstr(MAX_FRAME_PAYLOAD);
    const color = r.u8();
    const brightness = r.u8();
    return { channel, text, color, brightness };
}

export function decodeNewGame(r: Reader): NewGameEvent {
    const status = r.u8();
    const game = decodeLobbyGame(r);
    return { status, game };
}

export const decodeDelGame = (r: Reader): number => r.u32Le();

export const decodeChannel = (r: Reader): string => r.cstr(64);

export function decodeUserInfo(r: Reader): UserInfo {
    const accountId = r.u32Le();
    const agencies = [emptyAgency(), emptyAgency(), emptyAgency(), emptyAgency(), emptyAgency()] as
        UserInfo["agencies"];
    for (let i = 0; i < 5; i++) {
        const a: AgencyStats = {
            wins: r.u16Le(),
            losses: r.u16Le(),
            xpToNextLevel: r.u16Le(),
            level: r.u8(),
            endurance: r.u8(),
            shield: r.u8(),
            jetpack: r.u8(),
            techSlots: r.u8(),
            hacking: r.u8(),
            contacts: r.u8(),
        };
        agencies[i] = a;
    }
    const name = r.lenstr();
    return { accountId, agencies, name };
}

export function decodePresence(r: Reader): PresenceUpdate {
    const action = r.u8();
    const accountId = r.u32Le();
    const gameId = r.u32Le();
    const status = r.u8() as GameStatus;
    const name = r.lenstr();
    return { removed: action === 1, accountId, gameId, status, name };
}

// MOTD: payload is just the body byte if it's a 1-byte terminator,
// else a cstr chunk. payloadSize includes the opcode byte.
export function decodeMotd(r: Reader, payloadSize: number): MotdChunk {
    if (payloadSize <= 2 && r.remaining >= 1) {
        const b = r.u8();
        return { terminator: b === 0, text: b === 0 ? "" : String.fromCharCode(b) };
    }
    return { terminator: false, text: r.cstr(MAX_FRAME_PAYLOAD) };
}

// ---- Symmetric encoders for inbound shapes (used in tests) -------------

export function encodeVersionReply(w: Writer, v: VersionResult): void {
    w.u8(v.ok ? 1 : 0);
    if (!v.ok && v.updateUrl !== "") {
        const urlBytes = utf8Encode(v.updateUrl);
        w.u16Le(urlBytes.length);
        w.write(urlBytes);
        if (v.sha256.length !== 32) throw new CodecError("sha256 must be 32 bytes");
        w.write(v.sha256);
    }
}

export function encodeAuthReply(w: Writer, a: AuthResult): void {
    w.u8(a.ok ? 1 : 0);
    if (a.ok) w.u32Le(a.accountId);
    else w.cstr(a.error);
}

export function encodeChatPush(w: Writer, m: ChatMessage): void {
    w.cstr(m.channel);
    w.cstr(m.text);
    w.u8(m.color);
    w.u8(m.brightness);
}

export function encodeMotdChunk(w: Writer, c: MotdChunk): void {
    if (c.terminator) w.u8(0);
    else w.cstr(c.text);
}

export function encodePresence(w: Writer, p: PresenceUpdate): void {
    w.u8(p.removed ? 1 : 0);
    w.u32Le(p.accountId);
    w.u32Le(p.gameId);
    w.u8(p.status);
    w.lenstr(p.name);
}

export function encodeUserInfoBody(w: Writer, u: UserInfo): void {
    w.u32Le(u.accountId);
    for (const a of u.agencies) {
        w.u16Le(a.wins);
        w.u16Le(a.losses);
        w.u16Le(a.xpToNextLevel);
        w.u8(a.level);
        w.u8(a.endurance);
        w.u8(a.shield);
        w.u8(a.jetpack);
        w.u8(a.techSlots);
        w.u8(a.hacking);
        w.u8(a.contacts);
    }
    w.lenstr(u.name);
}
