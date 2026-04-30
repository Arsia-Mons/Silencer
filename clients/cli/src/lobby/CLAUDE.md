# clients/cli/src/lobby — agent-driven fake lobby presences

Single supervisor daemon (`silencer-lobbyd`) holds N `@silencer/lobby-sdk`
`LobbyClient` instances keyed by session name. CLI subcommands are thin
clients: open the unix socket, send one JSON-lines request, exit.

## Files

- `paths.ts` — co-located dir resolution
  (`SILENCER_LOBBYD_DIR` overrides; defaults: `$XDG_RUNTIME_DIR/silencer/`
  on Linux, `$TMPDIR/silencer/` on macOS, `%LOCALAPPDATA%\Silencer\lobbyd\`
  on Windows). Both `lobbyd.sock` and `lobbyd.log` live here. Caps the
  resolved socket path at `MAX_SOCKET_PATH` (100) and throws a clear error
  on macOS if that limit is exceeded (sun_path is 104 bytes; 100 leaves a
  3-byte cushion for the trailing NUL).
- `protocol.ts` — `Request` / `Reply` JSON-lines frames.
- `session-manager.ts` — in-memory session map; takes a `LobbyLike`
  factory so tests can inject fakes. Defines `NoSessionError` for the
  daemon's `NO_SESSION` error code.
- `rpc-server.ts` — `Bun.listen({ unix })`; dispatches to the manager.
  Streaming `tail` op emits a `{event: "registered"}` ack frame after
  listener registration so clients can sync without polling.
- `rpc-client.ts` — `Bun.connect({ unix })`; one-shot (`rpcCall`) and
  streaming (`rpcStream`).
- `spawn.ts` — auto-spawn detached `silencer-lobbyd`, poll the socket.
- `daemon.ts` — entry point. Idempotent `shutdown` guard. Auto-exits
  when sessions=0 AND conns=0.
- `commands.ts` — handlers wired into `index.ts`'s `LOCAL_OPS.lobby`.
  Streaming handlers return `result: null` so `main()` skips the
  trailing JSON-summary line.

## Lifecycle

1. First `lobby spawn` → `ensureDaemon` probes the socket. Refused →
   forks detached `bun src/lobby/daemon.ts` and polls until it accepts.
2. Daemon serves until `kill_all` or until `kill` removes the last
   session AND the last connection drops (`onIdle` callback).
3. Stale socket file (crashed daemon) is unlinked on the next bind.

## Invariants

- Creds (`--user`, `--pass`) are passed once to `spawn`; subsequent
  calls use only `--as <name>`. Passwords never touch disk.
- macOS sun_path is 104 bytes; `paths.ts` caps the resolved socket
  path at `MAX_SOCKET_PATH` (100) and throws a clear error otherwise.
- One process, N sessions: per-player overhead is just a `LobbyClient`
  instance + its TCP socket. The Bun runtime cost is amortized once.
- The `tail` op subscribes a connection to `chat`/`presence`/
  `channel`/`newGame`/`delGame`/`stateChanged` events. Final frame
  fires on session disconnect or socket close (idempotent via
  `tailEnded` flag).

## Known limitations

- `lobby game create` constructs a `LobbyGame` with `mapHash: <20
zero bytes>` and `port: 0`. This is fine for emitting test traffic
  to a local lobby server, but a production lobby may reject games
  without a real map hash and listening port. If you need a joinable
  game from the CLI, extend `commands.ts::game` with `--map-hash`
  and `--listen-port` flags (or compute them from `--map`).

## When NOT to touch this dir

- If you're changing the wire protocol (adding lobby opcodes), edit
  `services/lobby/protocol.go` first, then `clients/lobby-sdk/ts`,
  then `shared/lobby-protocol/vectors.json`. This module consumes
  the SDK and shouldn't grow protocol knowledge of its own.
- If you're adding non-lobby CLI features, they don't belong here.
  The `lobby` namespace is the only thing in this dir.
