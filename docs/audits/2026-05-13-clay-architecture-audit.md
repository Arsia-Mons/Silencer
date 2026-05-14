# Clay UI Architecture Audit

Date: 2026-05-13
Branch audited: `hv/clay-ui-migration`
Branch state covered: Clay migration commits from `857f184 Add Clay UI migration`
through `1d76e2b Remove demo ClientUi state path`, plus the current working-tree
navigation ownership extraction.

## Scope

There is no root `architecture.md` in this checkout. I audited against the root
`architecture-goal.md`, which points at `/Users/hv/repos/sdl3-clay/architecture.md`
and `/Users/hv/repos/sdl3-clay/docs/architecture.md` as the responsibility-boundary
source material.

The audit covered the code and tests added by the Clay migration, with emphasis
on plan faithfulness, shortcuts, folder hierarchy, and file/class naming from a
game/web engineering perspective.

Verification run during this audit/update:

- `cmake --build build --target silencer_tests -j 8` passed.
- `build/tests/silencer_tests` passed.
- `ctest --test-dir build --output-on-failure` passed.
- `cmake --build build --target silencer -j 8` passed with existing `sprintf`
  and inline-definition warnings.
- `tests/cli-agent/e2e/10_navigate.sh` passed.
- `tests/cli-agent/e2e/11_keyboard_navigation.sh` passed.
- `tests/cli-agent/e2e/12_controls_scroll.sh` passed.
- `tests/cli-agent/e2e/13_password_modal.sh` passed.
- `tests/cli-agent/e2e/14_directional_navigation.sh` passed.
- `tests/cli-agent/e2e/50_resize_screenshot.sh` passed.
- `tests/cli-agent/e2e/51_ingame_ui_overlays.sh` passed.
- `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` passed.

## Overall Assessment

The implementation is directionally faithful to the big migration goal: the
legacy `Interface` object model has largely been removed from the visible UI,
Silencer-specific screens moved under `clients/silencer/src/client/ui`, generic
Clay primitives/runtime live under `clients/silencer/src/ui`, and the work added
real screenshot/parity and CLI-driven test coverage.

After the follow-up integration passes, production UI composition has a single
owner: `Game::RenderClientUiFrame` begins one `ClientUi` / `ClayService` frame,
screens/modals/HUD/overlays declare into it, and the Clay compositor renders one
command stream after the world frame. `ClientUi` now owns real screen/modal
navigation through `client/ui/navigation/ScreenStack`; `Game` only exposes
narrow transition wrappers for the state machine, screens, and control socket.
Pointer/wheel/gamepad UI input is collected into `UiInputState`, typed
interaction actions are drained after layout, and `clay_inspector` has been
removed in favor of `UiAutomationRegistry`.

No placeholder `ClientUiState` or `ModalStack` was added. Modal behavior still
uses the real existing single-stack overlay semantics via `Screen::IsOverlay()`.

The remaining architectural gaps are narrower: several screens are still large
raw Clay layouts, the generic toolkit boundary remains debatable, input dispatch
still has compatibility-shaped text/key paths, and the HUD still has some
privileged friend access into `World`/`Player` while those domain read models are
being separated.

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
- Production frame setup: `clients/silencer/src/game/game.cpp:485`.
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
- Central render pass: `clients/silencer/src/game/game.cpp:485`.
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
  `clients/silencer/CMakeLists.txt:189` and `clients/silencer/CMakeLists.txt:195`.
- Control socket now names the domain screen:
  `clients/silencer/src/net/controldispatch.cpp:801` and
  `clients/silencer/src/net/controldispatch.cpp:810`.

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

### Medium: Client UI Navigation Ownership

Status: resolved by the navigation ownership extraction.

The architecture goal's client UI shape lists `ClientUiState`,
`ClientUiActions`, `navigation/ScreenStack`, and `navigation/ModalStack` under
`clients/silencer/src/client/ui`. That is the correct desired direction: client
UI should own UI-session state and screen/modal navigation. The final pass moved
the real push/pop/replace/top, pending clear, visible traversal, overlay ticking,
and modal automation-scoping semantics into
`clients/silencer/src/client/ui/navigation/ScreenStack`. `ClientUi` owns that
stack directly.

`Game` still has public `PushScreen` / `PopScreen` / `ReplaceScreen` /
`GetTopScreen` methods, but they are now narrow request/access wrappers for
state transitions and control-socket handlers. They no longer store the stack or
implement its mechanics. The deleted `ClientUiState` demo path remains gone.

