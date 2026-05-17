# clients/silencer/src/client/ui - Client UI

This subtree owns Silencer's app-side UI composition: screen/modal navigation, HUD/overlay declaration, screen-local components, and the bridge from semantic UI actions back into game state.

Clay is a frame layout and render-command generator. It is not the application state owner, event loop, renderer, widget toolkit, or navigation stack.

This UI is actively migrating toward good flexbox layout, Clay lifecycle, and shadcn-style primitive API first principles. If you touch stale code that conflicts with those principles, update it in the same change instead of preserving the old pattern.

## Ownership

- `ClientUi` owns one visible UI frame. `Game::RenderClientUiFrame` prepares input, begins one `ClientUi`/`ClayService` frame, asks visible layers to declare UI, ends the frame, renders commands, then dispatches typed UI actions.
- Screens, modals, HUD, and overlays only declare UI into the current frame. They must not call `Clay_BeginLayout`, `Clay_EndLayout`, `Clay_SetPointerState`, `clay_bridge::EnsureInitialized`, or `clay_bridge::Render`.
- Clay owns layout, wrapping, clipping, hover state, scroll containers, and final bounds. `UiInteractionRegistry` owns semantic metadata, focus, text editing, pointer hit testing, keyboard/gamepad navigation, automation, and typed actions.
- Prefer flexbox-style Clay layout: sizing, grow/fit, padding, gaps, alignment, and stable containers. Treat absolute coordinates and sprite-offset nudges as legacy escape hatches to remove when practical.
- The compositor/render layer owns sprite banks, palette effects, text drawing, clipping, and custom-payload dispatch. UI screens and ordinary primitives do not call SDL, `Renderer`, or `Surface` APIs directly.
- Screen-specific components stay under the owning screen directory. Promote a component only after there is real reuse.

## Primitive API

- Target public primitives are plain nouns: `Button`, `TextInput`, `Toggle`, `Panel`. Runtime/service types keep the `Ui` prefix: `UiInteractionRegistry`, `UiInputState`, `UiInputRouter`.
- Public primitive API follows shadcn's core shape, not its exact implementation: `variant + size`, composition, and named defaults. `variant` names the visual treatment; `size` names scale or fit behavior.
- Existing bridge primitives may still expose bank/palette details. Do not spread that into new or cleaned-up primitive signatures, enums, or comments.
- If many call sites pass the same option values, grow a named variant or size. Do not let padding, min/max width, wrap, or effect-color escape hatches become the normal API.
- One primitive owns one concern. Checkbox/toggle state belongs to checkbox/toggle primitives, not a `Button` mode.
- Every interactive, animated, scrollable, custom-rendered, tested, or automation-visible element needs an explicit stable Clay ID. A visible label must never double as the element ID.
- Dynamic strings and custom payloads must live until after Clay render command consumption. Use per-frame primitive arenas; reset them once from `ClientUi::BeginFrame`, never from a screen.

## Interaction

- Normalize input once at the platform boundary. The same coordinate space must feed `Clay_SetLayoutDimensions`, pointer state, registry hit testing, and compositor output.
- Queue state changes as typed `UiAction`s and handle them after layout. Do not mutate screen stacks, world state, or domain state from inside a Clay declaration block.
- Target UI feedback is declared by the widget/primitive and executed by the client layer from normalized transitions such as pointer enter, focus enter, press, release, and activate. Existing `ClientUi` button/toggle audio inference is migration debt; do not extend it.

## Verification

- Build through `clients/silencer/build.ps1` or `clients/silencer/build.sh`; do not run raw CMake/Ninja commands.
- For primitive/API work, run targeted lobby UI tests such as `tests/lobby-ui/button_test/run.sh`. For the stepped right-pane chrome specifically, use `tests/lobby-ui/lobby_stepped_pane_test/run.sh` rather than the retired legacy-parity right-pane panel harnesses. Add `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` when ownership boundaries change.
- If visual or interaction behavior is in question, verify the real runtime through the client/control socket/screenshots, not compile success alone.
