# clients/silencer — C++ client

Multiplayer 2D action game (SDL3/C++14). The same binary runs as the
local client and, when launched with `-s`, as a headless dedicated
server spawned by the Go lobby in `services/lobby/`.

## Building

Build/configure the client **only** through the wrapper — never invoke
`cmake`/`cl`/`ninja` directly, and never use CLion's bundled MinGW
(this codebase is MSVC-only; MinGW cannot link it):

- Windows: `clients/silencer/build.ps1 [win-ninja|win-ninja-release|win-ninja-unity]`
- macOS/Linux: `clients/silencer/build.sh [win-ninja|win-ninja-release|win-ninja-unity]`

Default preset is `win-ninja` (Debug → `build/`). Pass `-Clean`
(`--clean` on `.sh`) to wipe the CMake cache (keeps `vcpkg_installed`)
— the correct recovery from a poisoned cache; do not hand-roll
`rm`/reconfigure loops. The Windows wrapper pins the newest installed
Visual Studio via `vswhere` and resolves `VCPKG_ROOT` (process env →
persisted User env → hard error). The macOS/Linux wrapper uses
`VCPKG_ROOT` when it is exported and valid; otherwise it follows the
historical CMake path and lets CMake/pkg-config find system packages
(Homebrew/apt). Both wrappers take a build lock so CLion, CLI agents,
and parallel agents can't corrupt the shared CMake cache by configuring
concurrently. Idle CLion is fine; just don't run an IDE build at the
same time as a wrapper build into the same dir — CLion may keep its
`win-ninja` preset profile (same toolchain). Lobby host/version are
compile-time `-D` knobs (see *Gotchas*). Presets, vcpkg dependency
details, per-platform artifacts, and wrapper behavior are documented in
[`../../docs/silencer-client-build.md`](../../docs/silencer-client-build.md).

Source under `src/` is organized by concern (`game/`, `render/`,
`client/ui/`, `ui/`, etc.); keep new files in the owning concern
instead of reviving the old flat layout.

UI is authored in `.cppx`/`.hx` and transpiled to C++ at build time, so
**configure requires `Python3`** (`find_package(Python3 ... REQUIRED)`).
Generated `.cpp`/`.h` go to `<build>/generated/cppx/` — gitignored,
never committed, regenerated each build. See
[`../../docs/silencer-client-build.md`](../../docs/silencer-client-build.md)
(*cppx UI pipeline*).

## Client UI dogma

The live UI is the golden **retained cppx engine**: a React-style hook
runtime + Yoga flex layout + a premultiplied-RGBA draw-command IR. The
substrate lives in `src/ui` (runtime + styling); the app-shell lives in
`src/client/ui`. `client::ui::ClientUi` owns the retained UI tree across
frames; `GameUiPipeline` (`src/game/ui`) is the composition root that drives
it. The legacy UI layer (and the third-party layout library it wrapped) was
deleted — do not reintroduce its concepts.

Each frame `GameUiPipeline::RenderClientUiFrame` builds the global provider
chain and drives the retained `client::ui::UiPipeline` through the
`PipelineHost` (`src/render/cppx_ui`): begin frame → build the visible screen
set → run Yoga layout → run the focus/hit-test pass → emit the
`::ui::DrawCommandList` IR → execute it to a window-sized RGBA buffer.
`GameRenderer::Present` uploads that buffer via `RenderDevice::UploadUiFrame`,
which the GPU backend composites over the upscaled world.

Rules:

- `Game`/`game_loop` own the gameplay state machine (`GoToState`,
  `stateisnew`, `FADEOUT`) but NO LONGER mount UI screens. The always-mounted
  `AppRoot` (`src/client/ui/app_shell/app_root.h`, stack entry 0) reads
  `use_session().phase` and renders the screen that owns that phase — the
  declarative phase reconciler. The phase is a read-only projection of the
  game state machine (`src/game/ui/session_phase.h`).
- `ScreenStack` (`src/client/ui/app_shell/navigation`) is the single owner of
  the screen stack. Tier-1 navigation (options, pause, modals) pushes
  `OverlayScreen`s above `AppRoot`; overlays re-establish their own providers
  and never change `phase`. Stack mutations are queued and drained after
  render, never run mid-build.
- Screens are `.cppx` view functions that compose the capability hooks
  (`use_session`/`use_settings`/`use_key_map`/`use_navigation`/…) and return a
  `::ui::UiElement` tree. They never own the frame lifecycle, never touch SDL,
  `Renderer`, or `Surface`, and reach gameplay only through hook intent
  closures — never a raw `Game`/`World` handle.
