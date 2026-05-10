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

- **`node.h`** — `Node` value type + `NodeKind` enum + factories
  (`Background`, `Sprite`, `Label`, `Button`, `Group`). A screen is
  a function returning a `Node`; the tree is pure data.
- **`context.h`** — `Context` is the bag of inputs a screen depends on
  (asset catalog, logical dimensions, scale, version string). No
  `Game`/`World`/`Lobby` refs — screens stay testable in the
  preview harness.
- **`render.h` / `render.cpp`** — `Render(root, ctx, target, renderer)`
  walks the tree depth-first and blits into `target`. Sprite asset
  anchor offsets are honored exactly like the legacy widgets so a
  screen built with the same coords renders the same pixels.
- **`screens/`** — one file per screen. Each defines a single pure
  `Build*(const Context&)` function returning the tree.

Future containers (`VStack`, `HStack`, `Center`, `Padding`) and the
Yoga integration land in the PR that ships the first screen using
them; PR #1 is absolute-positioned only to keep the pixel diff
trivially verifiable against the live game.

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
