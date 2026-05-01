# Lobby Session Daemon Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `lobby` namespace to `silencer-cli` that lets agents spin up persistent authenticated lobby presences ("fake players") during dev, multiplexed inside a single supervisor daemon (`silencer-lobbyd`).

**Architecture:** One long-running Bun process holds N `LobbyClient` instances keyed by session name; thin CLI subcommands talk to it over a single Unix domain socket using JSON-lines RPC. Daemon auto-spawns on first use and auto-exits when its last session and last connection are gone. Per-session memory is just the `LobbyClient` (single-digit MB) — Bun runtime is amortized once across all sessions.

**Tech Stack:** Bun + TypeScript, `@silencer/lobby-sdk` (already on main), AF_UNIX sockets via `Bun.listen`/`Bun.connect`, `bun:test` for tests, `oxfmt` for formatting.

---

## File Structure

All new code lives under `clients/cli/src/lobby/`. The existing `clients/cli/index.ts` stays as the entry point and dispatch shell; the lobby namespace bolts in alongside the existing `keybind` / `gas` namespaces.

```
clients/cli/
├── package.json                   (modify: add @silencer/lobby-sdk + test script)
├── tsconfig.json                  (modify: include src/**)
├── index.ts                       (modify: register lobby namespace)
├── src/
│   └── lobby/
│       ├── paths.ts               platform dir resolution + sun_path guard
│       ├── protocol.ts            JSON-lines RPC frame types
│       ├── session-manager.ts     in-memory session map; takes a LobbyClient factory
│       ├── rpc-server.ts          unix-socket listener; dispatches to session-manager
│       ├── rpc-client.ts          unix-socket dialer; one-shot + streaming requests
│       ├── spawn.ts               auto-spawn detached daemon, wait for socket
│       ├── daemon.ts              entry point: wires manager + server, handles SIGINT
│       ├── commands.ts            CLI subcommand handlers (spawn/chat/game/ls/tail/kill)
│       └── CLAUDE.md              one-pager for this dir
└── tests/
    └── lobby/
        ├── paths.test.ts
        ├── protocol.test.ts
        ├── session-manager.test.ts
        ├── rpc-server.test.ts
        ├── rpc-client.test.ts
        └── daemon-integration.test.ts
```

Root files:

```
package.json                       (modify: add clients/lobby-sdk/ts to workspaces)
```

## RPC protocol (locked here for reference by every task)

JSON-lines over AF_UNIX. One JSON object per `\n`-terminated line.

**Request frame** (CLI → daemon):
```ts
type Request = {
  id: number;            // client-generated, opaque
  op: string;            // "spawn" | "kill" | "kill_all" | "ls" | "chat" | "join_channel"
                         //   | "game_create" | "game_join" | "tail"
  args: Record<string, unknown>;
  stream?: boolean;      // true for "tail"; default false
};
```

**Reply frame** (daemon → CLI):
```ts
type Reply = {
  id: number;            // matches request
  ok: boolean;
  result?: unknown;      // present when ok
  error?: string;        // present when !ok
  code?: string;         // short error code (e.g. "NO_SESSION", "AUTH_FAILED")
  final: boolean;        // false = more replies coming (streaming); true = RPC done
};
```

**Op contracts:**
- `spawn { name, host, port, version, platform, user, pass }` → `{ accountId }`. Final reply only when `state === "authenticated"` or `"failed"` (10s timeout).
- `kill { name }` → `{}`. Disconnects and removes the session.
- `kill_all {}` → `{}`. Disconnects all and triggers daemon exit.
- `ls {}` → `{ sessions: SessionSummary[] }`.
- `chat { name, channel, text }` → `{}`.
- `join_channel { name, channel }` → `{}`.
- `game_create { name, game: LobbyGame }` → `{ gameId }`. Resolves on the echoed `newGame` event for the host.
- `game_join { name, gameId }` → `{}`. Sends `setGame(gameId, GameStatus.Lobby)`.
- `tail { name }` (streaming) → series of `{ event: "chat" | "presence" | "channel" | "newGame" | "delGame" | "stateChanged", data: ... }` frames with `final: false`, then a final `{ ok: true, final: true }` when the session goes away or the client disconnects.

---

## Task 1: Register `@silencer/lobby-sdk` as a workspace and a CLI dep

**Files:**
- Modify: `package.json` (root)
- Modify: `clients/cli/package.json`
- Modify: `clients/cli/tsconfig.json`

- [ ] **Step 1: Add the SDK to root workspaces**

Edit `package.json` (root). Insert `"clients/lobby-sdk/ts"` into `workspaces`, alphabetically:

```json
{
  "name": "silencer",
  "private": true,
  "type": "module",
  "workspaces": [
    "clients/cli",
    "clients/lobby-sdk/ts",
    "services/admin-api",
    "shared/fonts",
    "shared/gas-validation",
    "web/admin",
    "web/website"
  ],
  "scripts": {
    "typecheck": "bun run --filter '*' typecheck"
  }
}
```

- [ ] **Step 2: Add the SDK as a CLI dep + add a test script + bin for the daemon**

Edit `clients/cli/package.json`:

```json
{
  "name": "silencer-cli",
  "private": true,
  "type": "module",
  "bin": {
    "silencer-cli": "./index.ts",
    "silencer-lobbyd": "./src/lobby/daemon.ts"
  },
  "scripts": {
    "fmt": "oxfmt --write .",
    "typecheck": "tsc --noEmit",
    "test": "bun test"
  },
  "dependencies": {
    "@silencer/gas-validation": "workspace:*",
    "@silencer/lobby-sdk": "workspace:*"
  },
  "devDependencies": {
    "@types/bun": "^1.3.13",
    "typescript": "^5.4.0"
  }
}
```

- [ ] **Step 3: Widen the CLI tsconfig include**

Edit `clients/cli/tsconfig.json`:

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "types": ["bun"]
  },
  "include": ["index.ts", "src/**/*.ts", "tests/**/*.ts"]
}
```

- [ ] **Step 4: Resolve workspace deps**

Run from repo root:

```bash
bun install
```

Expected: lockfile updates, no install errors. `clients/cli/node_modules/@silencer/lobby-sdk` resolves to `../../../lobby-sdk/ts` (or similar workspace symlink).

- [ ] **Step 5: Smoke-import the SDK from CLI tree**

Verify the resolution works:

```bash
cd clients/cli && bun -e 'import("@silencer/lobby-sdk").then(m => console.log(Object.keys(m).slice(0,3)))'
```

Expected output starts with `LobbyClient` (or another export name) — at minimum, no `Cannot find module` error.

- [ ] **Step 6: Commit**

```bash
git add package.json bun.lock clients/cli/package.json clients/cli/tsconfig.json
git commit -m "build(cli): wire @silencer/lobby-sdk as workspace dep"
```

---

## Task 2: Platform path resolution (`paths.ts`)

**Files:**
- Create: `clients/cli/src/lobby/paths.ts`
- Test: `clients/cli/tests/lobby/paths.test.ts`

- [ ] **Step 1: Write the failing tests**

Create `clients/cli/tests/lobby/paths.test.ts`:

```ts
import { describe, expect, test, beforeEach, afterEach } from "bun:test";
import { resolveLobbydDir, socketPath, logPath, MAX_SUN_PATH } from "../../src/lobby/paths.ts";

