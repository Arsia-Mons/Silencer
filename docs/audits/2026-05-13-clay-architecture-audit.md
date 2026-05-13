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

- `cmake --build build --target silencer_tests -j 8` passed.
- `build/tests/silencer_tests` passed.
- `ctest --test-dir build --output-on-failure` passed.
- `cmake --build clients/silencer/build --target silencer -j 8` passed
  with existing `sprintf` deprecation warnings.
- `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` passed.
- `ctest --test-dir clients/silencer/build --output-on-failure -R ui_architecture`
  found no tests because that build tree has no registered test entries.

## Overall Assessment

The implementation is directionally faithful to the big migration goal: the
legacy `Interface` object model has largely been removed from the visible UI,
Silencer-specific screens moved under `clients/silencer/src/client/ui`, generic
Clay primitives/runtime live under `clients/silencer/src/ui`, and the work added
real screenshot/parity and CLI-driven test coverage.

After the follow-up integration passes, production UI composition has a single
owner: `Game::RenderClientUiFrame` begins one `ClientUi` / `ClayService` frame,
screens/modals/HUD/overlays declare into it, and the Clay compositor renders one
command stream after the world frame. Pointer/wheel/gamepad UI input is collected
into `UiInputState`, typed interaction actions are drained after layout, and
`clay_inspector` has been removed in favor of `UiAutomationRegistry`.

That statement is about frame ownership, not final navigation ownership. The
desired end state is still a coherent client UI layer that owns screen/modal
navigation. `Game::screenStack` is the current real navigation owner, but it
should be treated as extraction source material, not the permanent architecture.
Do not reintroduce the deleted `ClientUiState` stub as a temporary bridge.

The remaining architectural gaps are narrower: several screens are still large
raw Clay layouts, the generic toolkit boundary remains debatable, and the HUD
still has some privileged friend access into `World`/`Player` while those domain
read models are being separated.

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

   The work added `tools/pixdiff`, `tests/lobby-ui`, and CLI E2E coverage for
   keyboard navigation, scrolling, password modal behavior, directional navigation,
   resize screenshots, in-game overlays, and architecture boundaries.

## Findings

### High: Production UI Bypasses The Planned Central UI Runtime

Status: resolved by the follow-up integration pass.

The plan requires platform/event code to collect input, `ClientUi` to convert it
into `UiInputState`, `ClayService::beginFrame` to run the Clay lifecycle,
callbacks to emit typed actions, and controllers to drain those actions after
layout.

The production path now owns that lifecycle through `Game::RenderClientUiFrame`:
`Game` prepares a per-frame `UiInputState`, enters `ClientUi::BeginFrame`,
asks visible screens/modals/HUD/overlays to run `BuildUi`/builder hooks,
then ends and renders a single Clay command stream. Screens and modals no longer
call `Clay_BeginLayout`, `Clay_EndLayout`, or `clay_bridge::Render` directly.

`EnsureInitialized` no longer resets pointer state, wheel deltas are collected
from `SDL_EVENT_MOUSE_WHEEL`, and gamepad menu navigation is stored on the
central `UiInputState` before being dispatched for the active UI frame.
Interactions now queue typed `UiAction`s in `UiAutomationRegistry`; the current
screen callbacks remain the compatibility controller path while the larger raw
screen layouts are still being decomposed.

Evidence:

- Plan: `architecture-goal.md:186` through `architecture-goal.md:208`.
- Production frame setup: `clients/silencer/src/game/game.cpp:498`.
- Screen declaration hook: `clients/silencer/src/client/ui/screens/screen.h:24`.
- Clay backend: `clients/silencer/src/client/ui/ClayBridgeFrameBackend.cpp:11`.
- Runtime lifecycle: `clients/silencer/src/ui/runtime/ClayService.cpp:8`.
- Typed action queue: `clients/silencer/src/ui/runtime/UiAutomationRegistry.cpp:152`.

### High: Clay HUD Rendering Still Lives Inside `Renderer`

Status: resolved by the central `ClientUi` ownership pass.

The plan explicitly says not to fold Clay into the existing monolithic
`Renderer`, and not to make it another responsibility inside `Renderer::Draw` or
`Renderer::DrawHUD`. `Renderer` no longer exposes `DrawHud*Clay` APIs, no longer
declares HUD Clay layout, and no longer calls in-game UI from inside
`Renderer::Draw`.

HUD and overlay UI now live under `clients/silencer/src/client/ui/hud` and
declare into the same `ClientUi` frame as screens and modals. System-camera
insets and minimap pixels still use renderer primitives for world/pixel drawing,
but their surrounding UI composition belongs to the client UI layer.

Evidence:

