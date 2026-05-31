# clients/silencer/src/ui - UI Runtime And Primitives

This subtree owns the reusable UI substrate: design tokens, the retained cppx
runtime/style/components, interaction registry/router, text/layout helpers, and
shared primitives. It must stay screen-agnostic and game-agnostic.

`src/ui/components` is the generic cppx component layer imported from the
authoritative cppx repo. Treat `.cppx` and `.hx` as source files and generated
files under `build/generated/cppx/` as disposable build output.

The retained cppx runtime is the target UI architecture. Production client UI
must be authored as components, hooks, and providers. This layer
does not own screen navigation, game state, SDL events, audio playback, or
final renderer submission.

This is mid-migration toward good flexbox layout and shadcn-style primitive API first principles. If you touch stale code that conflicts with those principles, update it in the same change.

## Boundaries

- Do not include or depend on `Game`, `World`, `ScreenContext`, concrete screens, audio, SDL event loops, `Renderer`, or `Surface` from this layer. Use narrow runtime state, primitive options, and custom payload contracts instead.
- Retained providers and components own UI composition. Do not add a separate
  view/action mediation layer.
- `UiInteractionRegistry` owns semantic metadata, focus, text editing, pointer hit testing, keyboard/gamepad navigation, automation, and typed action queuing.
- Custom render payloads are the renderer bridge. Keep sprite-bank details inside payloads or existing bridge primitives; do not leak them into new public primitive APIs.
- Public cppx component contracts are props and children. Do not expose
  `UiElementFrame`, retained builders, context bags, or renderer plumbing in
  authored component APIs.
- Keep shared runtime APIs React-style: components, hooks, providers, and
  narrow services only. Do not add a separate view/action mediation layer.

## Primitive API

- Target public primitives are plain nouns: `Button`, `TextInput`, `Toggle`, `Panel`, `Text`. Runtime/service types keep the `Ui` prefix: `UiInteractionRegistry`, `UiInputState`, `UiInputRouter`.
- Text consumers use the `Text` primitive plus semantic text metrics. Keep sprite-bank and font details behind the text primitive/render boundary.
- Primitive APIs follow shadcn's core shape, not its exact implementation: `variant + size`, composition, and named defaults. Repeated call-site option bundles should become named variants or sizes.
- New or cleaned-up primitive APIs must not expose palette indices, sprite banks, legacy `B196x33`-style codes, or one-consumer presets in public signatures, enums, or docs.
- One primitive owns one concern. Checkbox/toggle state belongs to checkbox/toggle primitives, not a `Button` mode.

## Verification

- Build through `clients/silencer/build.ps1` or `clients/silencer/build.sh`; do not run raw CMake/Ninja commands.
- Run `cmake --build clients/silencer/build --target silencer_cppx_check` after
  editing `.cppx` or `.hx`.
- For primitive/API work, add runtime screenshot or control-socket verification when visual/interaction behavior is at risk.