describe("resolveLobbydDir", () => {
  const origPlatform = process.platform;
  const origEnv = { ...process.env };
  afterEach(() => {
    Object.defineProperty(process, "platform", { value: origPlatform });
    process.env = { ...origEnv };
  });

  test("override env wins on every platform", () => {
    process.env.SILENCER_LOBBYD_DIR = "/custom/dir";
    Object.defineProperty(process, "platform", { value: "linux" });
    expect(resolveLobbydDir()).toBe("/custom/dir");
    Object.defineProperty(process, "platform", { value: "darwin" });
    expect(resolveLobbydDir()).toBe("/custom/dir");
    Object.defineProperty(process, "platform", { value: "win32" });
    expect(resolveLobbydDir()).toBe("/custom/dir");
  });

  test("linux uses XDG_RUNTIME_DIR/silencer when set", () => {
    delete process.env.SILENCER_LOBBYD_DIR;
    process.env.XDG_RUNTIME_DIR = "/run/user/1000";
    Object.defineProperty(process, "platform", { value: "linux" });
    expect(resolveLobbydDir()).toBe("/run/user/1000/silencer");
  });

  test("linux falls back to /tmp/silencer when XDG_RUNTIME_DIR unset", () => {
    delete process.env.SILENCER_LOBBYD_DIR;
    delete process.env.XDG_RUNTIME_DIR;
    Object.defineProperty(process, "platform", { value: "linux" });
    expect(resolveLobbydDir()).toBe("/tmp/silencer");
  });

  test("macOS uses TMPDIR/silencer", () => {
    delete process.env.SILENCER_LOBBYD_DIR;
    process.env.TMPDIR = "/var/folders/xx/yy/T/";
    Object.defineProperty(process, "platform", { value: "darwin" });
    expect(resolveLobbydDir()).toBe("/var/folders/xx/yy/T/silencer");
  });

  test("windows uses LOCALAPPDATA\\Silencer\\lobbyd", () => {
    delete process.env.SILENCER_LOBBYD_DIR;
    process.env.LOCALAPPDATA = "C:\\Users\\u\\AppData\\Local";
    Object.defineProperty(process, "platform", { value: "win32" });
    expect(resolveLobbydDir()).toBe("C:\\Users\\u\\AppData\\Local\\Silencer\\lobbyd");
  });
});

describe("socketPath / logPath", () => {
  test("socketPath is dir + lobbyd.sock", () => {
    expect(socketPath("/tmp/silencer")).toBe("/tmp/silencer/lobbyd.sock");
  });
  test("logPath is dir + lobbyd.log", () => {
    expect(logPath("/tmp/silencer")).toBe("/tmp/silencer/lobbyd.log");
  });
  test("socketPath throws when resolved path exceeds MAX_SUN_PATH on darwin", () => {
    const longDir = "/" + "a".repeat(120);
    const origPlatform = process.platform;
    Object.defineProperty(process, "platform", { value: "darwin" });
    expect(() => socketPath(longDir)).toThrow(/exceeds.*sun_path/);
    Object.defineProperty(process, "platform", { value: origPlatform });
  });
});

test("MAX_SUN_PATH is 100 (10-byte safety margin under macOS's 104)", () => {
  expect(MAX_SUN_PATH).toBe(100);
});
```

- [ ] **Step 2: Run the tests (expect failure)**

```bash
cd clients/cli && bun test tests/lobby/paths.test.ts
```

Expected: all tests fail with "Cannot find module".

- [ ] **Step 3: Implement `paths.ts`**

Create `clients/cli/src/lobby/paths.ts`:

```ts
// Resolves the co-located directory holding lobbyd's Unix socket and log
// file. One env override (SILENCER_LOBBYD_DIR) wins on every platform.
//
// macOS sun_path is 104 bytes; we cap the resolved socket path at 100
// to leave a margin for trailing-NUL accounting in different libc impls.

import { join } from "node:path";

export const MAX_SUN_PATH = 100;

export function resolveLobbydDir(): string {
    if (process.env.SILENCER_LOBBYD_DIR) return process.env.SILENCER_LOBBYD_DIR;
    switch (process.platform) {
        case "linux": {
            const xdg = process.env.XDG_RUNTIME_DIR;
            return xdg ? join(xdg, "silencer") : "/tmp/silencer";
        }
        case "darwin": {
            const tmp = process.env.TMPDIR ?? "/tmp/";
            return join(tmp, "silencer");
        }
        case "win32": {
            const local = process.env.LOCALAPPDATA;
            if (!local) throw new Error("LOCALAPPDATA not set");
            return join(local, "Silencer", "lobbyd");
        }
        default:
            return "/tmp/silencer";
    }
}

export function socketPath(dir: string): string {
    const p = join(dir, "lobbyd.sock");
    if (process.platform === "darwin" && p.length > MAX_SUN_PATH) {
        throw new Error(
            `socket path "${p}" (${p.length} bytes) exceeds macOS sun_path limit (${MAX_SUN_PATH}). ` +
            `Override with SILENCER_LOBBYD_DIR=<shorter dir>.`
        );
    }
    return p;
}

export function logPath(dir: string): string {
    return join(dir, "lobbyd.log");
}
```

- [ ] **Step 4: Run tests (expect green)**

```bash
cd clients/cli && bun test tests/lobby/paths.test.ts
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add clients/cli/src/lobby/paths.ts clients/cli/tests/lobby/paths.test.ts
git commit -m "feat(cli): platform path resolution for lobbyd"
```

---

## Task 3: RPC frame types (`protocol.ts`)

**Files:**
- Create: `clients/cli/src/lobby/protocol.ts`
- Test: `clients/cli/tests/lobby/protocol.test.ts`

- [ ] **Step 1: Write the failing tests**

Create `clients/cli/tests/lobby/protocol.test.ts`:

```ts
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
```

- [ ] **Step 2: Run tests (expect failure)**

```bash
cd clients/cli && bun test tests/lobby/protocol.test.ts
```

Expected: failure with "Cannot find module".

- [ ] **Step 3: Implement `protocol.ts`**

Create `clients/cli/src/lobby/protocol.ts`:

```ts
// JSON-lines RPC over AF_UNIX. One frame per newline-terminated line.

export type Request = {
    id: number;
    op: string;
    args: Record<string, unknown>;
    stream?: boolean;
};

