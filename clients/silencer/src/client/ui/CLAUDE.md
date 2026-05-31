# clients/silencer/src/client/ui - Client UI

This subtree owns Silencer's app-side UI composition: screen/modal navigation,
HUD/overlay composition, screen-local components, and the bridge from semantic
UI actions back into game state.

This UI is actively migrating toward retained cppx roots, hooks/providers, good
flexbox layout, and shadcn-style primitive API first principles.
If you touch stale code that conflicts with those principles, update it in the
same change instead of preserving the old pattern.

## Ownership

- `ClientUi` owns one visible UI frame. `Game::RenderClientUiFrame` prepares input, begins one `ClientUi` frame, asks visible screens, modals, HUD, and overlays for retained roots, ends the frame, renders retained commands, then dispatches typed UI actions.
- Screens, modals, HUD, and overlays return retained cppx roots from `BuildElement` or provider-backed components. They must not depend on legacy immediate-mode UI APIs.
- The retained tree owns layout, focus, hover, and final bounds. `UiInteractionRegistry` owns semantic metadata, text editing, keyboard/gamepad navigation, automation, and typed actions.
- Prefer flexbox-style retained layout: sizing, grow/fit, padding, gaps, alignment, and stable containers. Treat absolute coordinates and sprite-offset nudges as legacy escape hatches to remove when practical.
- The compositor/render layer owns sprite banks, palette effects, text drawing, clipping, and custom-payload dispatch. UI screens and ordinary primitives do not call SDL, `Renderer`, or `Surface` APIs directly.
- Screen-specific components stay under the owning screen directory. Promote a component only after there is real reuse.
- Keep screen/component APIs React-style: props, children, hooks, and providers.
  Do not add a separate view/action mediation layer.

## Primitive API

- Target public primitives are plain nouns: `Button`, `TextInput`, `Toggle`, `Panel`. Runtime/service types keep the `Ui` prefix: `UiInteractionRegistry`, `UiInputState`, `UiInputRouter`.
- Public primitive API follows shadcn's core shape, not its exact implementation: `variant + size`, composition, and named defaults. `variant` names the visual treatment; `size` names scale or fit behavior.
- Existing sprite-backed render helpers may still expose bank/palette details. Do not spread that into new or cleaned-up primitive signatures, enums, or comments.
- If many call sites pass the same option values, grow a named variant or size. Do not let padding, min/max width, wrap, or effect-color escape hatches become the normal API.
- One primitive owns one concern. Checkbox/toggle state belongs to checkbox/toggle primitives, not a `Button` mode.
- Every interactive, animated, scrollable, custom-rendered, tested, or automation-visible element needs an explicit stable ID. A visible label must never double as the element ID.
- Dynamic strings and custom payloads must live until after retained render command consumption. Use per-frame arenas; reset them once from `ClientUi::BeginFrame`, never from a screen.

## Interaction

- Normalize input once at the platform boundary. The same coordinate space must feed retained layout, pointer state, registry hit testing, and renderer output.
- Queue state changes as typed `UiAction`s and handle them after layout. Do not mutate screen stacks, world state, or domain state from inside component declaration.
- Target UI feedback is declared by the widget/primitive and executed by the client layer from normalized transitions such as pointer enter, focus enter, press, release, and activate. Existing `ClientUi` button/toggle audio inference is migration debt; do not extend it.

## Verification

- Build through `clients/silencer/build.ps1` or `clients/silencer/build.sh`; do not run raw CMake/Ninja commands.
- For primitive/API work, run `python3 clients/silencer/tools/check-cppx.py`. Add `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` when ownership boundaries change.
- If visual or interaction behavior is in question, verify the real runtime through the client/control socket/screenshots, not compile success alone.
