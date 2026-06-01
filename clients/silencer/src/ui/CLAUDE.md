# clients/silencer/src/ui - UI runtime + styling substrate

This subtree is the golden retained cppx UI engine: a React-style hook
runtime, Yoga flex layout, a focus/interaction model, and an RGBA
draw-command IR, plus the styling substrate (theme, visual style, resolve).
It must stay screen-agnostic and game-agnostic — it knows nothing about
`Game`, `World`, SDL, audio, or the renderer's SDL handles.

## Layout

| Dir/file | What it owns |
|---|---|
| `runtime/react.{h,cpp}` | Hook runtime: fibers, `use_state`/`use_effect`/`use_ref`/`use_callback`, `PROVIDE`/`use_context`, per-frame begin/end. |
| `runtime/element.h`, `tree.{h,cpp}` | `UiElement` descriptors + the retained node tree the reconciler commits. |
| `runtime/flex_layout.*`, `yoga_flex_layout.*` | Yoga-backed flexbox layout adapter (sizing, grow/fit, padding, gaps, alignment). |
| `runtime/focus.*`, `interaction_hooks.*` | Focus runtime + per-frame interaction reads (`use_focused`/`use_hovered`/`use_pressed`/`use_focus_visible`, one-frame lag by design). |
| `runtime/geometry.*` | Points, sizes, rects. |
| `runtime/draw_command.{h,cpp}`, `draw_command_builder.*` | The tagged-union, premultiplied-RGBA `DrawCommandList` IR + its pure transcriber. |
| `style/visual_style.h` | `VisualStyle`: the dense resolved paint description the renderer reads (straight-alpha authoring). |
| `style/style_patch.h` | Sparse `StylePatch` / `StyleStatePatch` overlays. |
| `style/theme.h`, `default_theme.cpp` | `Theme`/`RoleStyle` + the neutral fallback theme (`use_theme()` via `ThemeContext`). |
| `style/resolve.{h,cpp}` | `resolve()` — layers role + override patches + interaction state into one `VisualStyle` at authoring time. |
| `style/text_measure.{h,cpp}`, `text_wrap.{h,cpp}` | SDL-free text measure/wrap seam (`set_text_measurer`); the renderer installs the real measurer. |
| `input.h` | `UiInputFrame` — the per-frame nav/confirm/cancel/pointer + key/text/editing event channels. |

The legacy `design/Colors.h` + `Spacing.h` constants are gone (SIL-17). The
single source of app paint is now the design tokens (`silencer::tokens` in
`src/client/ui/components/tokens.h`) plus the product theme
(`src/client/ui/app_theme.cpp`); components resolve their `VisualStyle` from
those, never from a standalone palette/spacing header.

## Boundaries

- Do not include or depend on `Game`, `World`, concrete screens, audio, SDL,
  `Renderer`, or `Surface` from this layer. The renderer reads the SDL-free
  `DrawCommandList` IR; text measurement is injected via `set_text_measurer`.
- The runtime owns layout, focus, hit testing, and IR emission. Components
  compose hooks + elements; they do not run the frame lifecycle (that is the
  app-shell's `UiPipeline`, `src/client/ui`).
- Colors are authored straight-alpha and premultiplied once at the IR emit
  boundary. The IR carries integer handles (text/font/texture ids), never
  pointers, so it stays trivially copyable and relocatable.

## Authoring discipline

- Theme is read with `use_theme()`; components resolve their own `VisualStyle`
  via `resolve()` at authoring time and pass it down. The renderer never sees
  the theme.
- Reorderable same-type siblings need stable keys (`REACT_COMPONENT_BEGIN_KEY`)
  so fiber identity — and the hook state keyed to it — survives reorders.
- Dynamic per-frame text uses the per-call-site `use_text_storage`
  scratch; do not hand the IR a pointer into a transient stack buffer.

## Verification

- Build through `clients/silencer/build.ps1` or `clients/silencer/build.sh`;
  do not run raw CMake/Ninja commands.
- Add runtime screenshot / control-socket verification (via
  `clients/cli/index.ts`) when visual or interaction behavior is at risk;
  compile success alone is not sufficient.