- Plan: `architecture-goal.md:235` through `architecture-goal.md:251`.
- Central render pass: `clients/silencer/src/game/game.cpp:498`.
- HUD builder: `clients/silencer/src/client/ui/hud/InGameHud.cpp:819`.
- Overlay builder: `clients/silencer/src/client/ui/hud/InGameOverlays.cpp:386`.
- Renderer boundary guard:
  `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh:65`.

### High: Domain-Level Names Encode The Framework

Status: resolved after the lobby/domain renaming pass.

The original naming smell was the lobby. The plan's target hierarchy named the
domain concern as `screens/lobby/`, while the migration snapshot used
`screens/lobby_clay/`, `LobbyClayScreen`, `clay_game_create_panel.cpp`,
`clay_game_select_panel.cpp`, `SILENCER_HAVE_LOBBY_CLAY`, and CLI operations
that mentioned `LobbyClayScreen`.

Those application-layer names have since been moved back to domain names such
as `screens/lobby/`, `LobbyScreen`, `game_create_panel.cpp`,
`game_select_panel.cpp`, and `SILENCER_HAVE_LOBBY_UI`.

Evidence:

- Plan target: `architecture-goal.md:153` through `architecture-goal.md:157`.
- Current folder: `clients/silencer/src/client/ui/screens/lobby`.
- Current class: `clients/silencer/src/client/ui/screens/lobby/lobby_screen.h:22`.
- CMake now uses the domain-oriented symbol:
  `clients/silencer/CMakeLists.txt:98` and `clients/silencer/CMakeLists.txt:114`.
- Control socket now names the domain screen:
  `clients/silencer/src/net/controldispatch.cpp:792`.

Result: Clay is no longer a permanent product concept in the lobby application
layer. Keep future framework names in low-level adapter names only where the file
truly bridges Clay render commands.

### Medium: Automation Uses A Parallel Global Inspector Instead Of The Planned Metadata Registry

Status: resolved by the follow-up integration pass.

The plan asks for a Clay-aware automation registry where primitives register
stable metadata and control socket operations route through that registry and
typed UI actions.

The separate `clay_inspector` runtime was removed. Production screen widgets now
register through `UiAutomationRegistry`; the `silencer::ui::automation`
namespace is a thin compatibility surface over the active registry. The registry
now exposes widget metadata, focus/text/input dispatch, click dispatch, and a
typed `UiAction` queue that is drained after the Clay layout frame.

Evidence:

- Plan: `architecture-goal.md:325` through `architecture-goal.md:342`.
- Registry implementation: `clients/silencer/src/ui/runtime/UiAutomationRegistry.cpp:58`.
- Compatibility namespace: `clients/silencer/src/ui/runtime/UiAutomationRegistry.cpp:366`.
- Control socket lookup: `clients/silencer/src/net/controldispatch.cpp:607`.
- Boundary guard: `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh:29`.

### Medium: Client UI Navigation Ownership Is Still In `Game`

Status: open. See the single current handoff section at the end of this doc.

The architecture goal's client UI shape lists `ClientUiState`,
`ClientUiActions`, `navigation/ScreenStack`, and `navigation/ModalStack` under
`clients/silencer/src/client/ui`. That is the correct desired direction: client
UI should own UI-session state and screen/modal navigation. However, the deleted
`clients/silencer/src/client/ui/ClientUiState.h` was not a correct unfinished
version of that architecture. It only held `ScreenId activeScreen` and
`focusedElementId`, and `ClientUi::BuildFrame` only used it to build a fake
two-button main menu path. Restoring or wiring that file would reintroduce a
parallel enum router beside the real screen stack.

Evidence:

- Desired client UI shape: `architecture-goal.md:130` through
  `architecture-goal.md:145`.
- Current real owner: `clients/silencer/src/game/game.h:82` through
  `clients/silencer/src/game/game.h:87`.
- Current push/pop/replace mechanics:
  `clients/silencer/src/game/game.cpp:1237` through
  `clients/silencer/src/game/game.cpp:1255`.
- Current visible UI traversal:
  `clients/silencer/src/game/game.cpp:477` through
  `clients/silencer/src/game/game.cpp:486`.
- Deleted stub shape:
  `clients/silencer/src/client/ui/ClientUiState.h` before deletion.

### Medium: The Input Contract Is Only Partially Implemented

Status: resolved for the central Clay screen path; renderer-owned HUD Clay
fragments remain outside this path until the HUD renderer cleanup happens.

The plan requires full mouse, keyboard, and gamepad navigation, including wheel
or trackpad scrolling and modal focus behavior. The code covers some keyboard
and CLI-equivalent navigation, but actual input normalization is incomplete.

Mouse position/down state, pointer edges, wheel deltas, frame dimensions, frame
time, and gamepad UI nav actions now flow into `UiInputState`.
`ClayService::BeginFrame` feeds pointer and wheel state into Clay before layout.
Gamepad directional navigation and confirm are collected by `TickGamepadMenuNav`
and dispatched through the prepared UI input for the active screen frame.

