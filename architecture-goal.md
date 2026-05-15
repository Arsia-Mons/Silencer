# Silencer Clay UI Architecture Goal

## Goal

Completely replace the legacy UI implementation in `clients/silencer/src/ui`
with a modern Clay-based UI architecture.

This is a UI architecture refactor, not a whole-game rewrite. Do not try to
move every gameplay, networking, renderer, server, or world subsystem into a
new folder layout as part of this work. The required result is that the old UI
dogma is gone and the new UI is built around Clay, reusable primitives, flex
layout, native window-space input, and a strong separation between generic UI
toolkit code and Silencer-specific client UI.

The implementation should ideally recreate the original Silencer look using
the existing palette, sprite banks, sprite fonts, panel art, and screen design
docs, but it must do so through Clay and modern layout primitives instead of
preserving the legacy `Interface` object system.

## Source Material

Use these as inputs:

- `/Users/hv/repos/sdl3-clay/architecture.md` for the desired responsibility
  boundaries and mental model.
- `/Users/hv/repos/sdl3-clay/docs/architecture.md` and the reference
  implementation under `/Users/hv/repos/sdl3-clay/src` for practical Clay frame
  lifecycle and action-queue patterns.
- `/Users/hv/repos/sdl3-clay/src/rendering/SdlClayRenderer.*` and
  `/Users/hv/repos/sdl3-clay/third_party/clay/clay_renderer_SDL3.*` as local
  examples of SDL3 Clay command rendering.
- The official Clay repository's SDL3 renderer at
  `https://github.com/nicbarker/clay/tree/main/renderers/SDL3`.
- `docs/design/` in this repo for visual parity with existing Silencer screens,
  palette behavior, sprite fonts, sprite banks, and widget appearance.
- `clients/silencer/CLAUDE.md` for current client build/run/test rules.
- `clients/cli/` and `tests/cli-agent/` for automation and screenshot capture.

Treat the local SDL3 Clay example and the official SDL3 Clay renderer as
reference material and implementation guidance, not as dogma. Do not copy their
SDL_Renderer/SDL_ttf rendering model directly unless it genuinely fits
Silencer. Silencer already has an SDL3 GPU backend, an indexed framebuffer, a
palette pipeline, and sprite-font assets. The Clay adapter should fit
Silencer's presentation pipeline and assets.

## Current Legacy Problems To Eliminate

The current UI is not just visually old; it is architecturally wrong for the
future:

- Menu screens build world objects such as `Interface`, `Button`, `TextBox`,
  `TextInput`, `SelectBox`, `ScrollBar`, `Toggle`, and `Overlay`.
- `Game` routes UI input through `currentinterface` and legacy object IDs.
- `Player::Tick` creates in-game chat, buy, and tech interfaces as hidden world
  objects.
- `Renderer::DrawHUD` draws substantial UI directly, including buy/tech menus
  and chat.
- Mouse coordinates for UI are scaled into a fixed `640x480` logical space.
- Automation commands inspect and click legacy object widgets instead of stable
  UI element metadata.
- `clients/silencer/src/ui` mixes generic widgets, screens, modals, panels,
  and misplaced world/game concepts.

The new architecture should delete this model. Do not keep compatibility shims
unless explicitly requested.

Some files currently under `src/ui` are not actually UI toolkit concepts:

- `Overlay` is used for world doodads, lights, projectile hit effects, map
  markers, and old UI images/text. Split the old responsibilities instead of
  preserving `Overlay` as a catch-all.
- `TeamBillboard` is a world/map object, not a UI primitive.
- `Stats` is game/lobby data, not a UI component.

Move or rename those concepts as needed when bulldozing `src/ui`.

## Target Architecture

### Generic Clay Toolkit

`clients/silencer/src/ui` should become a generic Clay-based toolkit/runtime.
It should not know about Silencer-specific concepts such as lobbies, agencies,
teams, buyable items, maps, tech choices, credits, players, or weapons.

Target shape:

```text
clients/silencer/src/ui/
  runtime/
    ClayService.h/.cpp
    UiFrameContext.h
    UiInputState.h
    CallbackStore.h/.cpp
    TextStorage.h/.cpp
    UiActionQueue.h
    UiInteractionRegistry.h/.cpp
    UiIds.h

  design/
    Theme.h
    Colors.h
    Typography.h
    Spacing.h
    SpriteTokens.h

  layout/
    Box.h/.cpp
    Row.h
    Column.h
    ScrollArea.h/.cpp
    Spacer.h
    Divider.h

  primitives/
    Text.h/.cpp
    SpriteImage.h/.cpp
    Button.h/.cpp
    Field.h/.cpp
    Panel.h/.cpp
    Tabs.h/.cpp
    ListItem.h/.cpp
    Slider.h/.cpp
    ProgressBar.h/.cpp
```