export type Reply = {
    id: number;
    ok: boolean;
    result?: unknown;
    error?: string;
    code?: string;
    final: boolean;
};

export function encodeFrame(frame: Request | Reply): string {
    return JSON.stringify(frame) + "\n";
}

export function parseFrames<T>(buf: string): { frames: T[]; rest: string } {
    const frames: T[] = [];
    let rest = buf;
    for (;;) {
        const nl = rest.indexOf("\n");
        if (nl < 0) return { frames, rest };
        const line = rest.slice(0, nl);
        rest = rest.slice(nl + 1);
        if (line.length === 0) continue;
        try {
            frames.push(JSON.parse(line) as T);
        } catch (e) {
            throw new Error(`malformed RPC frame: ${line} (${(e as Error).message})`);
        }
    }
}
```

- [ ] **Step 4: Run tests (expect green)**

```bash
cd clients/cli && bun test tests/lobby/protocol.test.ts
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add clients/cli/src/lobby/protocol.ts clients/cli/tests/lobby/protocol.test.ts
git commit -m "feat(cli): JSON-lines RPC frame types for lobbyd"
```

---

## Task 4: Session manager (`session-manager.ts`)

The session manager owns the in-memory session map. It takes a `LobbyClient`-shaped factory so tests can inject a fake. This is the only file that talks to `@silencer/lobby-sdk` directly.

**Files:**
- Create: `clients/cli/src/lobby/session-manager.ts`
- Test: `clients/cli/tests/lobby/session-manager.test.ts`

- [ ] **Step 1: Write the failing tests (uses an in-memory fake LobbyClient)**

Create `clients/cli/tests/lobby/session-manager.test.ts`:

```ts
import { describe, expect, test } from "bun:test";
import { SessionManager, type LobbyLike } from "../../src/lobby/session-manager.ts";

type Listener = (...args: any[]) => void;

class FakeLobby implements LobbyLike {
  state: "disconnected" | "connecting" | "awaiting_version" | "awaiting_auth" | "authenticated" | "failed" = "disconnected";
  accountId = 0;
  lastError = "";
  private ls: Record<string, Set<Listener>> = {};
  on(event: string, fn: Listener): () => void {
    (this.ls[event] ??= new Set()).add(fn);
    return () => this.ls[event]?.delete(fn);
  }
  emit(event: string, ...args: unknown[]): void {
    for (const fn of this.ls[event] ?? []) fn(...args);
  }
  async connect(): Promise<void> { /* tests drive state manually */ }
  async disconnect(): Promise<void> { this.state = "disconnected"; this.emit("stateChanged", "disconnected"); }
  sendVersion(): void {}
  sendCredentials(): void {}
  sendChat(): void {}
  joinChannel(): void {}
  createGame(): void {}
  setGame(): void {}
}

const factory = (_cfg: unknown) => new FakeLobby();

describe("SessionManager", () => {
  test("spawn resolves with accountId on auth success", async () => {
    let lobby!: FakeLobby;
    const mgr = new SessionManager((cfg) => (lobby = new FakeLobby()));
    const p = mgr.spawn({ name: "alice", host: "h", port: 1, version: "v", platform: 0, user: "u", pass: "p" });
    // Drive the fake through the auth dance.
    await Promise.resolve();
    lobby.state = "awaiting_version"; lobby.emit("stateChanged", "awaiting_version");
    lobby.state = "awaiting_auth";    lobby.emit("stateChanged", "awaiting_auth");
    lobby.accountId = 42; lobby.state = "authenticated"; lobby.emit("stateChanged", "authenticated");
    await expect(p).resolves.toEqual({ accountId: 42 });
    expect(mgr.list().map((s) => s.name)).toEqual(["alice"]);
  });

  test("spawn rejects with lastError on failure", async () => {
    let lobby!: FakeLobby;
    const mgr = new SessionManager((cfg) => (lobby = new FakeLobby()));
    const p = mgr.spawn({ name: "alice", host: "h", port: 1, version: "v", platform: 0, user: "u", pass: "p" });
    await Promise.resolve();
    lobby.lastError = "bad password"; lobby.state = "failed"; lobby.emit("stateChanged", "failed");
    await expect(p).rejects.toThrow("bad password");
    expect(mgr.list()).toEqual([]);
  });

  test("kill removes the session and disconnects", async () => {
    let lobby!: FakeLobby;
    const mgr = new SessionManager((cfg) => (lobby = new FakeLobby()));
    const p = mgr.spawn({ name: "alice", host: "h", port: 1, version: "v", platform: 0, user: "u", pass: "p" });
    await Promise.resolve();
    lobby.accountId = 1; lobby.state = "authenticated"; lobby.emit("stateChanged", "authenticated");
    await p;
    await mgr.kill("alice");
    expect(lobby.state).toBe("disconnected");
    expect(mgr.list()).toEqual([]);
  });

  test("spawn rejects on duplicate name", async () => {
    const mgr = new SessionManager(() => new FakeLobby());
    const cfg = { name: "alice", host: "h", port: 1, version: "v", platform: 0 as 0, user: "u", pass: "p" };
    // Trigger the duplicate check synchronously by adding the session first.
    (mgr as any).sessions.set("alice", { name: "alice", lobby: new FakeLobby() });
    await expect(mgr.spawn(cfg)).rejects.toThrow(/already exists/);
  });

  test("kill on missing session throws NO_SESSION", async () => {
    const mgr = new SessionManager(() => new FakeLobby());
    await expect(mgr.kill("nobody")).rejects.toThrow(/NO_SESSION/);
  });

  test("getOrThrow returns the session or throws NO_SESSION", () => {
    const mgr = new SessionManager(() => new FakeLobby());
    expect(() => mgr.getOrThrow("nobody")).toThrow(/NO_SESSION/);
  });
});
```

- [ ] **Step 2: Run tests (expect failure)**

```bash
cd clients/cli && bun test tests/lobby/session-manager.test.ts
```

Expected: failure (module missing).

- [ ] **Step 3: Implement `session-manager.ts`**

Create `clients/cli/src/lobby/session-manager.ts`:

```ts
// In-memory session map for lobbyd. Each session wraps one LobbyClient.
//
// Decoupled from @silencer/lobby-sdk via the LobbyLike interface so tests
// can substitute an in-memory fake without standing up a real lobby server.

import type { LobbyClient as RealLobbyClient } from "@silencer/lobby-sdk";

export interface LobbyLike {
    readonly state: "disconnected" | "connecting" | "awaiting_version" | "awaiting_auth" | "authenticated" | "failed";
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
    host: string; port: number; version: string; platform: number;
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
            host: args.host, port: args.port, version: args.version, platform: args.platform,
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
                    else if (s === "authenticated") { cleanup(); resolve({ accountId: lobby.accountId }); }
                    else if (s === "failed") { cleanup(); reject(new Error(lobby.lastError || "auth failed")); }
                });
                const cleanup = () => { clearTimeout(timer); off(); };
                lobby.connect().catch((e) => { cleanup(); reject(e); });
            });
            return result;
        } catch (e) {
            this.sessions.delete(args.name);
            try { await lobby.disconnect(); } catch { /* ignore */ }
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

    size(): number { return this.sessions.size; }
}

