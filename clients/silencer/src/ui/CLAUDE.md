# clients/silencer/src/ui - UI Runtime And Primitives

This subtree owns the reusable UI substrate: design tokens, Clay frame runtime, interaction registry/router, text/layout helpers, and shared primitives. It must stay screen-agnostic and game-agnostic.

Clay is a flexbox-like frame layout and render-command generator. This layer wraps Clay with Silencer's design-system primitives; it does not own screen navigation, game state, SDL events, audio playback, or final renderer submission.

This is mid-migration toward good flexbox layout, Clay lifecycle, and shadcn-style primitive API first principles. If you touch stale code that conflicts with those principles, update it in the same change.

## Boundaries

- Do not include or depend on `Game`, `World`, `ScreenContext`, concrete screens, audio, SDL event loops, `Renderer`, or `Surface` from this layer. Use narrow runtime state, primitive options, and custom payload contracts instead.
- `ClayService` owns production Clay frame lifecycle. Primitives declare Clay inside an existing frame; they do not begin/end layout, set pointer state, update scroll containers, or render command streams.
- `ClayService` also brackets the React-style runtime frame. Component code may
  use `REACT_COMPONENT_*`, hooks, refs, effects, and providers while declaring
  UI, but it must not call `react_begin_frame`, `react_end_frame`, or
  `react_shutdown` directly outside focused runtime tests.
- `ClientUi` owns the `UiFocusRuntime` frame lifecycle. Components may declare
  focus scopes and focusables, but they must not call `ui_focus_begin_frame`,
  `ui_focus_end_layout`, `ui_focus_set_current`, or `ui_focus_init` directly.
- `UiInputState::source` is set by input adapters for real keyboard, gamepad,
  and pointer edge sources. Focus code should not treat a held pointer as a
  new mouse-source frame when keyboard/gamepad navigation also arrives.
- Screen-local navigation and state writes should go through ClientUi's
  screen provider hooks (`UseScreenNavigator`, `UseUiWriteQueue`) so mutations
  drain after layout/render instead of during declaration.
- `UiInteractionRegistry` owns semantic metadata, focus, text editing, pointer hit testing, keyboard/gamepad navigation, automation, and typed action queuing. Clay still owns layout and final bounds.
- Custom render payloads are the renderer bridge. Keep sprite-bank details inside payloads or existing bridge primitives; do not leak them into new public primitive APIs.

## Primitive API

- Target public primitives are plain nouns: `Button`, `TextInput`, `Toggle`, `Panel`, `Text`. Runtime/service types keep the `Ui` prefix: `UiInteractionRegistry`, `UiInputState`, `UiInputRouter`, `UiFrameContext`.
- Text consumers use the `Text` primitive plus semantic `TextSize` metrics. Keep sprite-bank and Clay font fields behind the text primitive/compositor boundary.
- Primitive APIs follow shadcn's core shape, not its exact implementation: `variant + size`, composition, and named defaults. Repeated call-site option bundles should become named variants or sizes.
- New or cleaned-up primitive APIs must not expose palette indices, sprite banks, legacy `B196x33`-style codes, or one-consumer presets in public signatures, enums, or docs.
- One primitive owns one concern. Checkbox/toggle state belongs to checkbox/toggle primitives, not a `Button` mode.

## Clay Discipline

- Prefer flexbox-style Clay layout: sizing, grow/fit, padding, gaps, alignment, clipping, and stable containers. Absolute coordinates, sprite-offset nudges, and hand-measured widths are legacy escape hatches to remove when practical.
- Every interactive, animated, scrollable, custom-rendered, tested, or automation-visible element needs an explicit stable Clay ID. A visible label must never double as the element ID.
- Dynamic strings and custom payloads must live until after Clay render command consumption. Use per-frame primitive arenas; production resets them once from `UiFrameContext::BeginFrame`.

## Verification

- Build through `clients/silencer/build.ps1` or `clients/silencer/build.sh`; do not run raw CMake/Ninja commands.
- For primitive/API work, run the relevant `clay_*_check` control-socket ops through `clients/cli/index.ts`; add runtime screenshot or control-socket verification when visual/interaction behavior is at risk.
