# clients/silencer/src/ui - UI Runtime And Primitives

This subtree owns the reusable UI substrate: design tokens, the retained cppx
runtime/style/components, Clay frame runtime, interaction registry/router,
text/layout helpers, and shared primitives. It must stay screen-agnostic and
game-agnostic.

`src/ui/components` is the generic cppx component layer imported from the
authoritative cppx repo. Treat `.cppx` and `.hx` as source files and generated
files under `build/generated/cppx/` as disposable build output.

Clay is still the production compositor for HUD and legacy bridge primitives
during the migration. The retained cppx runtime is the target UI model. This
layer does not own screen navigation, game state, SDL events, audio playback, or
final renderer submission.

This is mid-migration toward good flexbox layout, Clay lifecycle, and shadcn-style primitive API first principles. If you touch stale code that conflicts with those principles, update it in the same change.

## Boundaries

- Do not include or depend on `Game`, `World`, `ScreenContext`, concrete screens, audio, SDL event loops, `Renderer`, or `Surface` from this layer. Use narrow runtime state, primitive options, and custom payload contracts instead.
- `ClayService` owns production Clay frame lifecycle while legacy screens remain.
  Primitives declare Clay inside an existing frame; they do not begin/end
  layout, set pointer state, update scroll containers, or render command
  streams.
- `UiInteractionRegistry` owns semantic metadata, focus, text editing, pointer hit testing, keyboard/gamepad navigation, automation, and typed action queuing. Clay still owns layout and final bounds.
- Custom render payloads are the renderer bridge. Keep sprite-bank details inside payloads or existing bridge primitives; do not leak them into new public primitive APIs.
- Public cppx component contracts are props and children. Do not expose
  `UiElementFrame`, retained builders, context bags, or renderer plumbing in
  authored component APIs.

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
- Run `cmake --build clients/silencer/build --target silencer_cppx_check` after
  editing `.cppx` or `.hx`.
- For primitive/API work, run the relevant `clay_*_check` control-socket ops through `clients/cli/index.ts`; add runtime screenshot or control-socket verification when visual/interaction behavior is at risk.