// Convenience factory used by the real daemon. Lazily imports the SDK so
// unit tests don't pay the cost of resolving it.
export async function realLobbyFactory(): Promise<LobbyFactory> {
    const sdk = await import("@silencer/lobby-sdk");
    return (cfg) => new (sdk.LobbyClient as typeof RealLobbyClient)({
        host: cfg.host, port: cfg.port, version: cfg.version,
        platform: cfg.platform as 0 | 1 | 2,
    }) as unknown as LobbyLike;
}
```

- [ ] **Step 4: Run tests (expect green)**

```bash
cd clients/cli && bun test tests/lobby/session-manager.test.ts
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add clients/cli/src/lobby/session-manager.ts clients/cli/tests/lobby/session-manager.test.ts
git commit -m "feat(cli): in-memory session manager for lobbyd"
```

---

## Task 5: RPC server (`rpc-server.ts`)

The RPC server binds the unix socket, parses JSON-lines frames, and dispatches each `Request` to the `SessionManager`. Streaming `tail` requests subscribe to session events and emit `final: false` reply frames until the client disconnects.

**Files:**
- Create: `clients/cli/src/lobby/rpc-server.ts`
- Test: `clients/cli/tests/lobby/rpc-server.test.ts`

- [ ] **Step 1: Write the failing tests**

Create `clients/cli/tests/lobby/rpc-server.test.ts`:

```ts
import { describe, expect, test, beforeEach, afterEach } from "bun:test";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { SessionManager } from "../../src/lobby/session-manager.ts";
import { startRpcServer, type RpcServer } from "../../src/lobby/rpc-server.ts";
import { encodeFrame, parseFrames, type Reply, type Request } from "../../src/lobby/protocol.ts";

class FakeLobby {
  state: any = "disconnected"; accountId = 0; lastError = "";
  private ls: Record<string, Set<any>> = {};
  on(e: string, fn: any) { (this.ls[e] ??= new Set()).add(fn); return () => this.ls[e]?.delete(fn); }
  emit(e: string, ...a: any[]) { for (const fn of this.ls[e] ?? []) fn(...a); }
  async connect() { queueMicrotask(() => { this.state = "authenticated"; this.accountId = 7; this.emit("stateChanged", "authenticated"); }); }
  async disconnect() { this.state = "disconnected"; this.emit("stateChanged", "disconnected"); }
  sendVersion() {} sendCredentials() {} sendChat() {} joinChannel() {} createGame() {} setGame() {}
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
        open(s) { s.write(encodeFrame(req)); },
        data(s, chunk) {
          buf += new TextDecoder().decode(chunk);
          const { frames, rest } = parseFrames<Reply>(buf);
          buf = rest;
          for (const f of frames) {
            out.push(f);
            if (out.length >= expected) { s.end(); resolve(out); return; }
          }
        },
        close() { if (out.length < expected) reject(new Error(`closed early; got ${out.length}/${expected}`)); },
        error(_s, e) { reject(e); },
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
    const s = await rpc({ id: 2, op: "spawn", args: {
      name: "alice", host: "h", port: 1, version: "v", platform: 0, user: "u", pass: "p",
    }});
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
});
```

- [ ] **Step 2: Run tests (expect failure — module missing)**

```bash
cd clients/cli && bun test tests/lobby/rpc-server.test.ts
```

- [ ] **Step 3: Implement `rpc-server.ts`**

Create `clients/cli/src/lobby/rpc-server.ts`:

```ts
import { unlink } from "node:fs/promises";
import type { SessionManager } from "./session-manager.ts";
import { encodeFrame, parseFrames, type Reply, type Request } from "./protocol.ts";

export interface RpcServerOptions {
    socketPath: string;
    manager: SessionManager;
    onIdle?: () => void;
}

export interface RpcServer {
    stop(): Promise<void>;
    activeConnections(): number;
}

