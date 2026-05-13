# Clay UI Architecture Audit

Date: 2026-05-13
Branch audited: `hv/clay-ui-migration`
Commits audited after the architecture goal baseline: `857f184 Add Clay UI migration`, `60d0cbf Fix mobile lobby Clay layout`

## Scope

There is no root `architecture.md` in this checkout. I audited against the root
`architecture-goal.md`, which points at `/Users/hv/repos/sdl3-clay/architecture.md`
and `/Users/hv/repos/sdl3-clay/docs/architecture.md` as the responsibility-boundary
source material.

The audit covered the code and tests added by the Clay migration, with emphasis
on plan faithfulness, shortcuts, folder hierarchy, and file/class naming from a
game/web engineering perspective.

Verification run during this audit:

- `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` passed.
- `ctest --test-dir build --output-on-failure` passed.
- `ctest --test-dir clients/silencer/build --output-on-failure -R ui_architecture`
  found no tests because that build tree has no registered test entries.

## Overall Assessment

The implementation is directionally faithful to the big migration goal: the
legacy `Interface` object model has largely been removed from the visible UI,
Silencer-specific screens moved under `clients/silencer/src/client/ui`, generic
Clay primitives/runtime live under `clients/silencer/src/ui`, and the work added
real screenshot/parity and CLI-driven test coverage.

It is not yet faithful to the architecture as a system. The migration appears to
have taken practical shortcuts to get surfaces rendering and tests passing:
production screens mostly bypass the planned central `ClayService` / `UiInputState`
/ typed action queue path, automation uses a separate global `clay_inspector`,
some Clay HUD drawing still lives inside `Renderer`, and several domain-level
folders/classes are named after Clay instead of the game concern.

## What Was Faithful

1. Legacy UI object removal mostly happened.

   `architecture-goal.md` called out `Interface`, `Button`, `TextBox`,
   `TextInput`, `SelectBox`, `ScrollBar`, `Toggle`, and UI `Overlay` object
   usage as the model to delete. The current tree no longer has
   `clients/silencer/src/ui/components`, `src/ui/screens`, `src/ui/modals`, or
   `src/ui/panels`, and the boundary test enforces that. `Overlay` and
   `TeamBillboard` moved to `clients/silencer/src/world`, and `Stats` moved to
   `clients/silencer/src/game`.

2. The high-level folder split mostly matches the plan.

   `clients/silencer/src/ui` now contains toolkit-ish concerns:
   `design`, `primitives`, and `runtime`. `clients/silencer/src/client/ui`
   contains screens, modals, and HUD code. That follows the intended split:
   generic UI toolkit in `src/ui`, game-client UI in `client/ui`.

3. The migration covered most named surfaces.

   The current `client/ui` tree contains main menu, lobby connect, lobby panels,
   options, update, mission summary, modals, and in-game HUD/overlay code.
   This aligns with the screen migration scope in `architecture-goal.md`.

4. Visual and automation verification were taken seriously.

   The work added `tools/pixdiff`, `tests/lobby-clay`, and CLI E2E coverage for
   keyboard navigation, scrolling, password modal behavior, directional navigation,
   resize screenshots, in-game overlays, and architecture boundaries.

## Findings

### High: Production UI Bypasses The Planned Central UI Runtime

The plan requires platform/event code to collect input, `ClientUi` to convert it
into `UiInputState`, `ClayService::beginFrame` to run the Clay lifecycle,
callbacks to emit typed actions, and controllers to drain those actions after
layout.

The code has pieces of that architecture, but they are not the production path.
`ClayService::BeginFrame` performs the intended ordered lifecycle, and `ClientUi`
calls it, but `ClientUi` is not referenced outside `clients/silencer/src/client/ui`
and tests. Real screens call `ScreenContext::BeginClayFrame`, which only calls
`EnsureInitialized`, polls SDL mouse state, and sets pointer state. The screens
then call `Clay_BeginLayout` and `Clay_EndLayout` directly.

Evidence:

