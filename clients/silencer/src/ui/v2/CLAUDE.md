# `ui/v2/` — declarative UI library

Greenfield replacement for the imperative widget tree in `ui/components/`,
`ui/screens/`, `ui/panels/`, `ui/modals/`. Ships side-by-side with the
legacy code while screens migrate one at a time. The legacy directories
get deleted as their last user moves over.

## Why it exists

The legacy widgets are `Object` subclasses in `world.objectlist`,
hand-positioned with sprite-anchor-relative integer coordinates, polled
each frame for `clicked` flags. Coding agents struggle to author them
correctly: silent `uid` collisions, magic `res_bank` integers, dual
`objects`/`tabobjects` lists, manual text-centering math, and
forget-`clicked = false` re-fire bugs are routine. Documented in detail
in the design conversation that produced this library.

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
  and emits a Clay scope per container subtree, then reads back
  rects via `Clay_GetElementData` into `node.rect_*`. Nodes outside
  any container subtree are left with `rect_w == 0` so render +
  dispatch fall through to absolute `.at()` positioning.
- **`render.h` / `render.cpp`** — `Render(root, ctx, target, renderer)`
  walks the tree depth-first and blits into `target`. Buttons inside
  a Clay subtree use the computed `rect_*`; absolute buttons use
  `.at()` + sprite anchor (legacy semantics, pixel-identical with
  the legacy widget render).
- **`dispatch.{h,cpp}`** — `DispatchClick(root, ctx)` and `ButtonHit`.
  Hit-test reads `rect_*` for Clay-managed buttons; falls back to
  the absolute path otherwise. Must call `Layout()` before dispatch
  so rects exist.
- **`screens/`** — one file per screen. Each defines a single pure
  `Build*(const Context&)` function returning the tree *and* a
  `<Name>Runtime` class (the engine wire-in). The two live in the same
  TU because they're tightly coupled: the Runtime calls `Build*` each
  frame, and the preview harness calls `Build*` standalone.
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
  calls or `using ui::v2::Button;` per name are fine.

## Status

- PR #1: skeleton + `BuildMainMenu` declared and pixel-equivalent to
  `MainMenuScreen::Build` (verification harness pending in chunk 2).
- PR #1 chunk 2: standalone preview harness (separate executable
  target sharing the engine's render primitives) + PPM diff tool.
- PR #2+: per-screen migrations; engine-side widget classes deleted
  as their last user moves.