export async function startRpcServer(opts: RpcServerOptions): Promise<RpcServer> {
    // Best-effort cleanup of stale socket; bind will fail loudly if a live
    // peer is still listening, which is exactly what we want.
    await unlink(opts.socketPath).catch(() => {});

    let activeConns = 0;
    const tailUnsubs = new WeakMap<any, Array<() => void>>();

    const server = Bun.listen({
        unix: opts.socketPath,
        socket: {
            open(socket) {
                activeConns++;
                (socket as any).__buf = "";
            },
            data(socket, chunk: Uint8Array) {
                (socket as any).__buf += new TextDecoder().decode(chunk);
                let parsed;
                try {
                    parsed = parseFrames<Request>((socket as any).__buf);
                } catch (e) {
                    socket.write(encodeFrame({ id: 0, ok: false, error: (e as Error).message, code: "BAD_FRAME", final: true }));
                    socket.end();
                    return;
                }
                (socket as any).__buf = parsed.rest;
                for (const req of parsed.frames) handle(socket, req);
            },
            close(socket) {
                activeConns--;
                for (const off of tailUnsubs.get(socket) ?? []) off();
                tailUnsubs.delete(socket);
                if (activeConns === 0 && opts.manager.size() === 0) opts.onIdle?.();
            },
            error(_s, _e) { /* swallow; close will fire */ },
        },
    });

    function send(socket: any, reply: Reply): void {
        try { socket.write(encodeFrame(reply)); } catch { /* peer gone */ }
    }

    async function handle(socket: any, req: Request): Promise<void> {
        try {
            switch (req.op) {
                case "ls":
                    send(socket, { id: req.id, ok: true, result: { sessions: opts.manager.list() }, final: true });
                    return;
                case "spawn": {
                    const a = req.args as any;
                    const r = await opts.manager.spawn(a);
                    send(socket, { id: req.id, ok: true, result: r, final: true });
                    return;
                }
                case "kill": {
                    await opts.manager.kill((req.args as any).name);
                    send(socket, { id: req.id, ok: true, result: {}, final: true });
                    if (opts.manager.size() === 0 && activeConns <= 1) opts.onIdle?.();
                    return;
                }
                case "kill_all": {
                    await opts.manager.killAll();
                    send(socket, { id: req.id, ok: true, result: {}, final: true });
                    opts.onIdle?.();
                    return;
                }
                case "chat": {
                    const a = req.args as any;
                    opts.manager.getOrThrow(a.name).lobby.sendChat(a.channel, a.text);
                    send(socket, { id: req.id, ok: true, result: {}, final: true });
                    return;
                }
                case "join_channel": {
                    const a = req.args as any;
                    opts.manager.getOrThrow(a.name).lobby.joinChannel(a.channel);
                    send(socket, { id: req.id, ok: true, result: {}, final: true });
                    return;
                }
                case "game_create": {
                    const a = req.args as any;
                    const session = opts.manager.getOrThrow(a.name);
                    const off = session.lobby.on("newGame", (e: any) => {
                        off();
                        send(socket, { id: req.id, ok: true, result: { gameId: e.game.id }, final: true });
                    });
                    session.lobby.createGame(a.game);
                    return;
                }
                case "game_join": {
                    const a = req.args as any;
                    opts.manager.getOrThrow(a.name).lobby.setGame(a.gameId, 0 /* Lobby */);
                    send(socket, { id: req.id, ok: true, result: {}, final: true });
                    return;
                }
                case "tail": {
                    const a = req.args as any;
                    const session = opts.manager.getOrThrow(a.name);
                    const offs: Array<() => void> = [];
                    for (const ev of ["chat", "presence", "channel", "newGame", "delGame", "stateChanged"]) {
                        offs.push(session.lobby.on(ev, (data: unknown) => {
                            send(socket, { id: req.id, ok: true, result: { event: ev, data }, final: false });
                        }));
                    }
                    tailUnsubs.set(socket, [...(tailUnsubs.get(socket) ?? []), ...offs]);
                    return;
                }
                default:
                    send(socket, { id: req.id, ok: false, error: `unknown op: ${req.op}`, code: "BAD_OP", final: true });
            }
        } catch (e) {
            const msg = e instanceof Error ? e.message : String(e);
            const code = msg.startsWith("NO_SESSION") ? "NO_SESSION" : "ERR";
            send(socket, { id: req.id, ok: false, error: msg, code, final: true });
        }
    }

    return {
        async stop() {
            server.stop(true);
            await unlink(opts.socketPath).catch(() => {});
        },
        activeConnections() { return activeConns; },
    };
}
```

- [ ] **Step 4: Run tests (expect green)**

```bash
cd clients/cli && bun test tests/lobby/rpc-server.test.ts
```

Expected: all four tests pass.

- [ ] **Step 5: Commit**

```bash
git add clients/cli/src/lobby/rpc-server.ts clients/cli/tests/lobby/rpc-server.test.ts
git commit -m "feat(cli): unix-socket RPC server for lobbyd"
```

---

## Task 6: RPC client (`rpc-client.ts`)

The CLI's view of the daemon: open a socket, write one request, return either one reply (one-shot) or an `AsyncIterable<Reply>` (streaming).

**Files:**
- Create: `clients/cli/src/lobby/rpc-client.ts`
- Test: `clients/cli/tests/lobby/rpc-client.test.ts`

- [ ] **Step 1: Write the failing tests (uses a tiny fake server)**

Create `clients/cli/tests/lobby/rpc-client.test.ts`:

```ts
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
      open(s) { (s as any).__b = ""; },
      data(s, chunk) {
        (s as any).__b += new TextDecoder().decode(chunk);
        const { frames, rest } = parseFrames<Request>((s as any).__b);
        (s as any).__b = rest;
        for (const f of frames) {
          const out = reply(f);
          for (const r of Array.isArray(out) ? out : [out]) s.write(encodeFrame(r));
        }
      },
      close() {}, error() {},
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
    for await (const r of rpcStream(sock, { id: 1, op: "tail", args: {}, stream: true })) out.push(r);
    expect(out.map((r) => (r.result as any).event ?? "<final>")).toEqual(["a", "b", "<final>"]);
  });
});
```

- [ ] **Step 2: Run tests (expect failure)**

```bash
cd clients/cli && bun test tests/lobby/rpc-client.test.ts
```

- [ ] **Step 3: Implement `rpc-client.ts`**

Create `clients/cli/src/lobby/rpc-client.ts`:

```ts
import { encodeFrame, parseFrames, type Reply, type Request } from "./protocol.ts";

export async function rpcCall(socketPath: string, req: Request): Promise<Reply> {
    for await (const r of rpcStream(socketPath, req)) {
        if (r.final) return r;
    }
    throw new Error("daemon closed connection without a final reply");
}

export function rpcStream(socketPath: string, req: Request): AsyncIterable<Reply> {
    return {
        [Symbol.asyncIterator]() {
            const queue: Reply[] = [];
            let waiters: Array<(r: IteratorResult<Reply>) => void> = [];
            let done = false;
            let errored: unknown = null;
            let buf = "";
            let socket: any;

            const push = (r: Reply) => {
                if (waiters.length) waiters.shift()!({ value: r, done: false });
                else queue.push(r);
                if (r.final) finish();
            };
            const finish = () => {
                done = true;
                for (const w of waiters) w({ value: undefined, done: true });
                waiters = [];
                try { socket?.end(); } catch { /* ignore */ }
            };
            const fail = (e: unknown) => {
                errored = e;
                done = true;
                for (const w of waiters) w({ value: undefined as any, done: true });
                waiters = [];
            };

            Bun.connect({
                unix: socketPath,
                socket: {
                    open(s: any) { socket = s; s.write(encodeFrame(req)); },
                    data(_s: any, chunk: Uint8Array) {
                        buf += new TextDecoder().decode(chunk);
                        let parsed;
                        try { parsed = parseFrames<Reply>(buf); }
                        catch (e) { fail(e); return; }
                        buf = parsed.rest;
                        for (const f of parsed.frames) push(f);
                    },
                    close() { if (!done) fail(new Error("daemon closed connection")); },
                    error(_s: any, e: Error) { fail(e); },
                },
            }).catch(fail);

            return {
                async next(): Promise<IteratorResult<Reply>> {
                    if (errored) throw errored;
                    if (queue.length) return { value: queue.shift()!, done: false };
                    if (done) return { value: undefined as any, done: true };
                    return new Promise((resolve) => waiters.push(resolve));
                },
            };
        },
    };
}
```

- [ ] **Step 4: Run tests (expect green)**

```bash
cd clients/cli && bun test tests/lobby/rpc-client.test.ts
```

- [ ] **Step 5: Commit**

```bash
git add clients/cli/src/lobby/rpc-client.ts clients/cli/tests/lobby/rpc-client.test.ts
git commit -m "feat(cli): unix-socket RPC client for lobbyd"
```

---

## Task 7: Auto-spawn (`spawn.ts`)

Detect whether the daemon is up; if not, fork a detached Bun process running `daemon.ts` and poll the socket until it accepts a connection.

**Files:**
- Create: `clients/cli/src/lobby/spawn.ts`

(No unit tests for this module — it's a thin glue around `Bun.spawn` and the filesystem; covered by the daemon integration test in Task 9.)

- [ ] **Step 1: Implement `spawn.ts`**

Create `clients/cli/src/lobby/spawn.ts`:

```ts
import { existsSync, mkdirSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { resolveLobbydDir, socketPath } from "./paths.ts";

const POLL_INTERVAL_MS = 50;
const POLL_TIMEOUT_MS = 5_000;

async function probe(sock: string): Promise<boolean> {
    try {
        const s = await Bun.connect({
            unix: sock,
            socket: { open(s) { s.end(); }, data() {}, close() {}, error() {} },
        });
        return true;
    } catch {
        return false;
    }
}

/** Returns the resolved socket path, spawning the daemon if not yet listening. */
export async function ensureDaemon(): Promise<string> {
    const dir = resolveLobbydDir();
    mkdirSync(dir, { recursive: true });
    const sock = socketPath(dir);
    if (await probe(sock)) return sock;

    // Daemon entry lives next to this file at runtime.
    const here = fileURLToPath(import.meta.url);
    const daemonEntry = resolve(dirname(here), "daemon.ts");

    Bun.spawn({
        cmd: ["bun", daemonEntry],
        stdio: ["ignore", "ignore", "ignore"],
        // Detach so the parent CLI can exit while the daemon stays up.
        // unref() lets node-style event-loop drain even if we forget to wait.
    }).unref();

    const deadline = Date.now() + POLL_TIMEOUT_MS;
    while (Date.now() < deadline) {
        if (await probe(sock)) return sock;
        await new Promise((r) => setTimeout(r, POLL_INTERVAL_MS));
    }
    throw new Error(`lobbyd did not start within ${POLL_TIMEOUT_MS}ms (socket: ${sock})`);
}
```

- [ ] **Step 2: Typecheck**

```bash
cd clients/cli && bun run typecheck
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add clients/cli/src/lobby/spawn.ts
git commit -m "feat(cli): auto-spawn detached lobbyd"
```

---

## Task 8: Daemon entry point (`daemon.ts`)

Wires `SessionManager` + `RpcServer`, installs signal handlers, and exits when idle (no sessions AND no active connections).

**Files:**
- Create: `clients/cli/src/lobby/daemon.ts`

- [ ] **Step 1: Implement `daemon.ts`**

Create `clients/cli/src/lobby/daemon.ts`:

```ts
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
        const probe = await Bun.connect({
            unix: sock, socket: { open(s) { s.end(); }, data() {}, close() {}, error() {} },
        });
        logLine("another lobbyd already listening; exiting");
        process.exit(0);
    } catch { /* expected */ }

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
        onIdle: () => { void shutdown("idle"); },
    });

    process.on("SIGINT", () => void shutdown("SIGINT"));
    process.on("SIGTERM", () => void shutdown("SIGTERM"));
    logLine(`listening on ${sock}`);
}