- Plan: `architecture-goal.md:186` through `architecture-goal.md:208`.
- Intended implementation exists: `clients/silencer/src/ui/runtime/ClayService.cpp:8`.
- Production shortcut: `clients/silencer/src/client/ui/screens/screen_context.cpp:60`.
- Unused shell: `clients/silencer/src/client/ui/ClientUi.cpp:9`.
- Mouse wheel is not wired: `clients/silencer/src/game/events.cpp:356`.

Impact: the code passes some UI tests but has two UI runtimes: the planned one
and the one screens actually use. Wheel/trackpad scrolling, typed action
draining, and central input normalization are especially weak.

Recommendation: make `ClientUi`/`ClayService` the production UI frame owner, or
delete that architecture and update the plan. Do not keep both.

### High: Clay HUD Rendering Still Lives Inside `Renderer`

The plan explicitly says not to fold Clay into the existing monolithic
`Renderer`, and not to make it another responsibility inside `Renderer::Draw` or
`Renderer::DrawHUD`.

The migration moved some HUD orchestration into `client/ui/hud`, but `Renderer`
still exposes and implements several Clay-specific HUD drawing methods. Those
methods call Clay directly and render UI fragments from inside `renderer.cpp`.

Evidence:

- Plan: `architecture-goal.md:235` through `architecture-goal.md:251`.
- Renderer still owns Clay HUD APIs: `clients/silencer/src/render/renderer.h:81`.
- Renderer still declares Clay layout: `clients/silencer/src/render/renderer.cpp:2697`.
- Renderer draw path calls in-game UI from inside `Renderer::Draw`:
  `clients/silencer/src/render/renderer.cpp:262`.
- Client HUD then calls those renderer Clay methods:
  `clients/silencer/src/client/ui/hud/InGameHud.cpp:238`.

Impact: this preserves the renderer/UI coupling the plan was trying to remove.
It also creates a confusing ownership split: some HUD layout lives in
`client/ui/hud`, while lower-level HUD UI layout still lives in `Renderer`.

Recommendation: move these `DrawHud*Clay` layout functions out of `Renderer`
into a client UI HUD renderer/compositor layer. `Renderer` should provide
primitive drawing/resource operations, not own HUD UI element trees.

### High: Domain-Level Names Encode The Framework

Status: resolved after the lobby/domain renaming pass.

The most obvious naming smell is the lobby. The plan's target hierarchy names
the domain concern as `screens/lobby/`, but the implementation uses
`screens/lobby_clay/`, `LobbyClayScreen`, `clay_game_create_panel.cpp`,
`clay_game_select_panel.cpp`, `SILENCER_HAVE_LOBBY_CLAY`, and CLI operations
that mention `LobbyClayScreen`.

Evidence:

- Plan target: `architecture-goal.md:153` through `architecture-goal.md:157`.
- Current folder: `clients/silencer/src/client/ui/screens/lobby_clay`.
- Current class: `clients/silencer/src/client/ui/screens/lobby_clay/lobby_clay_screen.h:22`.
- CMake bakes the framework name into build policy:
  `clients/silencer/CMakeLists.txt:98` and `clients/silencer/CMakeLists.txt:114`.
- Control socket error names the implementation, not the domain:
  `clients/silencer/src/net/controldispatch.cpp:792`.

Impact: this makes Clay a permanent product concept in the application layer.
From a game-client architecture perspective, the screen is the lobby, not the
Clay lobby. A future UI library swap, renderer change, or second UI backend
would require renaming product-level concepts.

Recommendation: rename application-layer files/classes/namespaces to domain
names: `screens/lobby/`, `LobbyScreen`, `GameCreatePanel`, `GameSelectPanel`,
`GameTechPanel`. Keep Clay in low-level adapter names only where the file truly
bridges Clay render commands.

### Medium: Automation Uses A Parallel Global Inspector Instead Of The Planned Metadata Registry

The plan asks for a Clay-aware automation registry where primitives register
stable metadata and control socket operations route through that registry and
typed UI actions.