- `Renderer` owns world/pixel (8-bit indexed) drawing only. It must not own UI
  layout or composition. The `src/render/cppx_ui` bridge is the only place SDL
  meets the UI (SDL_ttf fonts, `SDL_Texture`); the `src/ui` runtime stays
  SDL-free.

### UI styling + theming

Styling is a from-first-principles substrate (`src/ui/style`): a `Theme` of
`RoleStyle`s, sparse `StylePatch` overlays, and `resolve()` layering role +
override patches + interaction state into one dense `VisualStyle` at authoring
time. The current theme (dark slate, accent blue, control gradients) lives in
`src/client/ui/app_theme.cpp` and is installed OUTERMOST via `ThemeProvider`;
`use_tokens()`/`use_theme()` read it. **NOTE: this slate/blue palette is a visual
regression, not the target design.** The golden visual design is Silencer's
**origin/main** (the Clay UI: cool-blue `#9FC9FF`/panel `#10141C`/border `#565E6F`,
green oval sprite buttons, sprite chrome/starfield). Restoring it *while keeping the
cppx engine* is tracked by **SIL-84**
(`docs/plans/2026-06-01-cppx-design-parity-restore.md`). The golden `~/repos/ui` repo
is the authority for the cppx **engine + authoring conventions** only — NOT the look. `src/ui` keeps only a neutral fallback
(`default_theme()`). The renderer never sees the theme — components resolve
their `VisualStyle` and the IR carries only resolved, premultiplied paint.

The legacy `src/ui/design/Colors.h` + `Spacing.h` constants were removed
(SIL-17). `silencer::tokens` (`src/client/ui/components/tokens.h`) + the product
theme (`app_theme.cpp`) are the single paint source.

### Input contract

Raw SDL events are consumed in exactly one place: `src/game/session/events.cpp`
(the only file under `src/` that calls `SDL_PollEvent`). It dispatches:

- **Gameplay shortcut keys** (F1 scoreboard, F2 team-color toggle, F4 music
  pause, F5 reshuffle music, F9 debug overlay, quit) — read directly in
  `OnScancodeDown`/`OnScancodeUp` and mutate `World` via its public setters
  (`SetShowingPlayerList`, `SetShowingTeamColors`, etc.). New keys that toggle
  world/gameplay state should follow this pattern.
- **Gameplay movement / keybinds** — flow through `GameInput` (the
  scancode/gamepad → keybind-action mapping).
- **UI input** — collected into a per-frame `::ui::UiInputFrame` (`src/ui/
  input.h`): nav (`nav_up`/…), `confirm`/`cancel`/`pointer` edges, and
  key/text/editing event channels. The retained pipeline consumes it in
  `ClientUi::begin_frame`/`end_layout` (focus + hit test); interactive cppx
  screens read interaction state via the runtime hooks (`use_focused`,
  `use_hovered`, `use_pressed`). The cppx UI currently polls the pointer
  directly; full nav/text wiring lands with the interactive screens.

Text-input platform gating (`SDL_StartTextInput`/`StopTextInput`) is owned by
`GameUiPipeline`, gated on `ClientUi::wants_text_input()`. Never call SDL
text-input functions elsewhere.

Run `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` after UI ownership
changes; it guards this boundary (and bans the deleted-layer tokens under
`src/`, including in docs).

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
  `game.cpp` is the dispatcher; `session/events.cpp` handles SDL input,
  `ingame.cpp` holds in-game lifecycle, `headless.cpp` glues the
  control queue, and each gameplay-state Tick body lives in
  `tick/tick_<state>.cpp`. `GameUiPipeline` (`src/game/ui`) is the cppx UI
  composition root; the always-mounted `AppRoot` maps the session phase onto
  the owning screen.
- Simulation loop, socket, peer list, replay: `src/world.cpp`.
- Rendering: `src/render/renderer.cpp`, `src/render/surface.cpp`,
  `src/render/sprite.cpp`, `src/render/palette.cpp`. Renderer is not a UI
  owner; it supplies world/pixel (8-bit) drawing primitives only. The
  RGBA cppx UI layer is bridged separately in `src/render/cppx_ui`.
- Audio (skipped in `-s`): `src/audio.cpp`.
- cppx UI runtime + styling substrate (screen-agnostic): `src/ui/runtime`,
  `src/ui/style`.
- Silencer UI app-shell: `src/client/ui/app_shell` (ClientUi/UiPipeline/
  ScreenStack/AppRoot), `src/client/ui/providers`, `src/client/ui/hooks`,
  and `src/client/ui/app_theme.cpp` (product theme).
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
