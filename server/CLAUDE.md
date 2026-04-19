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
  not create game" dialog (`src/game.cpp:798`). Don't reuse.
- **Don't hold `h.mu` across network I/O.** Copy state under lock,
  send outside.
