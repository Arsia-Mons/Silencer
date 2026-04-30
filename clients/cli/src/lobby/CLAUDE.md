# clients/cli/src/lobby — fake lobby players for multiplayer dev

Lets you fill a lobby with authenticated players from the CLI instead
of standing up real game clients. User-facing guide:
[`shared/skills/cli/SKILL.md`](../../../../shared/skills/cli/SKILL.md);
this file is for editing the code.

## Shape

A small background process (`silencer-lobbyd`, entry: `daemon.ts`)
holds N `LobbyClient` instances from `@silencer/lobby-sdk` keyed by
the `--as <name>` you give each session. Each CLI invocation is a
thin client: open the unix socket, send one JSON-lines request, exit.
Auto-spawned on first `lobby spawn` (`spawn.ts`); auto-exits when
sessions = 0 AND connections = 0.

Why one process for many sessions: per-player overhead is just a
`LobbyClient` + its TCP socket. Bun startup is amortized once, which
makes 5–10 fake players cheap to keep alive across an afternoon.

## Files

- `protocol.ts` — JSON-lines request/reply frames.
- `paths.ts` — socket + log location (see `SILENCER_LOBBYD_DIR`).
- `session-manager.ts` — in-memory session map + `LobbyLike` factory injection.
- `rpc-server.ts` / `rpc-client.ts` — `Bun.listen` / `Bun.connect` over the unix socket; one-shot + streaming.
- `spawn.ts` — fork + poll the socket to ensure the daemon is up.
- `daemon.ts` — entry point. Idempotent shutdown.
- `commands.ts` — handlers wired into `LOCAL_OPS.lobby` in the parent `index.ts`.

## Invariants worth not breaking

- **Credentials never touch disk.** `--user` / `--pass` are passed
  once to `spawn`, held in memory; subsequent commands use `--as`.
- **`paths.ts` caps the socket path at 100 chars.** macOS' `sun_path`
  is 104 bytes; we leave a 3-byte cushion. Fail loud, don't truncate.
- **One process, N sessions.** Don't introduce per-session subprocesses
  — that defeats the whole point.
- **`tail` final frame is guarded by `tailEnded`.** Subscribes to
  `chat`/`presence`/`channel`/`newGame`/`delGame`/`stateChanged`; fires
  one final frame on disconnect. Don't add an exit path that skips it.

## When NOT to touch this dir

- **New lobby opcodes** belong upstream: `services/lobby/protocol.go`
  → `clients/lobby-sdk/ts` → `shared/lobby-protocol/vectors.json`.
  This module *consumes* the SDK and shouldn't grow protocol knowledge.
- **Non-lobby CLI features** belong in the parent `clients/cli/`, not
  here.

## Known limitations

- `lobby game create` builds a `LobbyGame` with `mapHash: <20 zero
  bytes>` and `port: 0` — fine for local test traffic, may be
  rejected by a production lobby. To make a CLI-created game
  joinable, extend `commands.ts::game` with `--map-hash` /
  `--listen-port`.