main().catch((e) => {
    process.stderr.write(`[lobbyd] fatal: ${e instanceof Error ? e.stack ?? e.message : String(e)}\n`);
    process.exit(1);
});
```

- [ ] **Step 2: Typecheck**

```bash
cd clients/cli && bun run typecheck
```

- [ ] **Step 3: Commit**

```bash
git add clients/cli/src/lobby/daemon.ts
git commit -m "feat(cli): lobbyd entry point with idle exit"
```

---

## Task 9: CLI commands and dispatch wire-up (`commands.ts` + `index.ts`)

**Files:**
- Create: `clients/cli/src/lobby/commands.ts`
- Modify: `clients/cli/index.ts`
- Test: `clients/cli/tests/lobby/daemon-integration.test.ts`

- [ ] **Step 1: Write the integration test (drives the real daemon)**

Create `clients/cli/tests/lobby/daemon-integration.test.ts`:

```ts
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
  state: any = "disconnected"; accountId = 0; lastError = "";
  ls: Record<string, Set<any>> = {};
  on(e: string, fn: any) { (this.ls[e] ??= new Set()).add(fn); return () => this.ls[e]?.delete(fn); }
  emit(e: string, ...a: any[]) { for (const fn of this.ls[e] ?? []) fn(...a); }
  async connect() { queueMicrotask(() => { this.state = "authenticated"; this.accountId = 99; this.emit("stateChanged", "authenticated"); }); }
  async disconnect() { this.state = "disconnected"; this.emit("stateChanged", "disconnected"); }
  sendVersion() {} sendCredentials() {}
  sendChat(channel: string, text: string) { this.emit("chat", { channel, text, color: 0, brightness: 0 }); }
  joinChannel(c: string) { this.emit("channel", c); }
  createGame(g: any) { this.emit("newGame", { status: 1, game: { ...g, id: 12345 } }); }
  setGame() {}
}

let tmp: string; let sock: string; let server: RpcServer;
beforeEach(async () => {
  tmp = mkdtempSync(join(tmpdir(), "lobbyd-int-"));
  sock = join(tmp, "lobbyd.sock");
  const mgr = new SessionManager(() => new FakeLobby());
  server = await startRpcServer({ socketPath: sock, manager: mgr });
});
afterEach(async () => { await server.stop(); rmSync(tmp, { recursive: true, force: true }); });

describe("daemon integration", () => {
  test("spawn → chat → tail flow", async () => {
    const spawn = await rpcCall(sock, {
      id: 1, op: "spawn",
      args: { name: "alice", host: "h", port: 1, version: "v", platform: 0, user: "u", pass: "p" },
    });
    expect(spawn.ok).toBe(true);

    // Open a tail before sending chat so we observe the emitted event.
    const tailIter = rpcStream(sock, { id: 2, op: "tail", args: { name: "alice" }, stream: true });
    const events: any[] = [];
    const consumer = (async () => {
      for await (const r of tailIter) {
        if (r.final) break;
        events.push(r.result);
      }
    })();
    // Give the tail RPC a tick to register listeners.
    await new Promise((r) => setTimeout(r, 20));

    const chat = await rpcCall(sock, { id: 3, op: "chat", args: { name: "alice", channel: "main", text: "hi" } });
    expect(chat.ok).toBe(true);

    // Game create should resolve once the fake echoes a newGame event.
    const create = await rpcCall(sock, { id: 4, op: "game_create", args: { name: "alice", game: { id: 0 } } });
    expect(create.ok).toBe(true);
    expect((create.result as any).gameId).toBe(12345);

    // Trigger end-of-tail by killing the session.
    await rpcCall(sock, { id: 5, op: "kill", args: { name: "alice" } });
    await consumer;

    const seenChat = events.some((e) => e.event === "chat" && e.data.text === "hi");
    expect(seenChat).toBe(true);
  });
});
```

- [ ] **Step 2: Run the test (expect failure: handlers don't exist yet — but actually the server already implements them, so this should pass)**

```bash
cd clients/cli && bun test tests/lobby/daemon-integration.test.ts
```

If passing: good — the server already covers this. Move on. If failing: fix `rpc-server.ts` until green before continuing.

- [ ] **Step 3: Implement `commands.ts`**

Create `clients/cli/src/lobby/commands.ts`:

```ts
// CLI subcommand handlers for the `lobby` namespace. Each handler returns
// { clean, result } in the same shape as LOCAL_OPS in index.ts. Anything
// stateful goes through the daemon; we ensure it's running first.

