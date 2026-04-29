#!/usr/bin/env bun

import { connect } from "node:net";

type Reply = {
  id: number;
  ok: boolean;
  result?: unknown;
  error?: string;
  code?: string;
};

function usage(): never {
  console.error(
    `usage: silencer-cli [--host H] [--port P] <op> [--key value ...]\n` +
      `       silencer-cli ping\n` +
      `       silencer-cli state\n` +
      `       silencer-cli inspect [--interface-id N]\n` +
      `       silencer-cli world_state\n` +
      `       silencer-cli click --label "OPTIONS"\n` +
      `       silencer-cli set_text --label TEXT_ID --text "hi"\n` +
      `       silencer-cli select --label LISTBOX --index 0\n` +
      `       silencer-cli back\n` +
      `       silencer-cli screenshot [--out /path/x.png]\n` +
      `       silencer-cli wait_for_state --state OPTIONS [--timeout-ms 5000]\n` +
      `       silencer-cli wait_frames --n 30\n` +
      `       silencer-cli wait_ms --n 500\n` +
      `       silencer-cli pause | resume\n` +
      `       silencer-cli step --frames 10 | --ms 200\n` +
      `       silencer-cli quit\n` +
      `       silencer-cli keybind list\n` +
      `       silencer-cli keybind actions\n` +
      `       silencer-cli keybind get [--profile N] [--action A]\n` +
      `       silencer-cli keybind put --profile N --action A --bindings KEY:F PAD:south\n` +
      `         (comma joins keys into an AND-chord, e.g. --bindings KEY:Up,KEY:Left)\n` +
      `       silencer-cli keybind unset --profile N --action A\n` +
      `       silencer-cli keybind use <profile>\n` +
      `       silencer-cli keybind new --profile N [--from M]\n` +
      `       silencer-cli keybind delete <profile>\n` +
      `       silencer-cli gas validate <dir>\n` +
      `         (runs locally; no daemon required. Exit 1 if errors[] non-empty.)\n` +
      `\n` +
      `Env: SILENCER_CONTROL_HOST (default 127.0.0.1)\n` +
      `     SILENCER_CONTROL_PORT (default 5170)`,
  );
  process.exit(2);
}

// Ops with a noun-first dispatch shape: `silencer-cli <op> <subop> [args]`.
// The wrapper recognizes the first positional as the op, the second as
// args.subop. The pattern is centralized so future namespaces (e.g.
// `profile`, `audio`) can opt in without touching the parser.
const NOUN_FIRST_OPS = new Set(["keybind", "gas"]);

// (op, subop) pairs that run entirely in this process and never touch
// the daemon. Each handler returns a JSON-serializable result and a
// boolean `clean` flag; `clean=false` exits non-zero so shell loops
// can branch on it.
type LocalHandler = (args: Record<string, unknown>) => Promise<{ clean: boolean; result: unknown }>;
const LOCAL_OPS: Record<string, Record<string, LocalHandler>> = {
  gas: {
    validate: async (args) => {
      const dir = (args["dir"] as string | undefined) ?? (args["_positional"] as string | undefined);
      if (!dir) throw new Error("gas validate requires a directory: silencer-cli gas validate <dir>");
      const { validateDirectory } = await import("@silencer/gas-validation");
      const res = await validateDirectory(dir);
      return { clean: res.ok, result: res };
    },
  },
};
// Per (op,subop) pair: which flag accepts a list of values rather than
// a single value. `--bindings KEY:A PAD:south` consumes both.
const VARIADIC_FLAGS: Record<string, Record<string, Set<string>>> = {
  keybind: {
    put: new Set(["bindings"]),
  },
};
// Per (op,subop) pair: flags whose values must stay strings even when they
// look numeric. Without this, `--profile 1` would JSON-encode as `{profile:1}`
// and the C++ side's `args.value("profile", default)` would return the default
// (silent operation on the wrong profile).
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
};
// Bindings within VARIADIC_FLAGS that accept comma-separated chord syntax:
// `--bindings KEY:Up,KEY:Left` becomes JSON `[["KEY:Up","KEY:Left"]]` (an
// AND-chord) instead of `["KEY:Up","KEY:Left"]` (two OR'd singles).
const CHORD_SPLIT_FLAGS: Record<string, Record<string, Set<string>>> = {
  keybind: {
    put: new Set(["bindings"]),
  },
};

