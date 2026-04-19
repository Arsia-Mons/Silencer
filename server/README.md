# zSILENCER lobby server

A self-hosted replacement for the defunct `lobby.zsilencer.com` service.
Speaks the existing client wire protocol (reverse-engineered from
`src/lobby.cpp` and `src/lobbygame.cpp`) and spawns `zsilencer -s`
subprocesses on demand to host individual games.

## Build

```
cd server
go build
```

No external dependencies — stdlib only. Produces `./zsilencer-lobby`.

## Run

```
sudo ./zsilencer-lobby -addr :517 -game-binary /path/to/build/zsilencer
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
| `-game-binary` | `../build/zsilencer` | path to the `zsilencer` binary to spawn per game |
| `-public-addr` | `127.0.0.1` | host or IP clients should use to reach the dedicated servers |

## How it works

- **TCP** on `-addr` serves the lobby protocol: authentication, chat,
  game-list browsing, create/join, user-info queries, stats.
- **UDP** on the same port receives heartbeats (`[0x00][gameid u32][port u16][state u8]`)
  from spawned dedicated servers.
- On each `MSG_NEWGAME`, the lobby spawns `zsilencer -s <public-addr> <port> <gameid> <accountid>`.
  That subprocess binds a UDP port for game traffic, heartbeats the
  lobby, and takes over as the game's AUTHORITY.
- The creator's client connects directly to that UDP port (published in
  the `LobbyGame.hostname` field returned to all clients).
- If no heartbeat arrives within 30 s, the create request fails.
- Accounts are auto-created on first login (password is SHA-1 hashed).

## Storage

Users and per-agency stats live in a flat JSON file (`lobby.json` by
default), written atomically on each change. For low-traffic servers
this is fine; swap in SQLite or Postgres later if you need it.

## Deployment notes

- The dedicated-server subprocess runs **headless** (no SDL_Init(VIDEO),
  no audio, no window). RSS is ~12 MB per active game; startup is
  sub-second.
- For cloud hosting set `-public-addr` to the VM's reachable IP so
  clients connect to the right host for UDP game traffic.
- Process cleanup: when a creator disconnects, the lobby kills their
  spawned dedicated server and removes the game from the list.
