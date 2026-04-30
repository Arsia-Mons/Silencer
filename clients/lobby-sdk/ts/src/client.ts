// Bun-based binary TCP client for the Silencer lobby server.

import { createHash } from "node:crypto";
import {
    decodeAuthReply,
    decodeChannel,
    decodeChatPush,
    decodeDelGame,
    decodeMotd,
    decodeNewGame,
    decodePresence,
    decodeUserInfo,
    decodeVersionReply,
    encodeAuthRequest,
    encodeChat,
    encodeJoinChannel,
    encodeNewGame,
    encodePingAck,
    encodeRegisterStats,
    encodeSetGame,
    encodeUpgradeStat,
    encodeUserInfoRequest,
    encodeVersionRequest,
    frameEncode,
    frameTryDecode,
    Reader,
} from "./codec.ts";
import {
    type AuthResult,
    type ChatMessage,
    type GameStatus,
    type LobbyGame,
    type MatchStats,
    type NewGameEvent,
    Op,
    type Platform,
    type PresenceUpdate,
    type UserInfo,
    type VersionResult,
} from "./types.ts";

export type ConnectionState =
    | "disconnected"
    | "connecting"
    | "awaiting_version"
    | "awaiting_auth"
    | "authenticated"
    | "failed";

export interface ClientConfig {
    host: string;
    port: number;
    /** Empty string = skip version handshake. */
    version?: string;
    platform?: Platform;
    /** Inactivity threshold; client closes the socket if no bytes arrive
     *  within this window. Default 20s, matching the reference C++ client. */
    readTimeoutMs?: number;
}

export interface ClientEvents {
    stateChanged: (state: ConnectionState) => void;
    version: (v: VersionResult) => void;
    auth: (a: AuthResult) => void;
    motd: (full: string) => void;
    chat: (m: ChatMessage) => void;
    channel: (name: string) => void;
    newGame: (e: NewGameEvent) => void;
    delGame: (id: number) => void;
    userInfo: (u: UserInfo) => void;
    presence: (p: PresenceUpdate) => void;
    statUpgraded: () => void;
    error: (message: string) => void;
}

export function sha1(data: Uint8Array | string): Uint8Array {
    const h = createHash("sha1");
    h.update(typeof data === "string" ? data : data);
    return new Uint8Array(h.digest());
}

export class LobbyClient {
    private cfg: Required<ClientConfig>;
    private socket: any | null = null;
    private rx = new Uint8Array(0);
    private motdBuf = "";
    private _state: ConnectionState = "disconnected";
    private _accountId = 0;
    private _lastError = "";
    private rxTimer: ReturnType<typeof setTimeout> | null = null;
    private lastRxAt = 0;

    private listeners: { [K in keyof ClientEvents]: Set<ClientEvents[K]> } = {
        stateChanged: new Set(),
        version: new Set(),
        auth: new Set(),
        motd: new Set(),
        chat: new Set(),
        channel: new Set(),
        newGame: new Set(),
        delGame: new Set(),
        userInfo: new Set(),
        presence: new Set(),
        statUpgraded: new Set(),
        error: new Set(),
    };

    constructor(cfg: ClientConfig) {
        this.cfg = {
            host: cfg.host,
            port: cfg.port,
            version: cfg.version ?? "",
            platform: cfg.platform ?? 0,
            readTimeoutMs: cfg.readTimeoutMs ?? 20_000,
        };
    }

    get state(): ConnectionState { return this._state; }
    get accountId(): number { return this._accountId; }
    get lastError(): string { return this._lastError; }

    on<K extends keyof ClientEvents>(event: K, fn: ClientEvents[K]): () => void {
        this.listeners[event].add(fn as never);
        return () => { this.listeners[event].delete(fn as never); };
    }

    private emit<K extends keyof ClientEvents>(event: K, ...args: Parameters<ClientEvents[K]>): void {
        for (const fn of this.listeners[event]) (fn as (...a: unknown[]) => void)(...args);
    }

    private setState(s: ConnectionState): void {
        if (this._state === s) return;
        this._state = s;
        this.emit("stateChanged", s);
    }

    async connect(): Promise<void> {
        await this.disconnect();
        this.setState("connecting");
        this.lastRxAt = Date.now();
        const self = this;
        this.socket = await Bun.connect({
            hostname: this.cfg.host,
            port: this.cfg.port,
            socket: {
                open() {
                    self.setState(self.cfg.version === "" ? "awaiting_auth" : "awaiting_version");
                },
                data(_sock, chunk: Uint8Array) {
                    self.lastRxAt = Date.now();
                    self.appendRx(chunk);
                    self.drain();
                },
                close() { self.handleClose("connection closed by peer"); },
                error(_sock, err: Error) { self.handleClose("socket: " + err.message); },
            },
        });
        this.startTimeoutWatch();
    }

    async disconnect(): Promise<void> {
        this.stopTimeoutWatch();
        if (this.socket) {
            try { this.socket.end(); } catch { /* ignore */ }
            this.socket = null;
        }
        this.rx = new Uint8Array(0);
        this.motdBuf = "";
        this._accountId = 0;
        this.setState("disconnected");
    }

