// CLI subcommand handlers for the `lobby` namespace. Each handler returns
// { clean, result } in the same shape as LOCAL_OPS in index.ts. Anything
// stateful goes through the daemon; we ensure it's running first.

import { rpcCall, rpcStream } from "./rpc-client.ts";
import { ensureDaemon } from "./spawn.ts";

type Handler = (args: Record<string, unknown>) => Promise<{ clean: boolean; result: unknown }>;

let nextId = 1;
function reqId(): number {
  return nextId++;
}

async function call(
  op: string,
  args: Record<string, unknown>,
): Promise<{ clean: boolean; result: unknown }> {
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
          minLevel: 0,
          maxLevel: 0,
          securityLevel: 0,
          extra: 0,
          players: 0,
          state: 0,
          accountId: 0,
          hostname: "",
          mapHash: new Uint8Array(20),
          port: 0,
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
    for await (const r of rpcStream(sock, {
      id: reqId(),
      op: "tail",
      args: { name: args["as"] },
      stream: true,
    })) {
      if (r.final) break;
      process.stdout.write(JSON.stringify(r.result) + "\n");
    }
    return { clean: true, result: {} };
  },
};
