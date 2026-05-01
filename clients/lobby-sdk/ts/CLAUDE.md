# clients/lobby-sdk/ts — TypeScript lobby SDK

Bun + TypeScript, no Node-specific runtime APIs (uses `Bun.connect`
for TCP and `node:crypto` for SHA-1). Per the universal rules: no
`.js` source, no `npm`/`pnpm`/`yarn`. Format with `oxfmt`.

## Build & test

```sh
bun install
bun test           # codec golden-vector tests
bun run typecheck  # tsc --noEmit
bun run fmt:check  # oxfmt --check
```

The example (`examples/chat-listener.ts`) connects to a real lobby
and prints chat — handy for end-to-end smoke checks against a local
lobby:

```sh
bun examples/chat-listener.ts 127.0.0.1 15170 1.2.3 alice hunter2
```

## Layout

- `src/types.ts` — wire-format types and enum-like consts
  (`Op`, `Platform`, `SecurityLevel`, `GameStatus`, `LobbyGame`,
  `MatchStats`, …).
- `src/codec.ts` — pure encoders/decoders. No I/O. Throws
  `CodecError` on malformed input.
- `src/client.ts` — `LobbyClient` class. Event-based API
  (`client.on("chat", …)`); connection lifecycle is fully async.
- `src/index.ts` — public re-exports. Importers should consume from
  `@silencer/lobby-sdk` (i.e. this file).

## Invariants

- **Opcodes & wire format** must match `services/lobby/protocol.go`
  byte-for-byte. The codec tests load
  `../../shared/lobby-protocol/vectors.json`.
- **`LobbyClient` listeners run synchronously** during the socket's
  `data` callback. Don't block in them; if you need async work,
  schedule it (`queueMicrotask`, `setTimeout`, etc.).
- **Strict TS.** `noUncheckedIndexedAccess` is on; non-null assertions
  inside the codec are deliberate (we've already done length checks).

## Gotchas

- `Bun.connect()` returns immediately and emits `open` later. The
  `LobbyClient.connect()` promise resolves once the TCP socket
  exists (not when authenticated) — callers should subscribe to the
  `stateChanged` event before awaiting.
- The version handshake is opt-in: pass `version: "..."` in the
  config to enable it. Empty string skips it (server-side check is
  also bypassable when `-version=""`).
- This package is **not published to npm** today. Consumers within
  the monorepo can import from the path directly; we'll add a
  workspace registration when there's a non-test consumer.
