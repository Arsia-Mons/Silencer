import { unlink } from "node:fs/promises";
import type { ClientEvents } from "@silencer/lobby-sdk";
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

const TAIL_EVENTS = [
  "chat",
  "presence",
  "channel",
  "newGame",
  "delGame",
  "stateChanged",
] as const satisfies (keyof ClientEvents)[];

export async function startRpcServer(opts: RpcServerOptions): Promise<RpcServer> {
  // Best-effort cleanup of stale socket; bind will fail loudly if a live
  // peer is still listening, which is exactly what we want.
  await unlink(opts.socketPath).catch(() => {});

  let activeConns = 0;
  const tailUnsubs = new WeakMap<object, Array<() => void>>();

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
          socket.write(
            encodeFrame({
              id: 0,
              ok: false,
              error: (e as Error).message,
              code: "BAD_FRAME",
              final: true,
            }),
          );
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
      error(_s, _e) {
        /* swallow; close will fire */
      },
    },
  });

  function send(socket: any, reply: Reply): void {
    try {
      socket.write(encodeFrame(reply));
    } catch {
      /* peer gone */
    }
  }

  async function handle(socket: any, req: Request): Promise<void> {
    try {
      switch (req.op) {
        case "ls":
          send(socket, {
            id: req.id,
            ok: true,
            result: { sessions: opts.manager.list() },
            final: true,
          });
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
          for (const ev of TAIL_EVENTS) {
            offs.push(
              session.lobby.on(ev, (data: unknown) => {
                send(socket, {
                  id: req.id,
                  ok: true,
                  result: { event: ev, data },
                  final: false,
                });
              }),
            );
          }
          tailUnsubs.set(socket, [...(tailUnsubs.get(socket) ?? []), ...offs]);
          return;
        }
        default:
          send(socket, {
            id: req.id,
            ok: false,
            error: `unknown op: ${req.op}`,
            code: "BAD_OP",
            final: true,
          });
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
    activeConnections() {
      return activeConns;
    },
  };
}