Evidence:

- Plan: `architecture-goal.md:210` through `architecture-goal.md:214`.
- Required verification: `architecture-goal.md:414` through
  `architecture-goal.md:431`.
- Input collection: `clients/silencer/src/game/game.cpp:397`.
- Wheel events: `clients/silencer/src/game/events.cpp:356`.
- Gamepad nav collection: `clients/silencer/src/game/game.cpp:1040`.
- Clay input feed: `clients/silencer/src/ui/runtime/ClayService.cpp:8`.
- UI nav action type exists: `clients/silencer/src/ui/runtime/UiInputState.h:9`.
- Focus/key dispatch: `clients/silencer/src/ui/runtime/UiAutomationRegistry.cpp:214`.

### Medium: Screen Files Are Still Large Raw Clay Layouts

The plan allows raw Clay as the underlying layout system but says screen code
should read as composed primitives, not as large raw Clay config blocks.

The current code has many screen-local raw `CLAY({ ... })` trees and per-file
helper state. The largest examples are:

- `clients/silencer/src/client/ui/screens/lobby/lobby_screen.cpp`: 700 lines.
- `clients/silencer/src/client/ui/screens/lobby/game_tech_panel.cpp`: 505 lines.
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
See the single current handoff section for the active cleanup order.

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
payload policy makes the boundary less honest. See the single current handoff
section for the active cleanup order.

### Low: Root-Level Process Artifacts Leaked Into The Migration Commit

Status: resolved by `9d1e427 Remove Clay agent process artifacts`.

The migration commit added `prompt.md` and the `ralph/` directory. Those agent
process artifacts have since been removed from the branch.

Evidence:

- Commit: `9d1e427 Remove Clay agent process artifacts`.

## Current Handoff Instructions

This is the only handoff section. Update it in place as the most recent state;
do not append historical handoffs after each attempt.

Backlog audit:

- [x] Domain-level Clay names are out of `client/ui`: lobby folder, classes,
  namespaces, CMake symbols, tests, and control-socket text use product/domain
  names.
- [x] Renderer no longer owns Clay HUD layout or exposes `Draw*Clay` HUD APIs.
- [x] `ClientUi` / `ClayService` are the production frame runtime; screens,
  modals, HUD, and overlays declare into one frame.
- [x] `clay_inspector` is gone; automation uses `UiAutomationRegistry`.
- [x] Wheel/trackpad and gamepad UI input flow into `UiInputState`.
- [x] The fake `ClientUiState` / `BuildFrame` demo path is gone. Keep it gone.
- [ ] `Game::screenStack` still owns real screen/modal navigation. This is the
  next architectural backlog item.
- [ ] Large raw Clay screen files still need decomposition into domain
  components and reusable design primitives.
- [ ] The generic toolkit boundary is still not fully honest because several
  `src/ui/primitives` APIs embed Silencer sprite/font/palette payload policy.

Next implementation checklist:

1. Extract `Game::screenStack` into a real
   `clients/silencer/src/client/ui/navigation/ScreenStack`.
2. Move the existing push/pop/replace/top/overlay traversal semantics, not a
   simplified `ScreenId activeScreen` enum.
3. Let `ClientUi` own that navigation/session layer directly, or own a real
   `ClientUiState` aggregate that contains it. `ClientUiState` is acceptable
   only if it owns real UI-session state.
4. Add `ModalStack` only if extracting real modal semantics from the current
   stack. Do not add a placeholder stack.
5. Keep `Game` as gameplay/lobby/render-loop orchestrator. It may request UI
   transitions, but it should not remain the final owner of screen/modal stack
   mechanics.
6. Do not leave two routing systems alive. Update all callers in the same pass;
   no compatibility shim where both `Game::screenStack` and a client UI stack can
   route screens.
7. After navigation ownership is extracted, decompose large raw Clay files at the
   nearest useful domain boundary: lobby chrome/title bar/panel shell, option
   rows, HUD status bars, chat overlay, buy/tech menu rows.
8. Resolve the toolkit boundary by either naming Silencer-specific primitives as
   design-system primitives or hiding payload details behind renderer-agnostic
   token APIs.
9. Verify with `cmake --build clients/silencer/build --target silencer
   silencer_tests -j 8`, `clients/silencer/build/tests/silencer_tests`, and the
   focused CLI E2E scripts for navigation, scrolling, modals, resize screenshots,
   in-game overlays, and architecture boundaries.

Bottom line: the central Clay runtime is now the production frame owner. The
next correct step is not another temporary state shim; it is extracting real
screen/modal navigation ownership from `Game` into the client UI layer.