Clay's raw `CLAY({ ... }) { ... }` blocks remain the underlying layout system,
but screen code should read as composed UI primitives, not as large raw Clay
config blocks.

### Silencer-Specific Client UI

Silencer-specific UI should live outside the generic toolkit, under a client UI
layer. Suggested shape:

```text
clients/silencer/src/client/ui/
  ClientUi.h/.cpp
  ClientUiState.h
  ClientUiActions.h

  navigation/
    ScreenId.h
    ScreenStack.h/.cpp
    ModalStack.h/.cpp

  common/
    SilencerChrome.h/.cpp
    MapPreview.h/.cpp
    AgencyBadge.h/.cpp
    PlayerName.h/.cpp
    ErrorBanner.h/.cpp

  screens/
    main_menu/
    lobby_connect/
    lobby/
    options/
    update/
    mission_summary/

  modals/
    message/
    password/

  hud/
    Hud.h/.cpp
    StatusBars.h/.cpp
    MinimapPanel.h/.cpp
    PlayerList.h/.cpp

  overlays/
    chat/
    buy_menu/
    tech_menu/
```

Promote components only to the nearest useful level. If only the lobby game
create screen uses a component, keep it under that screen. If multiple
Silencer UI surfaces use it, move it to `client/ui/common`. Only generic,
game-agnostic pieces belong under `src/ui/primitives`.

## Frame And Input Model

Clay should run once per UI frame and declare the complete visible UI tree.

Production dogma after the ownership cleanup:

- `Game::RenderClientUiFrame` is the only production entry point that begins,
  ends, drains, and renders a Clay UI frame.
- `ClientUi` / `ClayService` own the Clay lifecycle and primitive frame-store
  resets. Screens, modals, HUD, and overlays only declare UI into the active
  frame.
- `Screen::BuildUi` is a declaration hook, not a render hook. It must not call
  `Clay_BeginLayout`, `Clay_EndLayout`, `Clay_SetPointerState`,
  `clay_bridge::EnsureInitialized`, or `clay_bridge::Render`.
- In-game HUD and overlays are client UI surfaces, not renderer methods.
  `Renderer` must not expose `Draw*Clay` APIs or own Clay layout.
- The Clay compositor is the only bridge that consumes render commands and
  calls renderer/palette/surface primitives.

Required lifecycle:

1. Platform/event code collects raw SDL input, text input, mouse position,
   pointer down state, wheel deltas, window size, and gamepad/keyboard UI
   actions.
2. `ClientUi` converts that into `UiInputState` in native UI/window
   coordinates.
3. `ClayService::beginFrame` calls:
   - `Clay_SetCurrentContext`
   - `Clay_SetLayoutDimensions`
   - `Clay_SetPointerState`
   - `Clay_UpdateScrollContainers`
   - `Clay_BeginLayout`
4. Active screens, modals, HUD, and overlays declare the UI tree.
5. `Clay_EndLayout` returns render commands.
6. UI callbacks and keyboard/gamepad focus emit typed actions into a queue.
7. Controllers drain actions after layout and mutate app/UI state or call
   narrow game/lobby/config adapters.
8. A dedicated Clay UI renderer/compositor consumes Clay commands after the
   world frame has been drawn.

Do not mutate screen stacks, world state, or domain state directly from inside
Clay callbacks. Queue typed actions and drain them after layout.

All interactive UI must be fully navigable with mouse, keyboard, and gamepad.
Mouse support includes pointer hover, click, drag where relevant, and wheel or
trackpad scrolling. Keyboard and gamepad support includes focus traversal,
confirm/cancel, directional navigation, tab/section changes, text entry where
applicable, and consistent modal focus blocking.

## Resolution And Coordinate Policy

The UI must not be locked to `640x480`.

For this migration, the game world renderer may continue using its current
internal indexed framebuffer and camera model. The UI, however, must use native
window-space layout dimensions and pointer coordinates.

That means:

- No new UI code should calculate layout around hard-coded `640` or `480`.
- No new UI input should scale mouse coordinates into `640x480`.
- Clay dimensions, pointer coordinates, hit testing, scissor rectangles, and UI
  rendering must share one native UI coordinate space.
