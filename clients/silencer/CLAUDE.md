# clients/silencer — C++ client

Multiplayer 2D action game (SDL3/C++14). The same binary runs as the
local client and, when launched with `-s`, as a headless dedicated
server spawned by the Go lobby in `services/lobby/`.

Build with the local `CMakeLists.txt` (`cmake -B build && cmake --build build`)
— see top-level `README.md` for platform notes and the
`-DSILENCER_LOBBY_*` knobs in *Gotchas* below. Source layout under
`src/` is flat: ~158 files, `.cpp` paired with `.h`, no subdirectories.

## Object hierarchy

`Object` is mixin multiple inheritance of five bases — `Sprite`,
`Physical`, `Hittable`, `Bipedal`, `Projectile` (`object.h:14`).
41 leaf classes inherit directly from `Object` (actors,
projectiles, stations, UI widgets); two-deep tree, no further
subclassing. Full breakdown in
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

## Actor definition system (`actordef.h` / `actordef.cpp`)

Actordefs are JSON files in `shared/assets/actordefs/<id>.json`. They
define per-NPC animation sequences, per-frame hurtboxes, and per-frame
sounds — everything that used to be hardcoded in the NPC `.cpp` files.

**Key types:**
- `FrameDef` — one sprite frame: `bank`, `index`, `duration` (ticks),
  `hurtbox` (`x1/y1/x2/y2` relative to feet), `sound` (filename), `soundVolume`
  (0 = default 128).
- `AnimSequence` — ordered list of `FrameDef`s + `loop` flag.
- `ActorDef` — keyed map of sequence name → `AnimSequence`.

**Playing sounds from actordefs** — two helpers on `AnimSequence`:
- `GetFrameSound(state_i, …)` — use when the state machine accumulates
  ticks (correct for tick-duration-based states).
- `GetFrameSoundByIndex(frameIdx, …)` — use when `res_index = state_i % N`
  (sprite frame index driven, not tick-accumulated). Guards and civilians use
  this path.

**Client reload** — `LoadActorDefs()` in `actordef.cpp` reads all `*.json`
files via `GLOB_RECURSE`. It is called on each map load (async fetch from the
admin API via `adminapiurl`). Adding or removing actordef files requires
`cmake -B build -S .` to regenerate the file list.

**Per-weapon guard actordefs** — `guard-blaster.json`, `guard-laser.json`,
`guard-rocket.json` replace the old single `guard.json`. `ActorDefName(weapon)`
in `guard.cpp` maps weapon integer (0/1/2/3) to the correct file name.

## Behavior tree system (`behaviortree.h` / `behaviortree.cpp`)

Tick-based interpreter. Trees are loaded from
`shared/assets/behaviortrees/<id>.json` and shared across all instances of
a given NPC type. Per-instance state lives in `BTContext`.

**Node types:** `Selector`, `Sequence`, `Parallel`, `RandomSelector`,
`Inverter`, `Cooldown`, `Repeat`, `Timeout`, `ForceSuccess`, `Wait`,
`Leaf` (dispatches to a named C++ lambda), `Condition` (compares a
blackboard key to a literal value).

**Blackboard** — `unordered_map<string, json>` on `BTContext`. Leaf
lambdas read/write it via `ctx.bb<T>(key, default)` / `ctx.bbSet(key, val)`.

**Wiring an NPC:**
1. Create `shared/assets/behaviortrees/<npc>.json` (edit in the admin BT editor).
2. In the NPC's `.cpp` constructor, call `bt_.Load("npc_id")` and register
   action lambdas with `bt_.Register("ActionName", [](BTContext& ctx) { … })`.
3. In the tick function, call `bt_.Tick(ctx_)` once per frame.

**Currently wired:** `guard.cpp`, `robot.cpp`, `civilian.cpp`.

## Where to look

- `src/actordef.h` / `src/actordef.cpp` — actor definition system (see above).
- `src/behaviortree.h` / `src/behaviortree.cpp` — BT interpreter (see above).
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
- **`libmodplug` removed from vcpkg.json.** `sdl3-mixer` no longer
  builds with the MOD/XM/IT music plugin — the game uses IMA ADPCM
  (`sound.bin`) and MP3 (`CLOSER2.mp3`) only. This cuts build time and
  removes an unused dependency.
- **`adminapiurl` config key** (`src/config.h`) — URL the client fetches
  actordefs and behavior trees from on each map load. Defaults to
  `http://localhost:24000/api` for local dev; set it to the production
  admin API URL when deploying. Does not affect lobby traffic.
