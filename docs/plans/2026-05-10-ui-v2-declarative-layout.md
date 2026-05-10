# UI v2 — declarative layout rewrite for the Silencer client

**Status:** In progress
**Date:** 2026-05-10
**Companion progress doc:** [2026-05-10-ui-v2-progress.md](2026-05-10-ui-v2-progress.md)

## Intent

Replace the current imperative widget tree in `clients/silencer/src/ui/`
with a declarative, value-typed Node tree so that:

- **Coding agents can author and modify UI correctly** without falling
  into the friction traps that exist today (sprite-anchor coord
  conventions, polled `clicked` flags, `uid` collisions, dual
  `objects` / `tabobjects` lists, magic `res_bank` integers,
  two-place type-switches in `interface.cpp` and `renderer.cpp`,
  manual text-centering math).
- **UI is interpretable from the source alone** — a screen reads like
  a React component (background → logo → label → buttons), not like
  fifty `world.CreateObject` + `strcpy` + `AddObject` lines.
- **Resolution-independent rendering** with integer-scaled
  pixel-art chrome supports cross-platform deployment (consoles, most
  monitor sizes) without sacrificing the existing visual style.

## Constraints (user-confirmed)

1. **1:1 visual match with the current Silencer client** during the
   migration. Pixel-identical PPM output between the v2 path and the
   legacy widget path for any screen that has been migrated, with the
   diff empty.
2. **Standalone runnable outside the main client** so individual
   screens can be iterated without launching the full game.
3. **Hot reload** on the standalone surface so save-rebuild-render is
   the iteration loop.
4. **Cross-platform including consoles** — the chosen layout engine
   and rendering strategy must work on the platforms Silencer targets.
5. **Support most monitor sizes** via native-resolution rendering
   with a per-display integer scale factor for chrome. UI lays out in
   *logical* pixels; sprite chrome is blitted at integer scales (1×,
   2×, 3×, …) preserving the pixel-art aesthetic.
6. **UI feature set will grow** — the abstraction must accommodate new
   screens, new widgets, and richer layouts over years.

## Approach

### Library shape

A new directory `clients/silencer/src/ui/v2/` (intent: temporary name
`v2/`; renames to `ui/` once the legacy tree is deleted). Three layers:

1. **Tree** — `Node` is a value-typed struct with a kind tag
   (`Background`, `Sprite`, `Label`, `Button`, `Group`, …) plus a
   children vector. Factories build nodes inline so a screen reads as
   nested function calls. Chainable `.at(x, y)` / `.onClick(...)` mutators.
2. **Layout pass** — walks the Node tree and produces a flat list of
   rendered positions. For absolute-positioned nodes (PR #1) this is
   a direct copy; for container nodes (`VStack` / `HStack` / `Center` /
   `Padding`, when introduced) the layout pass delegates to Yoga.
3. **Render pass** — walks the layout output and calls into the
   engine's existing low-level primitives (`Renderer::DrawSpriteAt`,
   `Renderer::DrawText`, palette-indexed `Surface`). Future Path B
   work multiplies blits by `Context.scale` for high-DPI / TV output.

### Authoring contract

```cpp
Node BuildMainMenu(const ui::v2::Context & ctx) {
    std::string version = std::string("Silencer v") + ctx.version;
    return Background(/*bank=*/6, /*index=*/0, {
        Sprite(/*bank=*/208, /*index=*/60),
        Label(version, /*font_bank=*/133, /*font_width=*/11)
            .at(10, ctx.logical_h - 17),
        Button("Tutorial").at(40, -134),
        Button("Connect To Lobby").at(80, -67),
        Button("Options").at(40, 0),
        Button("Exit").at(0, 67),
    });
}
```

Screens are **pure functions of `Context`**: no globals, no
side-effects in `Build`, no engine state mutated until input dispatch
runs the click handlers.

### Layout engine

