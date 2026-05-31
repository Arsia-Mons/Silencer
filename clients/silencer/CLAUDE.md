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

## Client UI dogma

`ClientUi` is the only production owner of visible UI composition and
screen/modal navigation. `Game::RenderClientUiFrame` collects the
`UiInputState`, begins one `ClientUi` frame, asks active screens, modals, HUD,
and overlays for retained roots, ends the frame once, renders retained
commands, then dispatches typed UI actions.
Navigation mechanics live in `src/client/ui/navigation/ScreenStack`; `Game`
may request transitions but must not store or traverse the stack itself.

Rules:

- Screens and modals implement `Screen::BuildElement`; they return a retained
  cppx component root. They must not depend on legacy immediate-mode UI APIs.
- HUD and overlays live under `src/client/ui/hud` and follow the same rule:
  build UI into the current `ClientUi` frame. Do not add UI draw methods to
  `Renderer`.
- `Renderer` owns world/pixel drawing primitives only. It must not own retained
  layout or UI screen/HUD composition.
- Retained per-frame arenas reset once in `ClientUi::BeginFrame`. Per-frame
  resets must not be called inside a screen, modal, HUD block, or overlay block.
- Modal overlays clear interaction metadata before their own `BuildElement`, so
  the top modal owns keyboard/CLI focus while lower visual layers can still
  render.
- Keep `ScreenStack` as the real single-stack owner for screens and modal
  overlays. Add a separate modal stack only if real modal semantics are being
  extracted, not as a placeholder.
- Keep UI architecture React-style: screen lifecycle code, retained components,
  providers, and hooks. Do not add a separate view/action mediation layer; data
  needed by a component should be read through a focused hook/provider when that
  is practical.

### UI primitive API contract

These rules port shadcn's surfaced best practices onto cppx: a default style
plus named `variant`s that encode the design system's identity, `size` for
scale/fit, composition over per-call configuration. Treat shadcn as the north
star for *why* an API shape is good — matching shadcn's exact API is
explicitly a non-goal.

- Primitives expose a **variant + size** API. `variant` names the visual
  treatment (the design system's identity); `size` names scale/fit behavior.
  Callers pass `{variant, size}` — never a palette index, sprite bank, or a
  `B196x33`-style sprite code. Sprite-bank terms are private implementation
  detail and must not appear in any public signature, enum, or doc comment.
- Responsive/auto sizing is a `size` value backed by the retained renderer's
  nine-slice path, never a new per-consumer preset.
- Public primitive names are plain nouns: `Button`, `Text`, `TextInput`,
  `Checkbox`, `Toggle`, `Panel`. Runtime/service types that name a subsystem
  keep their `Ui` prefix (`UiInteractionRegistry`, `UiInputState`,
  `UiInputRouter`).
- One primitive owns one visual/interaction concern. Checkbox/toggle state
  belongs to `Checkbox`/`Toggle`, not a `Button` mode — don't overload a
  primitive with a second widget's behavior.
- Every declared element needs an explicit, stable UI ID. A visible label
  must never double as its ID. Dynamic label text must be copied into retained
  frame storage or owned by the screen/provider state for the frame.
- When an all-params primitive grows callsites that all pass the same values,
  add a named variant — not another required param. The rule is "no hidden
  lobby coupling," not "no defaults ever."

### Input contract

There is exactly one path from SDL events to UI screens:

1. **Collection (single site):** `src/game/events.cpp` is the only place that
   consumes raw SDL events (`SDL_EVENT_KEY_DOWN`/`_UP`, `SDL_EVENT_TEXT_INPUT`,
   mouse, wheel, gamepad). It pushes everything into `clientUiInput`
   (`ClientUiInput`). No other file under `src/` calls `SDL_PollEvent` or
   reads SDL key events.
2. **Composition (single site):** `Game::RenderClientUiFrame` builds the
   per-frame `UiInputState` (`preparedUiInput`) from `clientUiInput`.
   `Game::ResetUiFrameDeltas` clears the queues at end of frame.
3. **Dispatch (single site):** `ClientUi::DispatchInput` hands `UiInputState`
   to `UiInputRouter::Route`, which translates pointer/text/nav/binding into
   typed `UiAction`s queued on `UiInteractionRegistry`. Screens consume those
   actions in `Screen::HandleUiIntent` (per the existing virtual). Screens
   must NOT implement `OnTextInput` or `OnKey` virtuals — those don't exist.

`UiInputState` carries three input channels (`src/ui/runtime/UiInputState.h`):

- `textInput` — typed ASCII characters from `SDL_EVENT_TEXT_INPUT`. Routed to
  the focused text widget via `DispatchTextInput`.
- `navActions` — semantic `UiNavAction` enums (Up/Down/Left/Right/Confirm/
  Cancel/Backspace/FocusNext/FocusPrevious/NextSection/PreviousSection).
  This is the channel screens read for keyboard/gamepad navigation.
- `bindingInputs` — raw scancode / gamepad-button / gamepad-axis edges, used
  only by the keybind capture flow (`controls_rebind_capture.cpp`). The
  router emits a `CaptureBinding` action per edge; the registry no-ops it
  unless a screen is in capture mode.

There is intentionally no fourth "raw key event" channel — every keypress is
either text, nav, or a binding edge.

### Gameplay shortcut keys

A few function keys (F1 scoreboard, F2 team-color toggle, F4 music pause,
F5 reshuffle music, F9 debug overlay, quit) are gameplay state changes, not
UI navigation. They are read directly in `events.cpp` `OnScancodeDown`/
`OnScancodeUp` and mutate `World` via its public setters
(`SetShowingPlayerList`, `SetShowingTeamColors`, etc.). They intentionally
bypass the `UiInputState` → `UiAction` path: routing them through a typed-
action layer would add an unnecessary UI-to-world indirection. New keys that
toggle world/gameplay state should follow the same pattern; new keys that
affect a UI screen should go through `navActions`.

Run `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` after UI ownership
changes; it guards this boundary.

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
  `tick/tick_<state>.cpp`. `GameUiPipeline::RenderClientUiFrame` starts the
  retained UI frame; `ClientUi` owns visible UI navigation.
- Simulation loop, socket, peer list, replay: `src/world.cpp`.
- Rendering: `src/render/renderer.cpp`, `src/render/surface.cpp`,
  `src/render/sprite.cpp`, `src/render/palette.cpp`. Renderer is not a UI
  owner; it supplies world/pixel drawing primitives used by retained UI renderers.
- Audio (skipped in `-s`): `src/audio.cpp`.
- Generic retained runtime/components: `src/ui/runtime`, `src/ui/components`,
  `src/ui/style`.
- Silencer-specific UI surfaces: `src/client/ui/screens`,
  `src/client/ui/modals`, `src/client/ui/hud`, and
  `src/client/ui/navigation`.
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
