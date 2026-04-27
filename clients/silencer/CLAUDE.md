# clients/silencer — C++ client

Multiplayer 2D action game (SDL2/C++14). The same binary runs as the
local client and, when launched with `-s`, as a headless dedicated
server spawned by the Go lobby in `services/lobby/`.

Build with the local `CMakeLists.txt` (`cmake -B build && cmake --build build`)
— see top-level `README.md` for platform notes and the
`-DSILENCER_LOBBY_*` knobs in *Gotchas* below. Source layout under
`src/` is flat: ~158 files, `.cpp` paired with `.h`, no subdirectories.

## Object hierarchy

`Object` → `Hittable` → `Physical` → `Bipedal` → {`Player`,
`Civilian`, `Guard`, `Robot`}. Type registry / factory:
`objecttypes.cpp`. Live objects replicate over the wire via
`serializer.cpp` (bit-aligned little-endian). Adding a replicated
field means updating `Serialize()` and bumping the version so old
clients don't desync.

## Networking model

Peer-to-peer over UDP from `world.cpp`. One peer is AUTHORITY — it
runs simulation; others send inputs and apply deltas. Dedicated mode
(`-s`) = permanent AUTHORITY with no SDL video/audio. Snapshot ring
in `oldsnapshots` / `totalsnapshots`. TCP lobby client is
`lobby.cpp` + `lobbygame.cpp`; wire format is mirrored in
`services/lobby/protocol.go` — changes must land on both sides.

## Dedicated-server contract

The same binary runs the client and, when launched with `-s`, a
headless dedicated server:

```
silencer -s <lobbyaddr> <lobbyport> <gameid> <accountid>
```

- Parsed in `src/main.cpp` → `src/game.cpp`.
- Spawned by the Go lobby in `services/lobby/proc.go` on each `MSG_NEWGAME`.
- Skips `SDL_Init(VIDEO)` and audio; RSS ~12 MB.
- Heartbeats UDP to the lobby: `[0x00][gameid u32][port u16][state u8]`.
  No heartbeat in 30 s → lobby aborts the create.

## Where to look

- Top-level state machine (menus, lobby, in-game): `src/game.cpp`.
- Simulation loop, socket, peer list, replay: `src/world.cpp`.
- Rendering: `src/renderer.cpp`, `src/surface.cpp`, `src/sprite.cpp`, `src/palette.cpp`.
- Audio (skipped in `-s`): `src/audio.cpp`.
- UI widgets: `src/interface.cpp`, `src/button.cpp`, `src/textbox.cpp`,
  `src/selectbox.cpp`, `src/scrollbar.cpp`, `src/toggle.cpp`, `src/overlay.cpp`,
  `src/minimap.cpp`.
- Projectiles: `src/*projectile.cpp` + `src/shrapnel.cpp`.
- Stations: `src/healmachine.cpp`, `src/creditmachine.cpp`,
  `src/inventorystation.cpp`, `src/techstation.cpp`, `src/walldefense.cpp`,
  `src/fixedcannon.cpp`, `src/terminal.cpp`.

## Build artifacts

- Linux: binary `silencer` (lowercase, GNU convention).
- macOS: `Silencer.app` bundle (`MACOSX_BUNDLE`); runtime asset path
  inside the bundle is `Contents/assets/` (loaded via `src/main.cpp`
  `CDResDir`). The Xcode project was retired — CMake `MACOSX_BUNDLE`
  is the only macOS build path.
- Windows: `Silencer.exe`. Runtime expects `assets\` next to the
  exe (`src/os.cpp` `GetResDir`). Resources / icon are wired through
  `resources.rc` (auto-included on Windows builds).

## CLI agent control

The game binary exposes a JSON-lines TCP control socket for headless
automation by coding agents (UI verification, screenshot testing, menu
navigation without a human).

**Start the daemon**

Use the E2E harness helpers — they handle binary detection across platforms:

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
wait_alive "$PORT"
# ... do work ...
stop_silencer "$PID" "$PORT"
```

The binary paths per platform:
- macOS: `build/Silencer.app/Contents/MacOS/Silencer`
- Linux: `build/silencer`
- Windows: `build/Silencer.exe`

**CLI wrapper**

```bash
bun clients/silencer-cli/index.ts --port $PORT <op> [args...]
```

See `clients/silencer-cli/` for the full wrapper and
`.claude/skills/using-silencer-cli/SKILL.md` for the complete op
reference and usage patterns.

**Relevant flags**

| Flag | Purpose |
|------|---------|
| `--headless` | Skip SDL video/audio init (required in CI) |
| `--control-port <n>` | Open JSON-lines TCP control socket on port *n* |

## Gotchas

- **Lobby host is a compile-time constant.** Baked in via
  `-DSILENCER_LOBBY_HOST=<host> -DSILENCER_LOBBY_PORT=<port>`. Default is
  `127.0.0.1:517`. CI sets it to `lobby.arsiamons.com`. Rebuild
  the client to point at a different lobby.
- **Version string must match the lobby.** Set via
  `-DSILENCER_VERSION=...` (default in `CMakeLists.txt`); the lobby's
  `-version` flag defaults to the same. Bump both together.
  `CPACK_PACKAGE_VERSION` is installer metadata only — unrelated to
  the wire handshake.
- **macOS data dir.** Client `chdir`s to
  `~/Library/Application Support/Silencer` at startup
  (`src/main.cpp` `CDDataDir`) — copy `../../shared/assets/` contents
  there or run from the repo with the binary in place.
- **Shared assets live two levels up** at `../../shared/assets/`
  relative to `clients/silencer/`. CMake install rules and the macOS
  bundle resource bake use that path.
- **Android/Ouya code paths exist** in `src/main.cpp` but are not
  actively maintained; don't rely on them. JNI symbols use the
  `com.silencer.game.Silencer` package convention.