- Layout should use flex sizing, grow/fixed/percent constraints, and responsive
  shell composition.
- The original `640x480` look remains an important parity target, but it is a
  target viewport, not the architecture.

## Rendering Strategy

Do not fold Clay into the existing monolithic `Renderer` class. Today that
class owns too many responsibilities already: camera following, world drawing,
sprite blits, palette effects, text drawing, HUD drawing, minimap/status/player
list drawing, debug overlays, and PNG capture. The `RenderDevice`/SDL3 GPU
backend separately owns GPU presentation, palette remap, upscale, lighting, and
particles.

The correct scope is a separate Clay UI renderer/compositor boundary that plugs
into Silencer's presentation pipeline and resource model. The local
`sdl3-clay` renderer and the official Clay SDL3 renderer can guide command
dispatch, clipping, text measurement, and render-command coverage, but the
Silencer adapter should be designed for Silencer's indexed/palette/GPU
pipeline. It should reuse Silencer assets, palette data, and GPU presentation
where appropriate, but it should not become another responsibility inside
`Renderer::Draw` or `Renderer::DrawHUD`.

Required UI renderer/compositor capabilities:

- Render Clay rectangles, borders, text, sprite/images, custom payloads, and
  scissor start/end commands.
- Measure text with the same behavior used for actual text drawing.
- Support Silencer sprite fonts from font banks `132..136`.
- Support palette colors and old-look visual tokens from the existing design
  docs.
- Support sprite-backed UI images and panel chrome from existing sprite banks.
- Support custom payloads for special Silencer surfaces such as minimap/map
  previews if needed.

The likely composition model:

1. World renders to the existing indexed framebuffer.
2. GPU backend remaps/upscales that framebuffer as it does today.
3. A dedicated Clay UI renderer/compositor renders over the final scene in
   native window coordinates.
4. Screenshot capture captures the composed result, not only the old indexed
   framebuffer.

If implementation chooses an intermediate UI overlay texture instead, the
contract is the same: the final frame and automation screenshots must include
the Clay UI.

## Visual Style Goal

Recreate the original Silencer look where practical:

- Use `docs/design/palette.md`, `docs/design/sprite-banks.md`,
  `docs/design/font.md`, and per-screen docs as the fidelity source.
- Preserve palette-driven color identity and sprite-font character.
- Preserve recognizable menu/lobby chrome, button feel, screen hierarchy, and
  original screen mood.
- Rebuild layout with Clay flex primitives, responsive containers, and stable
  dimensions instead of absolute object placement.

Do not turn Silencer into a generic modern SaaS UI. Modernize the
implementation, not the game's identity.

## Screen Migration Scope

At minimum, replace every current surface under `clients/silencer/src/ui`.
Also include the in-game UI surfaces currently created or drawn outside that
directory because they are part of the legacy UI architecture:

- Main menu
- Lobby connect/login
- Lobby chrome
- Lobby character panel
- Lobby chat panel
- Lobby game select panel
- Lobby game create panel
- Lobby game join panel
- Lobby game tech panel
- Options root
- Options controls
- Options display
- Options audio
- Update screen
- Mission summary
- Message modal
- Password modal
- In-game HUD UI where it is truly UI
- In-game chat overlay/input
- In-game buy menu
- In-game tech menu
- Player list/scoreboard-style overlay if currently UI-driven

Do not migrate unrelated gameplay/world rendering as part of this goal unless
it is necessary to remove legacy UI coupling.

## Automation And Control Socket

The Silencer CLI/control socket must continue to support agent-driven UI tests,
but it should stop depending on legacy `Interface` object inspection.

Replace legacy widget automation with a Clay-aware registry:

- Every interactive or inspectable element gets a stable ID.
- UI primitives register metadata such as kind, label, enabled state, value,
  focused state, selected state, and bounding box.
- `inspect` returns the current UI tree metadata.
- `click --label`, `set_text --uid`, `select`, `key`, and related operations
  route through this registry and typed UI actions.
- Automation must work for mouse, keyboard, gamepad-equivalent navigation, text
  fields, scroll areas, modals, lobby panels, and in-game overlays.

Keep CLI ergonomics similar to today so existing tests can be updated rather
than replaced wholesale.

## Visual Parity Verification

Visual parity is a core acceptance criterion. Build verification around real
screenshots, not subjective inspection alone.

