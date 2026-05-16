# clients/silencer — C++ client

Multiplayer 2D action game (SDL3/C++14). The same binary runs as the
local client and, when launched with `-s`, as a headless dedicated
server spawned by the Go lobby in `services/lobby/`.

## Building

Build/configure **only** through the wrapper — never raw
`cmake`/`cl`/`ninja`, never CLion's bundled MinGW (MSVC-only
codebase; MinGW cannot link it):

- Windows: `clients/silencer/build.ps1 [preset] [-Clean]`
- macOS/Linux: `clients/silencer/build.sh [preset] [--clean]`

Default preset `win-ninja` → Debug → `build/`. `-Clean` is the cache-
poisoning recovery (don't hand-roll `rm`/reconfigure). Presets,
compile-time lobby/version knobs, vcpkg deps, per-platform artifacts,
and what the wrapper enforces:
[`../../docs/silencer-client-build.md`](../../docs/silencer-client-build.md).

## Object hierarchy

`Object` is mixin multiple inheritance of five bases — `Sprite`,
`Physical`, `Hittable`, `Bipedal`, `Projectile` (`object.h:14`).
Leaf classes inherit directly from `Object` (actors, projectiles,
stations, UI widgets) in a two-deep tree, no further subclassing.
Full breakdown in
[`../../docs/silencer-client-architecture.md`](../../docs/silencer-client-architecture.md).

Type registry / factory: `objecttypes.cpp`. Live objects replicate
over the wire via `serializer.cpp` (bit-aligned little-endian).
Adding a replicated field means updating `Serialize()` and bumping
the version so old clients don't desync.

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

## Data-driven NPCs (actordefs + behavior trees)

NPC animation/hurtbox/sound (`actordef.{h,cpp}`,
`shared/assets/actordefs/`) and AI (`behaviortree.{h,cpp}`,
`shared/assets/behaviortrees/`) are JSON-driven, hot-fetched from the
admin API on each map load and edited in the admin BT editor.
`guard.cpp`, `robot.cpp`, `civilian.cpp` are currently wired. Type/
field reference, the two sound-helper variants, and NPC-wiring steps:
[`../../docs/silencer-client-architecture.md`](../../docs/silencer-client-architecture.md).

## Where to look

- `src/actordef.h` / `src/actordef.cpp` — actor definition system (see above).
- `src/behaviortree.h` / `src/behaviortree.cpp` — BT interpreter (see above).
- Top-level state machine (menus, lobby, in-game): `src/game/`.
  `game.cpp` is the dispatcher; `events.cpp` handles SDL input,
  `ingame.cpp` holds in-game lifecycle, `headless.cpp` glues the
  control queue, and each gameplay-state Tick body lives in
  `tick/tick_<state>.cpp`. Menu screens live in `src/ui/screens/`.
- Simulation loop, socket, peer list, replay: `src/world.cpp`.
- Rendering: `src/renderer.cpp`, `src/surface.cpp`, `src/sprite.cpp`, `src/palette.cpp`.
- Audio (skipped in `-s`): `src/audio.cpp`.
- UI widgets: `src/ui/components/` (`interface`, `button`, `textbox`,
  `textinput`, `selectbox`, `scrollbar`, `toggle`, `overlay`, …).
- UI screens / modals / panels: `src/ui/screens/`, `src/ui/modals/`,
  `src/ui/screens/<name>/panels/`. **Before touching any UI code,
  read [`../../shared/skills/silencer-ui/SKILL.md`](../../shared/skills/silencer-ui/SKILL.md)**
  (loaded via `.claude/skills/editing-silencer-ui`) — the widget
  system has no defaults, no event delivery, and several non-obvious
  conventions that bite new code.
- Projectiles: `src/*projectile.cpp` + `src/shrapnel.cpp`.
- Stations: `src/healmachine.cpp`, `src/creditmachine.cpp`,
  `src/inventorystation.cpp`, `src/techstation.cpp`, `src/walldefense.cpp`,
  `src/fixedcannon.cpp`, `src/terminal.cpp`.

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
bun clients/cli/index.ts --port $PORT <op> [args...]
```

See `clients/cli/` for the full wrapper and
[`../../shared/skills/cli/SKILL.md`](../../shared/skills/cli/SKILL.md)
(loaded by Claude Code via the `.claude/skills/using-silencer-cli` →
`../../shared/skills/cli` symlink) for the complete op reference and
usage patterns.

**Relevant flags**

| Flag | Purpose |
|------|---------|
| `--headless` | Skip SDL video/audio init (required in CI) |
| `--control-port <n>` | Open JSON-lines TCP control socket on port *n* |
| `--tui` | Stream paletted framebuffer over TCP to the `silencer-tui` host (`SILENCER_TUI_FRAME_HOST/_PORT`); skips SDL video, keeps audio. See `clients/tui/CLAUDE.md`. |

## Gotchas

- **Build-time config is baked at configure.** Lobby host/port and
  the wire-handshake version string are compile-time knobs (default
  local lobby; version must match the lobby or the handshake fails) —
  see [`../../docs/silencer-client-build.md`](../../docs/silencer-client-build.md).
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
- **`adminapiurl` config key** (`src/config.h`) — URL the client fetches
  actordefs and behavior trees from on each map load. Defaults to
  `http://localhost:24000/api` for local dev; set it to the production
  admin API URL when deploying. Does not affect lobby traffic.
