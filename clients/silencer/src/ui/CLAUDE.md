# `ui/` — declarative UI library

Clay-backed declarative UI for the Silencer client. All menus,
modals, and in-game overlays render through this library; the legacy
imperative widget tree (`Object` subclasses in `world.objectlist`,
hand-positioned with sprite-anchor coordinates, polled per-frame for
`clicked` flags) was removed in P24–P26.

A handful of legacy widgets that aren't UI screens still live under
`components/` — `overlay` (in-game world labels), `teambillboard`
(team displays), `interface` (gutted shell kept only for its static
`WordWrap` helper used by `world.cpp` and `textbox.cpp`), and `stats`
(per-match player data, misplaced — used by `peer.h` / `user.h` /
`mission_summary.cpp`, due for relocation).

## Shape

- **`node.h`** — `Node` value type + `NodeKind` enum + factories.
  Leaf kinds: `Background`, `Sprite`, `Label`, `Button`, `Group`.
  Container kinds: `VStack`, `HStack`, `Center`, `Padding`,
  `Spacer`. A screen is a function returning a `Node`; the tree is
  pure data.
- **`context.h`** — `Context` is the bag of inputs a screen depends on
  (asset catalog, logical dimensions, scale, version string, mouse,
  UIState, dt). No `Game`/`World`/`Lobby` refs — screens stay
  testable in the preview harness.
- **`layout.h` / `layout.cpp`** — `Layout(root, ctx)` walks the tree
  and emits a Clay scope per container subtree, recording the Clay
  element id on each emitted node (`node.clay_id`). Nodes outside
  any container subtree are left with `clay_id.id == 0` so render +
  dispatch fall through to absolute `.at()` positioning. Also exposes
  `DispatchClicks(root, ctx)` — fires Button on_click handlers whose
  bounding box (queried live via `Clay_GetElementData` for layout-managed
  buttons, or computed from sprite anchor for absolute buttons) contains
  `ctx.mouse_x/y`.
- **`render.h` / `render.cpp`** — `Render(root, ctx, target, renderer)`
  walks the tree depth-first and blits into `target`. Buttons inside
  a Clay subtree query `Clay_GetElementData(node.clay_id)` for their
  box and `Clay_PointerOver(node.clay_id)` for hover; absolute buttons
  use `.at()` + sprite anchor (legacy semantics, pixel-identical with
  the legacy widget render).
- **`screens/`** — one file per screen. Each defines a single pure
  `Build*(const Context&)` function returning the tree *and* a
  `<Name>Runtime` class (the engine wire-in). The two live in the same
  TU because they're tightly coupled: the Runtime calls `Build*` each
  frame, and the preview harness calls `Build*` standalone.
  `screen_context.{h,cpp}` (the engine-side bridge passed into
  `Build*`) lives here too.
- **`modals/`** — v2 message + password modals. Same Build/Runtime
  shape as screens.
- **`runtime.h`** — abstract `Runtime` base. `Game` holds a
  `std::unique_ptr<Runtime> active_runtime` and swaps it on state
  transition via `SetRuntime(GameState)`. Per-frame, Game's render
  branch delegates to `active_runtime->Render(...)`; events.cpp routes
  mouse / keyboard / text input through `DispatchMouseDown` /
  `DispatchKeyDown` / `DispatchTextInput`. `Tick()` advances state
  machines (lobby networking, rebind capture, update STAGING, etc.).
- **`modal_stack.{h,cpp}`** — `ModalStack` owns the v2 message /
  progress / password overlay stack. Rendered on top of any active
  runtime; intercepts mouse / key / text input when non-empty. Game
  keeps thin Show*/Pop*/Dispatch* wrappers since external callers
  (screen_context, lobby panels) use them by name.
- **`ingame_chat.{h,cpp}` / `ingame_buy.{h,cpp}` / `ingame_tech.{h,cpp}`** —
  in-game overlays composed on top of INGAME (not state-owned, so
  they don't go through `SetRuntime`). Each owns its own state +
  render + dispatch.

Container expression is the **preferred** layout style for new
screens. Absolute `.at()` is the documented escape hatch for screens
that need pixel-identical legacy parity with a non-container-shaped
layout (e.g. MainMenu's staggered button positions).

## Authoring rules

- Screens are **pure functions of `Context`**. No globals, no static
  state, no side-effects in `Build*`. Click handlers run later, in
  input dispatch.
- Use the factory functions; don't construct `Node` literals
  field-by-field. The factories document intent.
- **Don't `using namespace ui::v2;`** in a TU that also includes
  legacy engine headers — `ui::v2::Sprite` / `Button` / etc. share
  names with engine classes (`::Sprite`, `::Button`). Qualified
  calls or `using ui::v2::Button;` per name are fine. (The
  `ui::v2` namespace is a relic of the side-by-side migration; not
  worth churning every TU to drop the `v2` segment.)
- Adding a new screen: drop `screens/<name>.{h,cpp}` (Build factory
  + `<Name>Runtime` class) and add one case to `Game::SetRuntime`.
  Preview / PPM-diff support: extend `preview.cpp` with a
  `<name>` `--preview-screen` branch.

## Preview harness

`preview.cpp` (entered via `Game::RunPreview`) renders any registered
screen to PPM or an interactive SDL window. Used by the dev loop and
the (now-retired) byte-identical PPM gate against legacy. Storybook
(`storybook.cpp`) is an interactive component playground, launched
via `--preview-screen storybook`.