**Clay** (Nic Barker's single-header C flexbox-ish layout library,
designed for games). Vendored as a single header file. Microsecond-
class on small trees, emits render commands that drop straight into
our palette-indexed framebuffer, no exceptions, no STL, no retained
view tree. Matches our pure-`Build`-each-frame architecture.
**Deferred until the first container node lands** — PR #1's MainMenu
uses absolute positioning to preserve pixel-identical output against
legacy, so a layout engine would be dead weight in the foundation PR.

Clay also owns one piece of UI state (scroll offsets) because scroll
needs to feed back into measurement. Everything else (hover, focus,
textbox, etc.) lives in our own `UIState` (see "State model" below).

**Why not Yoga.** Yoga is C++20 (we're C++14), considerably heavier,
and designed to feed a retained native view tree (React Native's
shadow tree). It fights our "rebuild the tree every frame" model.
Earlier drafts of this doc picked Yoga for "console-proven via React
Native"; Clay is also console-targeted and doesn't drag the
framework with it.

### State model

`Build` is a pure function of `Context`, but persistent things — hover
animation phase, keyboard focus, textbox cursor, scroll offset — must
live somewhere across frames. The model:

- A single `UIState` (per screen, per session, or per frame-stack —
  scope TBD when in-game UI lands) owns a few `unordered_map<NodeId,
  T>` shards, one per state type:
  - `anim` (float per ID, e.g. `hot_t` and `active_t`)
  - `text` (textbox cursor / contents)
  - `focus` (single hashed-ID slot, "who has focus")
  - scroll lives inside Clay (the carve-out)
- `Node` carries an optional explicit string `key`. Nodes that need
  per-instance state set it via `.key("foo")`. The `Button` factory
  auto-keys from its text label, since labels are unique within a
  screen — authors don't have to think about keys for the common case.
- `Render` and `DispatchClick` take `Context` (which holds a
  `UIState*` + frame `dt`). They read/write through the maps using
  hashed keys.
- End of frame: any IDs not visited this frame get GC'd from the maps
  (and, eventually, get cleanup effects emitted — like a "useEffect
  return" in React, but explicit).

**Why explicit keys, not React-style hooks.** React hooks rely on TLS
+ a "rules of hooks" lint rule + a fiber tree to attach the per-hook
slot array to a position-in-call-order. C++ has no lint enforcing
that, and coding agents will silently violate the rules and get
undefined behavior. Explicit `string` keys are greppable, refactorable,
and a stale key is a visible bug instead of UB.

### Animation

Two persistent floats per ID, exponentially approached toward a target
each frame — Ryan Fleury's "hot_t / active_t" pattern from the RAD
Debugger UI series:

```cpp
hot_t    = Approach(hot_t,    hovered ? 1.f : 0.f, /*rate=*/12.f, dt);
active_t = Approach(active_t, pressed ? 1.f : 0.f, /*rate=*/20.f, dt);
```

Visual mapping is per-widget: a button might map `hot_t` to chrome
frame index (`base + (int)(hot_t * 4)`) and brightness, `active_t` to
an extra brightness pulse on press. The legacy 4-frame ACTIVATING /
DEACTIVATING animation reduces to one `hot_t` slot.

This handles every smooth-state transition we have today (hover
brighten, button press) plus everything we're likely to add (fade-in,
focus halo, tooltip pop-in). No per-screen animation bookkeeping.

### Verification

Per-screen pixel-identity gate. For each screen migrated:

1. Render via the legacy path (`MainMenuScreen::Build` → `Renderer::Draw`)
   and dump 640×480 P6 PPM.
2. Render the same screen via the v2 path and dump a PPM.
3. Byte-diff the two. **Empty diff is the merge gate.**

Both paths run through `Silencer.exe`'s `--preview-screen NAME
--preview-impl v2|legacy --dump-ppm PATH` mode. Headless dump
(`--headless`) skips the SDL window for fast CI / agent verification.

### Standalone preview

`Silencer.exe --preview-screen main_menu --preview-impl v2` opens an
SDL3 window rendering one screen, re-rendering every frame so a
rebuild + restart picks up source changes. Restart-on-rebuild is the
PR #1 iteration loop; a shared-lib swap for true hot-reload is
deferred.

### Migration discipline

- One screen per PR.
- The PR ships **byte-identical pixel match** between v2 and legacy
  for that screen before flipping the live game over.
- When a legacy widget class (`Button`, `Overlay`, `TextInput`, etc.)
  has no remaining users, **delete it** in the same PR — and the
  matching `case` arms in `interface.cpp` and `renderer.cpp`.

## Path B — resolution-flexible rendering

Today the engine renders to a fixed 640×480 indexed framebuffer and
the SDL3 GPU backend upscales to the window (with a nearest /
bilinear toggle). For cross-platform + multi-monitor support the UI
moves to:

- Renderer takes `(window_w, window_h)`.
- `ui_scale = ChooseScale(window_h)` — `1` for `<720`, `2` for
  `720–1439`, `3` for `1440–2159`, `4` for `2160+` (initial table,
  open to tuning).
- `Context.logical_w = window_w / ui_scale`,
  `Context.logical_h = window_h / ui_scale`.
- Yoga lays out in logical pixels.
- Renderer blits sprite chrome at integer `ui_scale` (true
  pixel-doubling — preserves pixel-art crispness at any resolution).
- Hit-test divides cursor by `ui_scale` to land on logical-pixel
  rects.

The world/game scene can keep the 640×480 buffer + GPU upscale if
that's preferred for art-direction reasons; Path B is a UI-subsystem
change.

## What's deliberately NOT in this plan

(Sections below are scope **the user has not asked for**. They will
land as their own follow-ups, or never. Empty by design — when
something gets ruled out explicitly, it moves here.)

- *(nothing yet)*

## Open questions

These need a decision before the affected phase starts:

- **Bake sprite anchor offsets out of assets, or keep them.** Today's
  button chrome carries `offsetx=-310, offsety=-288` baked in,
  forcing the "magic negative-anchor" coordinate convention. Baking
  them out lets layout primitives operate in clean screen-pixel
  space; keeping them means Clay containers (and friends) quietly
  subtract the offsets forever. Not blocking PR #1 / #2 — affects the
  PR that introduces containers.
- **Hot-reload mechanism beyond restart-on-rebuild.** Currently the
  preview restarts after every `cmake --build`. A shared-library swap
  (dlopen the screen's `.o` and reload on file change) is on the
  table if iteration speed bites. Not blocking PR #1.
- **`UIState` lifetime / scope.** Per-screen instance, per-game-session
  singleton, or stack alongside the screen stack? Per-screen is the
  obvious default but a stack matches the screen-stack semantics
  (modal opens → its UIState stacks on top). Decide when in-game UI
  (chat / buy / tech) lands, since those genuinely overlap.
- **Logo animation parity.** The live game animates the MAINMENU
  logo through frames 29–60–29 via `Overlay::Tick`. v2 currently
  picks the steady-state frame (60) and renders static. The
  `hot_t`/`active_t` pattern doesn't naturally express a triangle
  wave; this likely wants its own `anim_phase` slot in `UIState`
  driven by an explicit time function. Not blocking pixel-identity
  at the steady-state frame.
- **In-game UI migration timing.** Chat / buy / tech popups are
  spawned imperatively from `Player::Tick` (`actors/player.cpp`) and
  bypass the Screen system. They become v2 trees in some later PR;
  exactly when is open.

## Reference

- Current UI subject-matter map: see the long-form audit captured by
  the SMD agent run in session `33c7a70f` (May 2026 — referenced in
  conversation, not committed). Key files: `clients/silencer/src/ui/
  components/`, `clients/silencer/src/ui/screens/`, switch arms in
  `clients/silencer/src/render/renderer.cpp` and
  `clients/silencer/src/ui/components/interface.cpp`.
- Companion progress + handoff doc: [2026-05-10-ui-v2-progress.md](2026-05-10-ui-v2-progress.md).
