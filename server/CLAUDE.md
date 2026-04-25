# server/ — Go lobby

Stdlib-only Go. Deployment docs (flags, how-it-works narrative,
storage notes) are in `README.md`; this file is for editing the code.

## Per-file

- `main.go` — flag parsing, wires `store → proc → hub`, TCP accept
  loop + UDP goroutine.
- `hub.go` — authoritative in-memory state (games, pending spawns,
  connected clients). **All mutations behind `h.mu`.** Fans out
  `NewGame` / `DelGame` to subscribers.
- `client.go` — one goroutine per TCP conn; dispatches opcodes to
  hub methods.
- `protocol.go` — wire format: `[len u8][payload]`, max 255 bytes.
  Reader/writer mirrors the client's `Serializer` (bit-aligned, but
  lobby fields happen to be byte-aligned).
- `proc.go` — spawns `zsilencer -s <publicAddr> <port> <gameID>
  <accountID>` per `MSG_NEWGAME`; tracks PIDs; `StopAll()` on shutdown.
- `udp.go` — decodes heartbeat `[0x00][gameid u32][port u16][state u8]`
  → `hub.OnHeartbeat`.
- `store.go` — `lobby.json` atomic writes (temp + rename), SHA-1
  password hashes, per-agency stats keyed by user.

## Invariants

- **Opcodes must match `src/lobby.cpp`.** Adding one requires both
  sides and a client version bump.
- **Heartbeat timeout: 30 s** (`hub.go:28`). Miss → pending create
  fails with `status=2`.
- **`status=2` on `opNewGame` is reserved** for the client's "Could
  not create game" dialog (`src/game.cpp:824`). Don't reuse.
- **Don't hold `h.mu` across network I/O.** Copy state under lock,
  send outside.

## Storage

`lobby.json` is source of truth — flat file, atomic writes
(temp + rename), SHA-1 password hashes. When `MONGO_URL` is set,
`mongosync.go` async-mirrors every mutation to MongoDB so the admin
dashboard sees up-to-date data. Password hashes are never synced.
Mongo is a mirror, not a backup — if it diverges, `lobby.json` wins.

## Gotchas

- **Port 517 needs root on macOS/Linux.** For local dev, run on
  `:15170` and rebuild the client with
  `-DZSILENCER_LOBBY_PORT=15170` (see `src/CLAUDE.md` gotchas).
