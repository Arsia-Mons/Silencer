// Minimal example: connects to a lobby, authenticates, prints chat.
//
//   bun examples/chat-listener.ts <host> <port> <version> <username> <password>

import { LobbyClient, Platform } from "../src/index.ts";

const [, , host, port, version, username, password] = process.argv;
if (!host || !port || !version || !username || !password) {
    console.error("usage: chat-listener.ts <host> <port> <version> <username> <password>");
    process.exit(1);
}

const c = new LobbyClient({
    host,
    port: Number(port),
    version,
    platform: Platform.Unknown,
});

c.on("stateChanged", (s) => {
    console.error(`[state] ${s}`);
    if (s === "awaiting_version") c.sendVersion();
    if (s === "awaiting_auth") c.sendCredentials(username, password);
});
c.on("auth", (a) => {
    if (a.ok) console.error(`[auth] ok account_id=${a.accountId}`);
    else console.error(`[auth] FAIL: ${a.error}`);
});
c.on("motd", (m) => console.error(`[motd]\n${m}`));
c.on("channel", (ch) => console.error(`[channel] ${ch}`));
c.on("chat", (m) => console.log(`[${m.channel}] ${m.text}`));
c.on("presence", (p) =>
    console.error(`[presence] ${p.removed ? "leave" : "join"} ${p.name} acct=${p.accountId} game=${p.gameId}`));
c.on("error", (e) => console.error(`[error] ${e}`));

await c.connect();

const stop = async () => { await c.disconnect(); process.exit(0); };
process.on("SIGINT", stop);
process.on("SIGTERM", stop);