import { rpcCall, rpcStream } from "./rpc-client.ts";
import { ensureDaemon } from "./spawn.ts";

type Handler = (args: Record<string, unknown>) => Promise<{ clean: boolean; result: unknown }>;

let nextId = 1;
function reqId(): number { return nextId++; }

async function call(op: string, args: Record<string, unknown>): Promise<{ clean: boolean; result: unknown }> {
    const sock = await ensureDaemon();
    const r = await rpcCall(sock, { id: reqId(), op, args });
    if (!r.ok) {
        process.stderr.write(`[${r.code ?? "ERR"}] ${r.error ?? ""}\n`);
        return { clean: false, result: { error: r.error, code: r.code } };
    }
    return { clean: true, result: r.result ?? {} };
}

export const LOBBY_HANDLERS: Record<string, Handler> = {
    spawn: async (args) => {
        const required = ["as", "user", "pass", "version", "host", "port"] as const;
        for (const k of required) if (args[k] === undefined) throw new Error(`missing --${k}`);
        return call("spawn", {
            name: args["as"],
            user: args["user"],
            pass: args["pass"],
            version: args["version"],
            host: args["host"],
            port: Number(args["port"]),
            platform: Number(args["platform"] ?? 0),
        });
    },
    ls: async (_args) => call("ls", {}),
    kill: async (args) => {
        if (args["all"]) return call("kill_all", {});
        if (!args["as"]) throw new Error("kill requires --as <name> or --all");
        return call("kill", { name: args["as"] });
    },
    chat: async (args) => {
        if (!args["as"] || !args["channel"] || args["text"] === undefined) {
            throw new Error("chat requires --as --channel --text");
        }
        return call("chat", { name: args["as"], channel: args["channel"], text: args["text"] });
    },
    join_channel: async (args) => {
        if (!args["as"] || !args["channel"]) throw new Error("join_channel requires --as --channel");
        return call("join_channel", { name: args["as"], channel: args["channel"] });
    },
    game: async (args) => {
        const sub = args["_subgame"] as string | undefined;
        if (sub === "create") {
            if (!args["as"] || !args["name"]) throw new Error("game create requires --as and --name");
            return call("game_create", {
                name: args["as"],
                game: {
                    id: 0,
                    name: args["name"],
                    password: args["password"] ?? "",
                    mapName: args["map"] ?? "",
                    maxPlayers: Number(args["max_players"] ?? 8),
                    maxTeams: Number(args["max_teams"] ?? 2),
                    minLevel: 0, maxLevel: 0, securityLevel: 0,
                    extra: 0, players: 0, state: 0,
                    accountId: 0, hostname: "", mapHash: new Uint8Array(20), port: 0,
                },
            });
        }
        if (sub === "join") {
            if (!args["as"] || args["id"] === undefined) throw new Error("game join requires --as --id");
            return call("game_join", { name: args["as"], gameId: Number(args["id"]) });
        }
        throw new Error(`unknown game subcommand: ${sub}`);
    },
    tail: async (args) => {
        if (!args["as"]) throw new Error("tail requires --as <name>");
        const sock = await ensureDaemon();
        for await (const r of rpcStream(sock, { id: reqId(), op: "tail", args: { name: args["as"] }, stream: true })) {
            if (r.final) break;
            process.stdout.write(JSON.stringify(r.result) + "\n");
        }
        return { clean: true, result: {} };
    },
};
```

- [ ] **Step 4: Wire `lobby` into `index.ts`**

Edit `clients/cli/index.ts` — three small changes:

1. At the top of the file, add the import:
```ts
import { LOBBY_HANDLERS } from "./src/lobby/commands.ts";
```

2. Add `"lobby"` to `NOUN_FIRST_OPS`:
```ts
const NOUN_FIRST_OPS = new Set(["keybind", "gas", "lobby"]);
```

3. Add the lobby branch into `LOCAL_OPS` (so it goes through the local-op fast-path and never opens the silencer game's control port):
```ts
const LOCAL_OPS: Record<string, Record<string, LocalHandler>> = {
  gas: {
    validate: async (args) => {
      const dir = (args["dir"] as string | undefined) ?? (args["_positional"] as string | undefined);
      if (!dir) throw new Error("gas validate requires a directory: silencer-cli gas validate <dir>");
      const { validateDirectory } = await import("@silencer/gas-validation/node");
      const res = await validateDirectory(dir);
      return { clean: res.ok, result: res };
    },
  },
  lobby: LOBBY_HANDLERS,
};
```

4. In `STRING_FLAGS`, register the lobby flags that must stay strings (passwords with digits, account names, etc.):
```ts
const STRING_FLAGS: Record<string, Record<string, Set<string>>> = {
  keybind: {
    get:    new Set(["profile", "action"]),
    put:    new Set(["profile", "action"]),
    unset:  new Set(["profile", "action"]),
    use:    new Set(["profile"]),
    new:    new Set(["profile", "from"]),
    delete: new Set(["profile"]),
  },
  gas: {
    validate: new Set(["dir"]),
  },
  lobby: {
    spawn:        new Set(["as", "user", "pass", "version", "host"]),
    chat:         new Set(["as", "channel", "text"]),
    join_channel: new Set(["as", "channel"]),
    kill:         new Set(["as"]),
    game:         new Set(["as", "name", "map", "password"]),
    tail:         new Set(["as"]),
  },
};
```

5. Update the `usage()` string. Append before the env-vars footer:
```
       silencer-cli lobby spawn --as alice --host H --port P --version V --user U --pass P
       silencer-cli lobby ls
       silencer-cli lobby chat --as alice --channel main --text "hi"
       silencer-cli lobby game create --as alice --name TEST [--map M --max-players 8]
       silencer-cli lobby game join   --as alice --id 12345
       silencer-cli lobby tail --as alice
       silencer-cli lobby kill --as alice | --all
       silencer-cli lobby join_channel --as alice --channel main
```

6. Handle the `game` sub-subcommand. After the existing positional-handling block in `parseArgs`, add a branch that captures the third positional when `op === "lobby" && subop === "game"` into `args["_subgame"]`:
```ts
      // lobby game <create|join>: third positional is the sub-subcommand.
      else if (op === "lobby" && subop === "game" && args["_subgame"] === undefined) {
        args["_subgame"] = a;
      }
```

- [ ] **Step 5: Typecheck and run all tests**

```bash
cd clients/cli && bun run typecheck && bun test
```

Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add clients/cli/src/lobby/commands.ts clients/cli/index.ts clients/cli/tests/lobby/daemon-integration.test.ts
git commit -m "feat(cli): lobby namespace — spawn/chat/game/tail/kill/ls"
```

---

