// In-memory session map for lobbyd. Each session wraps one LobbyClient.
//
// Decoupled from @silencer/lobby-sdk via the LobbyLike interface so tests
// can substitute an in-memory fake without standing up a real lobby server.

import type { LobbyClient as RealLobbyClient } from "@silencer/lobby-sdk";

export interface LobbyLike {
  readonly state:
    | "disconnected"
    | "connecting"
    | "awaiting_version"
    | "awaiting_auth"
    | "authenticated"
    | "failed";
  readonly accountId: number;
  readonly lastError: string;
  on(event: string, fn: (...args: any[]) => void): () => void;
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  sendVersion(): void;
  sendCredentials(user: string, pass: string): void;
  sendChat(channel: string, text: string): void;
  joinChannel(channel: string): void;
  createGame(g: any): void;
  setGame(gameId: number, status: number): void;
}

export type LobbyFactory = (cfg: {
  host: string;
  port: number;
  version: string;
  platform: number;
}) => LobbyLike;

export interface SpawnArgs {
  name: string;
  host: string;
  port: number;
  version: string;
  platform: number;
  user: string;
  pass: string;
}

export interface Session {
  name: string;
  lobby: LobbyLike;
  host: string;
  port: number;
}

export interface SessionSummary {
  name: string;
  state: string;
  accountId: number;
  host: string;
  port: number;
}

const SPAWN_TIMEOUT_MS = 10_000;

export class SessionManager {
  private sessions = new Map<string, Session>();
  constructor(private factory: LobbyFactory) {}

  async spawn(args: SpawnArgs): Promise<{ accountId: number }> {
    if (this.sessions.has(args.name)) {
      throw new Error(`session "${args.name}" already exists`);
    }
    const lobby = this.factory({
      host: args.host,
      port: args.port,
      version: args.version,
      platform: args.platform,
    });
    const session: Session = { name: args.name, lobby, host: args.host, port: args.port };
    // Reserve the slot eagerly so concurrent spawns can't race the duplicate check.
    this.sessions.set(args.name, session);

    try {
      const result = await new Promise<{ accountId: number }>((resolve, reject) => {
        const timer = setTimeout(() => {
          cleanup();
          reject(new Error(`spawn "${args.name}" timed out after ${SPAWN_TIMEOUT_MS}ms`));
        }, SPAWN_TIMEOUT_MS);
        const off = lobby.on("stateChanged", (s: string) => {
          if (s === "awaiting_version") lobby.sendVersion();
          else if (s === "awaiting_auth") lobby.sendCredentials(args.user, args.pass);
          else if (s === "authenticated") {
            cleanup();
            resolve({ accountId: lobby.accountId });
          } else if (s === "failed") {
            cleanup();
            reject(new Error(lobby.lastError || "auth failed"));
          }
        });
        const cleanup = () => {
          clearTimeout(timer);
          off();
        };
        lobby.connect().catch((e) => {
          cleanup();
          reject(e);
        });
      });
      return result;
    } catch (e) {
      this.sessions.delete(args.name);
      try {
        await lobby.disconnect();
      } catch {
        /* ignore */
      }
      throw e;
    }
  }

  async kill(name: string): Promise<void> {
    const s = this.sessions.get(name);
    if (!s) throw new Error(`NO_SESSION: ${name}`);
    this.sessions.delete(name);
    await s.lobby.disconnect();
  }

  async killAll(): Promise<void> {
    const names = [...this.sessions.keys()];
    await Promise.all(names.map((n) => this.kill(n).catch(() => {})));
  }

  getOrThrow(name: string): Session {
    const s = this.sessions.get(name);
    if (!s) throw new Error(`NO_SESSION: ${name}`);
    return s;
  }

  list(): SessionSummary[] {
    return [...this.sessions.values()].map((s) => ({
      name: s.name,
      state: s.lobby.state,
      accountId: s.lobby.accountId,
      host: s.host,
      port: s.port,
    }));
  }

  size(): number {
    return this.sessions.size;
  }
}

// Convenience factory used by the real daemon. Lazily imports the SDK so
// unit tests don't pay the cost of resolving it.
export async function realLobbyFactory(): Promise<LobbyFactory> {
  const sdk = await import("@silencer/lobby-sdk");
  return (cfg) =>
    new (sdk.LobbyClient as typeof RealLobbyClient)({
      host: cfg.host,
      port: cfg.port,
      version: cfg.version,
      platform: cfg.platform as 0 | 1 | 2,
    }) as unknown as LobbyLike;
}