A separate `ModalStack` was not added because there is not yet a separate real
modal state model to extract. Current modals are real `Screen` overlays in the
single stack, so splitting them now would create the placeholder architecture the
audit warned against.

Evidence:

- Desired client UI shape: `architecture-goal.md:130` through
  `architecture-goal.md:145`.
- Current owner: `clients/silencer/src/client/ui/ClientUi.h:28` through
  `clients/silencer/src/client/ui/ClientUi.h:41`.
- Extracted stack mechanics:
  `clients/silencer/src/client/ui/navigation/ScreenStack.cpp:13` through
  `clients/silencer/src/client/ui/navigation/ScreenStack.cpp:75`.
- Game transition wrappers:
  `clients/silencer/src/game/game.cpp:1224` through
  `clients/silencer/src/game/game.cpp:1237`.
- Game no longer stores the stack:
  `clients/silencer/src/game/game.h:176` through
  `clients/silencer/src/game/game.h:216`.
- Deleted stub shape:
  `clients/silencer/src/client/ui/ClientUiState.h` before deletion.

### Medium: The Input Contract Is Only Partially Implemented

Status: mostly resolved for the central Clay screen path; text/key dispatch
still uses compatibility screen hooks while the input layer is being normalized.

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
- Input collection: `clients/silencer/src/game/game.cpp:410`.
- Wheel events: `clients/silencer/src/game/events.cpp:356`.
- Gamepad nav collection: `clients/silencer/src/game/game.cpp:1081`.
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
- [x] Real screen/modal navigation ownership moved from `Game` into
  `ClientUi` / `navigation/ScreenStack`; `Game` has transition wrappers only.
- [x] Per-frame primitive and HUD payload arenas reset through a single
  `UiFrameContext::BeginFrame()` owned by `ClientUi`. No primitive `BeginFrame`
  is called from inside a screen, modal, HUD, or overlay block.
- [x] `friend class silencer::client_ui::InGameUiController;` removed from
  `world.h`. HUD/overlays consume `HudView` populated once per frame in
  `Game::RenderClientUiFrame`. HUD layer no longer includes `world.h`,
  `player.h`, `team.h`, `lobby.h`, or other gameplay headers.
- [x] Large raw Clay screen files decomposed at natural domain seams:
  - `InGameHud.cpp` 1050 → 69 lines, split into 7 focused sub-builders.
  - `InGameOverlays.cpp` 392 → 252 (chat + buy/tech + player-list extracted).
  - `lobby_screen.cpp` 708 → 133 (chrome / main area / controller split).
  - `game_tech_panel.cpp` 515 → 257 (tree grid + selected panel).
  - `game_select_panel.cpp` 487 → 263 (layout extracted).
  - `game_create_panel.cpp` 543 → 265 (options + map form extracted).
  - `chat_panel.cpp` 345 → 162 (layout extracted).
  - `options_controls_screen.cpp` 618 → 236 (keybind list + rebind capture).
  Every file in `client/ui/hud/`, `screens/lobby/`, `screens/options/` is now
  ≤ 300 lines.
- [x] Input contract documented and enforced. Single SDL→UI collection point
  (`game/events.cpp`), single dispatch site (`ClientUi::DispatchInput`).
  `Screen` has no `OnTextInput` / `OnKey` virtuals. `UiInputState` carries
  three channels (`textInput`, `navActions`, `bindingInputs`); a fourth raw
  `keyEvents` channel is intentionally not added because no consumer needs
  it. `60_ui_architecture_boundaries.sh` now forbids `SDL_EVENT_KEY_*` /
  `SDL_PollEvent` under `client/ui/` and `ui/runtime/`.
- [x] The user explicitly accepted Silencer-specific knowledge living in
  `src/ui/primitives` (bank text, bank buttons, sprite-backed toggles).
  This is now a deliberate design choice documented in
  `clients/silencer/CLAUDE.md`, not an oversight. The toolkit boundary is
  honest in its current form.

References for the completion work:

- Design: `docs/superpowers/specs/2026-05-14-clay-ui-architecture-completion-design.md`.
- Plan: `docs/superpowers/plans/2026-05-14-clay-ui-architecture-completion.md`.
- Independent final audit (verdict: Faithful) executed against `hv/clay-ui-migration`
  after all moves landed.

Bottom line: the Clay UI architecture refactor is closed. The next correct
work item is unrelated to this audit — likely Move 5's deferred
capture-binding gating (low priority) or a broader audit of `World`'s
remaining `friend` declarations and 2800-line gameplay files
(out-of-scope for this UI refactor).