## Task 10: Docs + end-to-end smoke + finalize

**Files:**
- Create: `clients/cli/src/lobby/CLAUDE.md` (+ `AGENTS.md` symlink)
- Modify: `clients/cli/CLAUDE.md`

- [ ] **Step 1: Write the lobby CLAUDE.md**

Create `clients/cli/src/lobby/CLAUDE.md`:

```markdown
# clients/cli/src/lobby — agent-driven fake lobby presences

Single supervisor daemon (`silencer-lobbyd`) holds N `@silencer/lobby-sdk`
`LobbyClient` instances keyed by session name. CLI subcommands are thin
clients: open the unix socket, send one JSON-lines request, exit.

## Files

- `paths.ts` — co-located dir resolution
  (`SILENCER_LOBBYD_DIR` overrides; defaults: `$XDG_RUNTIME_DIR/silencer/`
  on Linux, `$TMPDIR/silencer/` on macOS, `%LOCALAPPDATA%\Silencer\lobbyd\`
  on Windows). Both `lobbyd.sock` and `lobbyd.log` live here.
- `protocol.ts` — `Request` / `Reply` JSON-lines frames.
- `session-manager.ts` — in-memory session map; takes a `LobbyLike`
  factory so tests can inject fakes.
- `rpc-server.ts` — `Bun.listen({ unix })`; dispatches to the manager.
- `rpc-client.ts` — `Bun.connect({ unix })`; one-shot + streaming.
- `spawn.ts` — auto-spawn detached `silencer-lobbyd`, poll the socket.
- `daemon.ts` — entry point. Auto-exits when sessions=0 AND conns=0.
- `commands.ts` — handlers wired into `index.ts`'s `LOCAL_OPS.lobby`.

## Lifecycle

1. First `lobby spawn` → `ensureDaemon` probes the socket. Refused → forks
   detached `bun src/lobby/daemon.ts` and polls until it accepts.
2. Daemon serves until `kill_all` or until `kill` removes the last
   session AND the last connection drops.
3. Stale socket file (crashed daemon) is unlinked on the next bind.

## Invariants

- Creds (`--user`, `--pass`) are passed once to `spawn`; subsequent calls
  use only `--as <name>`. Passwords never touch disk.
- macOS sun_path is 104 bytes; `paths.ts` caps the resolved socket path
  at 100 and throws a clear error otherwise.
- One process, N sessions: per-player overhead is just a `LobbyClient`
  instance + its TCP socket. The Bun runtime cost is amortized once.

## When NOT to touch this dir

- If you're changing the wire protocol (adding lobby opcodes), edit
  `services/lobby/protocol.go` first, then `clients/lobby-sdk/ts`, then
  `shared/lobby-protocol/vectors.json`. This module consumes the SDK
  and shouldn't grow protocol knowledge of its own.
- If you're adding non-lobby CLI features, they don't belong here. The
  `lobby` namespace is the only thing in this dir.
```

- [ ] **Step 2: Add the AGENTS.md symlink (one-line stub on Windows)**

```bash
cd clients/cli/src/lobby && ln -s CLAUDE.md AGENTS.md
```

- [ ] **Step 3: Update the parent CLAUDE.md to mention the new namespace**

Edit `clients/cli/CLAUDE.md`. After the existing `Run` section, append:

```markdown
## Lobby fake players

`lobby` namespace spawns persistent authenticated lobby presences in a
shared supervisor daemon. See [`src/lobby/CLAUDE.md`](src/lobby/CLAUDE.md).

```bash
silencer-cli lobby spawn --as alice --host LOBBY_HOST --port 15170 \
                         --version 1.2.3 --user alice --pass hunter2
silencer-cli lobby chat  --as alice --channel main --text "hi"
silencer-cli lobby tail  --as alice    # streams events until SIGINT
silencer-cli lobby kill  --as alice
```

Defaults co-locate socket+log at `$SILENCER_LOBBYD_DIR` (override) or the
platform default (`$XDG_RUNTIME_DIR/silencer/` on Linux,
`$TMPDIR/silencer/` on macOS, `%LOCALAPPDATA%\Silencer\lobbyd\` on
Windows).
```

- [ ] **Step 4: End-to-end smoke against a real lobby**

Bring up a local lobby (`cd services/lobby && go run . -version=""`). In another shell:

```bash
cd clients/cli
bun ./index.ts lobby spawn --as alice --host 127.0.0.1 --port 15170 \
                           --version "" --user alice --pass alice
bun ./index.ts lobby ls
bun ./index.ts lobby chat --as alice --channel main --text "hello"
bun ./index.ts lobby kill --as alice
```

Expected:
- `spawn` prints `{"accountId":<n>}` and exits 0.
- `ls` shows alice with `state: "authenticated"`.
- `chat` prints `{}` and exits 0.
- `kill` prints `{}` and exits 0.
- After ~100ms, `silencer-lobbyd` process is gone (`pgrep -f silencer-lobbyd` empty).

Then verify auto-spawn works on a fresh shell: `bun ./index.ts lobby ls` from a clean state should silently start the daemon and return `{"sessions":[]}`.

- [ ] **Step 5: Final typecheck + tests + format**

```bash
cd clients/cli && bun run fmt && bun run typecheck && bun test
```

Expected: format clean, typecheck clean, tests green.

- [ ] **Step 6: Commit**

```bash
git add clients/cli/src/lobby/CLAUDE.md clients/cli/src/lobby/AGENTS.md clients/cli/CLAUDE.md
git commit -m "docs(cli): document lobby namespace and lobbyd"
```

---

## Self-review notes (verified against spec)

- **Spec coverage** — every requirement maps to a task: workspace registration (T1), platform paths + sun_path guard (T2), RPC protocol (T3), session lifecycle incl. duplicate/timeout/auth-fail (T4), unix server with all ops including streaming tail (T5), client with one-shot + streaming (T6), auto-spawn (T7), idle exit + signal handlers (T8), CLI namespace incl. `game create`/`game join` (T9), docs + smoke (T10).
- **Type consistency** — `LobbyLike` is the single interface across manager/server/tests. RPC ops names are consistent (`spawn`, `kill`, `kill_all`, `ls`, `chat`, `join_channel`, `game_create`, `game_join`, `tail`) — used identically in server, client handlers, and CLI dispatch.
- **No placeholders** — every step has the actual code or command. Test code is full and runnable.
- **Known caveats baked into the plan**:
  - macOS sun_path: hard-asserted at 100 bytes with a clear error suggesting `SILENCER_LOBBYD_DIR`.
  - Stale socket: unlinked on bind (server) and detected on probe (auto-spawn).
  - Auto-exit race: daemon checks idle on every `close` AND on every successful `kill`/`kill_all`; first idle wins.
  - The integration test (T9) avoids forking a real child process so it stays hermetic; the actual fork path is exercised in the smoke step (T10 step 4).