The implementation has `UiAutomationRegistry`, but production screens primarily
use `silencer::ui::clay_inspector`, a global per-frame registry of hand-authored
labels, rectangles, and callbacks. The inspector stores optional legacy `uid`s
and invokes callbacks directly. Several screens manually compute inspector
rectangles separately from the Clay layout. The control socket has a special
`lobby_show_panel` escape hatch because normal click routing cannot reach some
panel swaps.

Evidence:

- Plan: `architecture-goal.md:325` through `architecture-goal.md:342`.
- Planned registry exists: `clients/silencer/src/ui/runtime/UiAutomationRegistry.h:38`.
- Production registry: `clients/silencer/src/ui/runtime/clay_inspector.h:4`.
- Global storage: `clients/silencer/src/ui/runtime/clay_inspector.cpp:12`.
- Manual main menu registration: `clients/silencer/src/client/ui/screens/main_menu/main_menu_screen.cpp:96`.
- Special control-socket bypass: `clients/silencer/src/net/controldispatch.cpp:792`.

Impact: automation works, but the registry is not the single source of truth
from primitives/layout. This risks stale bounds, label ambiguity, missing stable
IDs, and direct mutation paths that bypass typed UI intents.

Recommendation: collapse `clay_inspector` into `UiAutomationRegistry` or make it
a thin adapter over that registry. Register metadata from primitives/components
where possible, and use stable IDs as the primary route, with labels as a
convenience layer.

### Medium: The Input Contract Is Only Partially Implemented

The plan requires full mouse, keyboard, and gamepad navigation, including wheel
or trackpad scrolling and modal focus behavior. The code covers some keyboard
and CLI-equivalent navigation, but actual input normalization is incomplete.

Mouse wheel events are empty. `UiInputState` contains wheel fields and
`ClayService` can feed `Clay_UpdateScrollContainers`, but that path is not used
by production screens. Gamepad state is polled for gameplay actions, and some
keyboard event handling checks keymap actions, but there is no clear edge-driven
UI navigation stream for gamepad confirm/cancel/focus movement independent of
keyboard events.

Evidence:

- Plan: `architecture-goal.md:210` through `architecture-goal.md:214`.
- Required verification: `architecture-goal.md:414` through
  `architecture-goal.md:431`.
- Wheel event stub: `clients/silencer/src/game/events.cpp:356`.
- UI nav action type exists: `clients/silencer/src/ui/runtime/UiInputState.h:9`.
- Default screen focus is global inspector based:
  `clients/silencer/src/client/ui/screens/screen.h:46`.

Impact: "gamepad-equivalent" tests can pass through CLI key names while real
controller navigation remains weaker than the contract. Wheel/trackpad scrolling
is a direct gap.

Recommendation: define a real `UiInput` stream from SDL/TUI/control input,
including pointer, wheel, text, confirm/cancel, focus next/previous, directional
navigation, and section changes. Feed it through the central UI runtime every
rendered UI frame.

### Medium: Screen Files Are Still Large Raw Clay Layouts

The plan allows raw Clay as the underlying layout system but says screen code
should read as composed primitives, not as large raw Clay config blocks.

The current code has many screen-local raw `CLAY({ ... })` trees and per-file
helper state. The largest examples are:

- `clients/silencer/src/client/ui/screens/lobby_clay/lobby_clay_screen.cpp`: 700 lines.
- `clients/silencer/src/client/ui/screens/lobby_clay/clay_game_tech_panel.cpp`: 505 lines.
- `clients/silencer/src/client/ui/screens/options/options_controls_screen.cpp`: 627 lines.
- `clients/silencer/src/client/ui/hud/InGameHud.cpp`: 408 lines.
- `clients/silencer/src/client/ui/hud/InGameOverlays.cpp`: 418 lines.

The build also documents that Clay primitives and lobby panels are "unsafe" for
unity builds because anonymous namespaces share names like `g_payloads`,
`g_customData`, `kPayloadCapacity`, and `AllocCustomData`.

Evidence:

- Plan: `architecture-goal.md:126` through `architecture-goal.md:128`.
- Unity-build smell: `clients/silencer/CMakeLists.txt:98` through
  `clients/silencer/CMakeLists.txt:115`.