Use the Silencer CLI wherever possible:

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
wait_alive "$PORT"
bun clients/cli/index.ts --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
bun clients/cli/index.ts --port "$PORT" screenshot --out /tmp/silencer-mainmenu.png
stop_silencer "$PID" "$PORT"
```

Recommended screenshot diff workflow:

1. Capture baseline screenshots from the current legacy UI before replacing a
   surface.
2. Capture Clay screenshots from the same state after migration.
3. Use an image diff tool to compare:
   - exact dimensions
   - perceptual similarity
   - changed pixel count
   - max/mean color delta
4. Store or document representative before/after diff outputs for the migrated
   surfaces.
5. Treat `640x480` as the strictest old-look parity viewport.
6. Also test `1280x720` and one narrow/tall viewport to prove the Clay UI is
   responsive and not hard-locked to legacy dimensions.

Suggested parity targets:

- Main menu: high visual fidelity at `640x480`; same background/logo/button
  identity, version placement, hover feel, and palette.
- Lobby: preserve chrome, header, panel identity, chat/list/form hierarchy, and
  readable sprite-font style.
- Options/update/modals: preserve recognizable Silencer chrome while allowing
  more robust responsive layout.
- In-game HUD/chat/buy/tech: preserve gameplay readability and original UI
  identity, but fix layout coupling and input handling.

Acceptable differences:

- Responsive layout changes at non-`640x480` viewports.
- Minor pixel differences caused by Clay command ordering, text measurement
  normalization, or deliberate primitive cleanup.
- Deliberate improvements documented in the migration notes.

Unacceptable differences:

- Missing visual assets.
- UI drawn only into the old `640x480` framebuffer.
- Incorrect palette or font identity.
- Text clipping/overlap that did not exist before.
- Broken hit testing or pointer coordinates after resize.
- Screenshots that omit the Clay UI layer.

## Functional Verification

At minimum, verification should cover:

- `cmake -B build -S clients/silencer`
- `cmake --build clients/silencer/build`
- Existing CLI smoke tests, updated for Clay automation:
  - `tests/cli-agent/e2e/00_ping.sh`
  - `tests/cli-agent/e2e/10_navigate.sh`
  - `tests/cli-agent/e2e/20_screenshot.sh`
- New/updated E2E flows:
  - main menu navigation by mouse, keyboard, and gamepad
  - options tabs and settings changes
  - text input in lobby connect and password modal
  - scrollable list behavior via wheel/trackpad, keyboard, and gamepad
  - modal focus blocking
  - lobby panel swapping
  - in-game chat text entry
  - buy/tech menu selection
  - screenshot capture includes composed UI
  - resize from `640x480` to `1280x720` with correct layout and hit testing

For Clay integration specifically verify:

- Pointer coordinates, layout dimensions, hit testing, and render output use the
  same coordinate space.
- Pressed, held, and released callbacks behave correctly across frames.
- Wheel/trackpad scrolling has no one-frame lag.
- Text measurement matches actual sprite-font rendering.
- Window resize updates layout dimensions before layout.
- Scissor commands clip all command types they should clip.
- IDs are unique and stable in repeated rows, modals, scroll areas, and
  automation.
- Custom payload memory remains valid until render command consumption
  completes.
- Application state changes happen after layout declaration, not inside it.

## Migration Rules

- Delete old UI code as surfaces are ported; do not build a parallel permanent
  UI stack.
- Do not preserve `Interface`/widget object behavior as a compatibility layer.
- Keep generic `src/ui` free of game, lobby, world, player, or renderer policy.
- Keep Silencer-specific UI in `client/ui`.
- Keep Clay-specific renderer code behind one renderer boundary.
- Do not let screen files call SDL or GPU APIs directly.
- Use typed UI actions and surface-local controllers for UI intent.
- Do not route hover, selected row, temporary field focus, or transient widget
  state through global game/network command layers.
- Do not mutate authoritative multiplayer state from client UI.
- No overengineering: build the primitives required by the migrated surfaces,
  not a speculative widget framework.

## Completion Definition

This goal is complete when:

- `clients/silencer/src/ui` has been completely replaced by the new generic
  Clay toolkit.
- Silencer-specific UI lives in the client UI layer.
- The legacy UI object model is no longer used for menus, modals, lobby UI,
  HUD UI, chat, buy menu, or tech menu.
- No new UI is hard-coded to a `640x480` coordinate system.
- CLI automation can inspect, click, type, select, scroll, and screenshot Clay
  UI.
- Screenshot diffs demonstrate acceptable visual parity for core old UI
  surfaces at `640x480`.
- Responsive screenshots prove the UI works at larger and differently shaped
  windows.
- The normal client build and relevant E2E tests pass.