function parseArgs(argv: string[]): { host: string; port: number; op: string; args: Record<string, unknown> } {
  let host = process.env.SILENCER_CONTROL_HOST ?? "127.0.0.1";
  let port = Number.parseInt(process.env.SILENCER_CONTROL_PORT ?? "5170", 10);
  const args: Record<string, unknown> = {};
  let op: string | null = null;
  let subop: string | null = null;
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i]!;
    if (a === "--host") {
      host = argv[++i] ?? usage();
    } else if (a === "--port") {
      port = Number.parseInt(argv[++i] ?? usage(), 10);
    } else if (a.startsWith("--")) {
      const key = a.slice(2).replace(/-/g, "_");
      const variadic = op && subop && VARIADIC_FLAGS[op]?.[subop]?.has(key);
      const chordSplit = op && subop && CHORD_SPLIT_FLAGS[op]?.[subop]?.has(key);
      const stringOnly = op && subop && STRING_FLAGS[op]?.[subop]?.has(key);
      if (variadic) {
        // Consume every following non-flag token as a list element. If the
        // flag accepts comma-chord syntax, a token like "KEY:Up,KEY:Left"
        // becomes a nested array (AND chord); plain tokens stay flat.
        const list: (string | string[])[] = [];
        while (i + 1 < argv.length && !argv[i + 1]!.startsWith("--")) {
          const tok = argv[++i]!;
          if (chordSplit && tok.includes(",")) {
            list.push(tok.split(",").filter((s) => s.length > 0));
          } else {
            list.push(tok);
          }
        }
        args[key] = list;
      } else {
        const next = argv[i + 1];
        if (next === undefined || next.startsWith("--")) {
          args[key] = true;
        } else if (stringOnly) {
          args[key] = next;
          i++;
        } else {
          const num = Number(next);
          args[key] = Number.isFinite(num) && next.match(/^-?\d+(\.\d+)?$/) ? num : next;
          i++;
        }
      }
    } else if (op === null) {
      op = a;
    } else if (NOUN_FIRST_OPS.has(op) && subop === null) {
      subop = a;
      args["subop"] = a;
    } else {
      // positional after op → treat as shorthand for the most common arg.
      if (op === "click" && args["label"] === undefined) args["label"] = a;
      else if ((op === "set_text" || op === "select") && args["label"] === undefined) args["label"] = a;
      else if (op === "set_text" && args["text"] === undefined) args["text"] = a;
      else if (op === "select" && args["index"] === undefined) {
        const num = Number(a);
        args[Number.isInteger(num) ? "index" : "text"] = Number.isInteger(num) ? num : a;
      }
      // keybind: third positional (after `keybind <subop>`) is the profile name
      // for `use` / `delete` (the most common single-positional shape).
      else if (op === "keybind" && (subop === "use" || subop === "delete") && args["profile"] === undefined) {
        args["profile"] = a;
      }
      // gas validate: third positional is the directory (allows
      // `silencer-cli gas validate shared/assets/gas/` w/o --dir).
      else if (op === "gas" && subop === "validate" && args["dir"] === undefined) {
        args["dir"] = a;
      }
    }
  }
  if (!op) usage();
  return { host, port, op, args };
}

async function main() {
  const { host, port, op, args } = parseArgs(process.argv.slice(2));

  // Local ops: run in-process, never open the control socket.
  const subop = args["subop"] as string | undefined;
  const local = subop ? LOCAL_OPS[op]?.[subop] : undefined;
  if (local) {
    const { clean, result } = await local(args);
    process.stdout.write(JSON.stringify(result) + "\n");
    process.exit(clean ? 0 : 1);
  }

  const id = Math.floor(Math.random() * 1_000_000) + 1;
  const payload = JSON.stringify({ id, op, args }) + "\n";

  const sock = connect({ host, port });
  await new Promise<void>((res, rej) => {
    sock.once("connect", () => res());
    sock.once("error", rej);
  });
  sock.write(payload);

  let buf = "";
  for await (const chunk of sock as AsyncIterable<Buffer>) {
    buf += chunk.toString("utf8");
    const nl = buf.indexOf("\n");
    if (nl >= 0) {
      const line = buf.slice(0, nl);
      sock.end();
      const reply = JSON.parse(line) as Reply;
      if (reply.ok) {
        process.stdout.write(JSON.stringify(reply.result ?? {}) + "\n");
        process.exit(0);
      } else {
        process.stderr.write(`[${reply.code ?? "ERR"}] ${reply.error ?? ""}\n`);
        process.exit(1);
      }
    }
  }
  process.stderr.write("[TRANSPORT] connection closed without reply\n");
  process.exit(2);
}

main().catch((e) => {
  process.stderr.write(`[TRANSPORT] ${e instanceof Error ? e.message : String(e)}\n`);
  process.exit(2);
});
