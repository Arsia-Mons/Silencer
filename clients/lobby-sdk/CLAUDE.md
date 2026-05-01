# clients/lobby-sdk — Lobby SDK (C++ + TypeScript)

Two parallel client libraries that speak the lobby's binary TCP
protocol. Both implement the same wire format defined in
[`../../shared/lobby-protocol/protocol.md`](../../shared/lobby-protocol/protocol.md).

- [`cpp/`](./cpp/) — standalone C++14 static library, no SDL dep,
  POSIX sockets (Windows port is straightforward via the same socket
  shape but isn't wired up yet).
- [`ts/`](./ts/) — Bun + TypeScript, uses `Bun.connect()` for TCP.

The reference C++ game client (`clients/silencer/src/lobby.{h,cpp}`)
**does not** consume this SDK yet — it has its own copy of the
protocol. Migrating the game to use this SDK is intentionally out of
scope for the SDK's initial PR; do it later in a focused change.

## Authority

[`services/lobby/protocol.go`](../../services/lobby/protocol.go) is
the source of truth for the wire format. If a bytes-on-the-wire
question comes up, read that file first; if these SDKs disagree with
the server, the server wins.

## Cross-language consistency

Both SDKs load the same golden vectors from
[`../../shared/lobby-protocol/vectors.json`](../../shared/lobby-protocol/vectors.json)
and assert that:

1. `decode(hex)` produces the expected struct, and
2. `encode(struct)` reproduces `hex` (unless the vector is in a
   skip-encode set — see test source).

If the wire format ever changes, update both impls, the
`protocol.md` spec, **and** `vectors.json`. Both test suites should
be re-run; the C++ test (`ctest --test-dir cpp/build`) and the TS
test (`bun test` from `ts/`) consume the same vectors.

## When NOT to touch this directory

- Adding/changing an opcode without bumping the lobby version
  handshake (see `services/lobby/CLAUDE.md` invariants).
- Changing the C++ game client's protocol (`clients/silencer/`)
  without making the matching change to the Go server. The SDK is a
  third location — keep all three in sync or all three out of sync.