Impact: this is a maintainability shortcut. It got screens migrated, but the
current files are hard to reason about and brittle under build-system changes.

Recommendation: extract domain components and composition helpers at the nearest
useful level: lobby chrome, lobby title bar, panel shell, option rows, HUD status
bars, chat overlay, buy/tech menu rows. Also replace repeated anonymous
namespace arena names with uniquely scoped storage or small reusable stores.

### Medium: Generic Toolkit Is Not As Generic As The Plan Says

The generic `src/ui` layer avoids direct `World`, `Lobby`, `Player`, and `Game`
includes, which is good. But several primitives depend directly on
`silencer::clay_bridge` payloads and Silencer sprite/font concepts such as
bank text, bank buttons, sprite-backed toggles, and palette payloads.

Evidence:

- Plan: `architecture-goal.md:81` through `architecture-goal.md:83`.
- Bridge coupling from primitives:
  `clients/silencer/src/ui/primitives/bank_text.cpp:32`,
  `clients/silencer/src/ui/primitives/bank_button.cpp:26`,
  `clients/silencer/src/ui/primitives/scroll_list.cpp:25`,
  `clients/silencer/src/ui/primitives/text_input.cpp:12`.

Impact: this may be an acceptable Silencer-specific design-system layer, but it
is not a fully generic UI toolkit. Naming it generic while embedding renderer
payload policy makes the boundary less honest.

Recommendation: either rename/scope these as Silencer design primitives
(`src/ui/design` or `src/client/ui/common`) or keep only renderer-agnostic
primitive APIs in `src/ui/primitives` and hide payload details behind generic
image/text token abstractions.

### Low: Root-Level Process Artifacts Leaked Into The Migration Commit

The migration commit added `prompt.md` and the `ralph/` directory. These appear
to be agent/process artifacts rather than runtime code, architecture docs, or
normal repo tooling.

Evidence:

- `prompt.md`
- `ralph/RALPH.md`
- `ralph/prd.json`
- `ralph/progress.txt`
- `ralph/ralph.sh`

Impact: not a runtime bug, but it adds root-level noise and makes it harder for
future engineers to distinguish official architecture from agent scaffolding.

Recommendation: move durable strategy into `docs/plans` or `docs/audits`, move
agent-only workflow files under a clearly named tooling area if they must stay,
or remove them before merging.

## Shortcut Summary

The main shortcuts were not "bad code" in isolation; they were delivery shortcuts:

- Screens were migrated directly to Clay before the central UI system became the
  real production path.
- Automation was made to work through a global inspector rather than through the
  planned first-class metadata/action registry.
- Renderer coupling was reduced but not removed.
- Framework names were left in application-layer domain concepts.
- Large raw Clay screen files were accepted, with a CMake skip around unity-build
  collisions instead of fixing the structure.

## Recommended Cleanup Order

1. [x] Rename domain-level Clay names out of `client/ui`: lobby folder, classes,
   namespaces, CMake symbols, tests, and control-socket text.
2. [ ] Move `Renderer::DrawHud*Clay` methods out of `Renderer` and into
   `client/ui/hud` or a dedicated UI presentation adapter.
3. [ ] Decide whether `ClayService`/`ClientUi` is the real runtime. If yes, route
   all screens through it and delete duplicate per-screen lifecycle plumbing.
4. [ ] Replace `clay_inspector` with `UiAutomationRegistry` or make it a compatibility
   adapter over the real registry.
5. [ ] Wire real wheel/trackpad and gamepad UI input into a central `UiInputState`.
6. [ ] Extract the largest raw Clay screen sections into domain components and
   reusable design primitives.
7. [ ] Move or remove root-level process artifacts (`prompt.md`, `ralph/`) before
   treating this branch as clean architecture.

## Bottom Line

The migration achieved a large practical UI replacement, but it should not be
considered architecturally complete. It is a solid intermediate port with
working Clay surfaces and tests, not yet the clean game-client UI architecture
described in the plan.
