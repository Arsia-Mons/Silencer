# Silencer lobby server

A self-hosted replacement for the defunct `lobby.zsilencer.com` service.
Speaks the existing client wire protocol (reverse-engineered from
`src/lobby.cpp` and `src/lobbygame.cpp`) and spawns `silencer -s`
subprocesses on demand to host individual games.

## Build

```
cd services/lobby
go build
```

No external dependencies — stdlib only. Produces `./silencer-lobby`.

## Run

```
sudo ./silencer-lobby -addr :517 -game-binary /path/to/build/silencer
```

Port 517 matches the original lobby and is what the client connects to
by default. On macOS that requires `sudo`; use any unprivileged port
(`-addr :15170`) and patch the two `lobby.Connect` call sites in
`src/game.cpp` to match if you don't want to run as root.

### Flags

| flag | default | meaning |
|---|---|---|
| `-addr` | `:517` | TCP + UDP listen address |
| `-db` | `lobby.json` | JSON user/stats database path |
| `-motd` | *(built-in)* | path to a plain-text MOTD file |
| `-version` | `00023` | required client version string (empty = accept any) |
| `-game-binary` | `../build/silencer` | path to the `silencer` binary to spawn per game |
| `-public-addr` | `127.0.0.1` | host or IP clients should use to reach the dedicated servers |
| `-player-auth-addr` | `:15171` | internal HTTP server address (Docker-internal, not publicly exposed) |

### Environment variables

| variable | meaning |
|---|---|
| `MONGO_URL` | MongoDB connection string for async player sync (e.g. `mongodb://mongo:27017/silencer`). Leave empty to disable MongoDB sync entirely. |

## How it works

- **TCP** on `-addr` serves the lobby protocol: authentication, chat,
  game-list browsing, create/join, user-info queries, stats.
- **UDP** on the same port receives heartbeats (`[0x00][gameid u32][port u16][state u8]`)
  from spawned dedicated servers.
- On each `MSG_NEWGAME`, the lobby spawns `silencer -s <public-addr> <port> <gameid> <accountid>`.
  That subprocess binds a UDP port for game traffic, heartbeats the
  lobby, and takes over as the game's AUTHORITY.
- The creator's client connects directly to that UDP port (published in
  the `LobbyGame.hostname` field returned to all clients).
- If no heartbeat arrives within 30 s, the create request fails.
- Accounts are auto-created on first login (password is SHA-1 hashed).
- **Ban enforcement**: `Login()` returns `(*User, bool, bool)` — the third bool
  indicates a banned account. Banned users receive `"Account suspended: <name>"`
  rather than `"Incorrect password"`. The `User` struct carries a `Banned bool` field.
- **Case-insensitive names**: `ByName` map keys are always `strings.ToLower(name)`;
  the display name (`User.Name`) preserves the original case. `NewStore()` deduplicates
  mixed-case keys on first load for backwards compatibility.
- **Internal HTTP server** on `:15171` (Docker-internal only, never exposed publicly):
  - `POST /player-auth` — validates game credentials (SHA-1 protocol, same as in-game login)
  - `POST /ban` — sets or clears the ban flag on a player account in real time
  - `POST /delete-player` — removes a player from the in-memory store and persists the change
- **MongoDB async sync** (`mongosync.go`): `MongoSync` upserts player records to the
  MongoDB `players` collection on every store mutation (register, ban, upgrade, delete).
  All upserts are fire-and-forget goroutines so they never block the lobby. Password
  hashes are **never** synced. `SyncAll()` runs on startup to mirror the full
  `lobby.json` to MongoDB. Set `MONGO_URL` to enable; leave it empty to disable.

## Storage

Users and per-agency stats live in a flat JSON file (`lobby.json` by
default), written atomically on each change. This file is the **primary
source of truth** — all reads and writes go through the in-memory store,
which is persisted here.

When `MONGO_URL` is set, `mongosync.go` maintains a **MongoDB mirror**:
every mutation (register, ban, upgrade, delete) is asynchronously upserted
to the `players` collection so the admin dashboard always has up-to-date
data. The sync is strictly additive — `lobby.json` always wins. Password
hashes are never written to MongoDB.

## Deployment notes

- The dedicated-server subprocess runs **headless** (no SDL_Init(VIDEO),
  no audio, no window). RSS is ~12 MB per active game; startup is
  sub-second.
- For cloud hosting set `-public-addr` to the VM's reachable IP so
  clients connect to the right host for UDP game traffic.
- Process cleanup: when a creator disconnects, the lobby kills their
  spawned dedicated server and removes the game from the list.