    private handleClose(msg: string): void {
        // Both 'failed' and 'disconnected' are terminal — when the dispatch
        // path already set 'failed' (version/auth rejection), a follow-up
        // socket close from the server should preserve that signal rather
        // than emit a spurious 'failed → disconnected' transition.
        if (this._state === "disconnected" || this._state === "failed") {
            this.stopTimeoutWatch();
            this.socket = null;
            return;
        }
        this._lastError = msg;
        this.emit("error", msg);
        this.stopTimeoutWatch();
        this.socket = null;
        this.setState("disconnected");
    }

    private startTimeoutWatch(): void {
        this.stopTimeoutWatch();
        this.rxTimer = setInterval(() => {
            if (Date.now() - this.lastRxAt > this.cfg.readTimeoutMs) {
                this.handleClose("read timeout");
            }
        }, Math.min(2_000, this.cfg.readTimeoutMs));
    }

    private stopTimeoutWatch(): void {
        if (this.rxTimer) { clearInterval(this.rxTimer); this.rxTimer = null; }
    }

    private appendRx(chunk: Uint8Array): void {
        const next = new Uint8Array(this.rx.length + chunk.length);
        next.set(this.rx, 0);
        next.set(chunk, this.rx.length);
        this.rx = next;
    }

    private drain(): void {
        for (;;) {
            let frame: { payload: Uint8Array; consumed: number } | null;
            try {
                frame = frameTryDecode(this.rx);
            } catch (e) {
                this.handleClose("frame: " + (e as Error).message);
                return;
            }
            if (!frame) return;
            this.rx = this.rx.slice(frame.consumed);
            try {
                this.dispatch(frame.payload);
            } catch (e) {
                this.handleClose("decode: " + (e as Error).message);
                return;
            }
        }
    }

    private dispatch(payload: Uint8Array): void {
        if (payload.length === 0) return;
        const r = new Reader(payload);
        const op = r.u8();
        switch (op) {
            case Op.Version: {
                const v = decodeVersionReply(r);
                if (v.ok) this.setState("awaiting_auth");
                else { this._lastError = "version rejected"; this.setState("failed"); }
                this.emit("version", v);
                break;
            }
            case Op.Auth: {
                const a = decodeAuthReply(r);
                if (a.ok) { this._accountId = a.accountId; this.setState("authenticated"); }
                else { this._lastError = a.error; this.setState("failed"); }
                this.emit("auth", a);
                break;
            }
            case Op.MOTD: {
                const c = decodeMotd(r, payload.length);
                if (c.terminator) {
                    this.emit("motd", this.motdBuf);
                    this.motdBuf = "";
                } else {
                    this.motdBuf += c.text;
                }
                break;
            }
            case Op.Chat:        this.emit("chat", decodeChatPush(r)); break;
            case Op.NewGame:     this.emit("newGame", decodeNewGame(r)); break;
            case Op.DelGame:     this.emit("delGame", decodeDelGame(r)); break;
            case Op.Channel:     this.emit("channel", decodeChannel(r)); break;
            case Op.UserInfo:    this.emit("userInfo", decodeUserInfo(r)); break;
            case Op.Ping:        this.sendRaw(encodePingAck()); break;
            case Op.Presence:    this.emit("presence", decodePresence(r)); break;
            case Op.UpgradeStat: this.emit("statUpgraded"); break;
            case Op.Connect:     /* reserved, ignore */ break;
            default:             this.emit("error", `unknown opcode ${op}`); break;
        }
    }

    private sendRaw(payload: Uint8Array): void {
        if (!this.socket) return;
        this.socket.write(frameEncode(payload));
    }

    // ---- outbound API ---------------------------------------------------

    sendVersion(): void { this.sendRaw(encodeVersionRequest(this.cfg.version, this.cfg.platform)); }
    sendCredentials(username: string, password: string): void {
        this.sendRaw(encodeAuthRequest(username, sha1(password)));
    }
    sendChat(channel: string, message: string): void { this.sendRaw(encodeChat(channel, message)); }
    joinChannel(channel: string): void { this.sendRaw(encodeJoinChannel("", channel)); }
    createGame(g: LobbyGame): void { this.sendRaw(encodeNewGame(g)); }
    requestUserInfo(accountId: number): void { this.sendRaw(encodeUserInfoRequest(accountId)); }
    upgradeStat(agencyIdx: number, statId: number): void {
        this.sendRaw(encodeUpgradeStat(agencyIdx, statId));
    }
    setGame(gameId: number, status: GameStatus): void {
        this.sendRaw(encodeSetGame(gameId, status));
    }
    registerStats(args: {
        gameId: number;
        teamNumber: number;
        accountId: number;
        statsAgency: number;
        won: boolean;
        xp: number;
        stats: MatchStats;
    }): void {
        this.sendRaw(encodeRegisterStats(args.gameId, args.teamNumber, args.accountId,
            args.statsAgency, args.won, args.xp, args.stats));
    }
}
