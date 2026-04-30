import { describe, expect, test } from "bun:test";
import { SessionManager, type LobbyLike } from "../../src/lobby/session-manager.ts";

type Listener = (...args: any[]) => void;

class FakeLobby implements LobbyLike {
  state:
    | "disconnected"
    | "connecting"
    | "awaiting_version"
    | "awaiting_auth"
    | "authenticated"
    | "failed" = "disconnected";
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
  async connect(): Promise<void> {
    /* tests drive state manually */
  }
  async disconnect(): Promise<void> {
    this.state = "disconnected";
    this.emit("stateChanged", "disconnected");
  }
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
    const p = mgr.spawn({
      name: "alice",
      host: "h",
      port: 1,
      version: "v",
      platform: 0,
      user: "u",
      pass: "p",
    });
    // Drive the fake through the auth dance.
    await Promise.resolve();
    lobby.state = "awaiting_version";
    lobby.emit("stateChanged", "awaiting_version");
    lobby.state = "awaiting_auth";
    lobby.emit("stateChanged", "awaiting_auth");
    lobby.accountId = 42;
    lobby.state = "authenticated";
    lobby.emit("stateChanged", "authenticated");
    await expect(p).resolves.toEqual({ accountId: 42 });
    expect(mgr.list().map((s) => s.name)).toEqual(["alice"]);
  });

  test("spawn rejects with lastError on failure", async () => {
    let lobby!: FakeLobby;
    const mgr = new SessionManager((cfg) => (lobby = new FakeLobby()));
    const p = mgr.spawn({
      name: "alice",
      host: "h",
      port: 1,
      version: "v",
      platform: 0,
      user: "u",
      pass: "p",
    });
    await Promise.resolve();
    lobby.lastError = "bad password";
    lobby.state = "failed";
    lobby.emit("stateChanged", "failed");
    await expect(p).rejects.toThrow("bad password");
    expect(mgr.list()).toEqual([]);
  });

  test("kill removes the session and disconnects", async () => {
    let lobby!: FakeLobby;
    const mgr = new SessionManager((cfg) => (lobby = new FakeLobby()));
    const p = mgr.spawn({
      name: "alice",
      host: "h",
      port: 1,
      version: "v",
      platform: 0,
      user: "u",
      pass: "p",
    });
    await Promise.resolve();
    lobby.accountId = 1;
    lobby.state = "authenticated";
    lobby.emit("stateChanged", "authenticated");
    await p;
    await mgr.kill("alice");
    const stateAfterKill: string = lobby.state;
    expect(stateAfterKill).toBe("disconnected");
    expect(mgr.list()).toEqual([]);
  });

  test("spawn rejects on duplicate name", async () => {
    const mgr = new SessionManager(() => new FakeLobby());
    const cfg = {
      name: "alice",
      host: "h",
      port: 1,
      version: "v",
      platform: 0 as 0,
      user: "u",
      pass: "p",
    };
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
