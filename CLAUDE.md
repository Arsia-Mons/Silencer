# zSILENCER

Multiplayer 2D action game (C++/SDL2) plus a self-hosted Go lobby server
that replaces the defunct `lobby.zsilencer.com`.

## Commands

```bash
# Build the client (Linux/macOS)
cd build && cmake .. && make

# Build the lobby server (stdlib only, no deps)
cd server && go build       # → ./zsilencer-lobby

# Run a local lobby + game
sudo ./server/zsilencer-lobby -addr :517 -game-binary ./build/zsilencer
./build/zsilencer
```

Windows: open `zSILENCER.sln` in Visual Studio; SDL2 + SDL2_mixer dev
libs must be on the VS include/lib path.

Runtime deps (client): SDL2, SDL2_mixer, zlib.

## Layout

- `src/` — C++ client (~159 files, CMake, C++14). Entry: `src/main.cpp`.
- `server/` — Go lobby server. Entry: `server/main.go`.
- `data/` — runtime assets (sprites, tiles, sounds, palette).
- `res/` — app icons.
- `build/` — out-of-tree CMake build dir (contains `zsilencer` binary).
- `terraform/` — AWS infra (EC2 + EBS + cloud-init + Tailscale). See
  `terraform/CLAUDE.md`.
- `docs/production.md` — production setup guide: stand up your own
  lobby from scratch, CI wiring, day-2 ops, failure modes.
- `scripts/fastdeploy.sh` — bypass CI: rsync → build on the box → swap
  binary → restart `zsilencer-lobby`. Debug-only; prod goes through
  `.github/workflows/deploy.yml`.
- `.github/workflows/` — `deploy.yml` (tag `v*` → ARM64 lobby build →
  scp over Tailscale → symlink swap); `release.yml` (macOS + Windows
  client zips).

Protocol constants and lobby wire format are in `src/lobby.cpp`,
`src/lobbygame.cpp`, and `server/protocol.go`.

## Dedicated-server contract

The same `zsilencer` binary runs the client and, when launched with
`-s`, a headless dedicated server:

```
zsilencer -s <lobbyaddr> <lobbyport> <gameid> <accountid>
```

- Parsed in `src/main.cpp:160` → `src/game.cpp:132`.
- Spawned by the Go lobby in `server/proc.go` on each `MSG_NEWGAME`.
- Dedicated mode skips `SDL_Init(VIDEO)` and audio; RSS ~12 MB.
- Heartbeats UDP to the lobby: `[0x00][gameid u32][port u16][state u8]`.
  If no heartbeat in 30 s, the lobby aborts the create.

## Gotchas

- **Lobby host is a compile-time constant.** Baked in via
  `-DZSILENCER_LOBBY_HOST=<host> -DZSILENCER_LOBBY_PORT=<port>` (see
  `CMakeLists.txt:48`, used at `src/game.cpp:4018`/`:4032`). Default is
  `127.0.0.1:517`. CI sets it to `silencer.hventura.com`; rebuild the
  client to point at a different lobby.
- **Version string must match.** Client sets it at `src/game.cpp:31`
  (`world.SetVersion("00024")`); the lobby's `-version` flag defaults
  to the same. Bump both together, or pass `-version ""` on the server
  to accept any client. `CMakeLists.txt` `CPACK_PACKAGE_VERSION` is
  installer metadata only — unrelated to the wire handshake.
- **Port 517 needs root on macOS/Linux.** For local dev, rebuild with
  `-DZSILENCER_LOBBY_PORT=15170` and run the lobby on `:15170`.
- **Data dir on macOS.** Client `chdir`s to
  `~/Library/Application Support/zSILENCER` at startup
  (`src/main.cpp` `CDDataDir`) — copy `data/` there or run from the
  repo with the binary in place.
- **Android/Ouya code paths exist** in `src/main.cpp` but are not
  actively maintained; don't rely on them.

## Lobby storage

Users and per-agency stats live in `lobby.json` (flat file, atomic
writes, SHA-1 hashed passwords). Swap for SQLite/Postgres in
`server/store.go` if traffic grows.
