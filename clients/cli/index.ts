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
      `\n` +
      `Env: SILENCER_CONTROL_HOST (default 127.0.0.1)\n` +
      `     SILENCER_CONTROL_PORT (default 5170)`,
  );
  process.exit(2);
}

function parseArgs(argv: string[]): { host: string; port: number; op: string; args: Record<string, unknown> } {
  let host = process.env.SILENCER_CONTROL_HOST ?? "127.0.0.1";
  let port = Number.parseInt(process.env.SILENCER_CONTROL_PORT ?? "5170", 10);
  const args: Record<string, unknown> = {};
  let op: string | null = null;
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i]!;
    if (a === "--host") {
      host = argv[++i] ?? usage();
    } else if (a === "--port") {
      port = Number.parseInt(argv[++i] ?? usage(), 10);
    } else if (a.startsWith("--")) {
      const key = a.slice(2).replace(/-/g, "_");
      const next = argv[i + 1];
      if (next === undefined || next.startsWith("--")) {
        args[key] = true;
      } else {
        const num = Number(next);
        args[key] = Number.isFinite(num) && next.match(/^-?\d+(\.\d+)?$/) ? num : next;
        i++;
      }
    } else if (op === null) {
      op = a;
    } else {
      // positional after op → treat as label/text shorthand for click/set_text/select
      if (op === "click" && args["label"] === undefined) args["label"] = a;
      else if ((op === "set_text" || op === "select") && args["label"] === undefined) args["label"] = a;
      else if (op === "set_text" && args["text"] === undefined) args["text"] = a;
      else if (op === "select" && args["index"] === undefined) {
        const num = Number(a);
        args[Number.isInteger(num) ? "index" : "text"] = Number.isInteger(num) ? num : a;
      }
    }
  }
  if (!op) usage();
  return { host, port, op, args };
}

async function main() {
  const { host, port, op, args } = parseArgs(process.argv.slice(2));
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
