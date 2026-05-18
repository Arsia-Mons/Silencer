---
name: clay-ui-integration
description: Use when designing, implementing, or auditing Silencer's Clay UI integration in clients/silencer. Applies to C++/SDL3 work involving Clay lifecycle, flexbox-style layout, responsive primitives, input normalization, UiInteractionRegistry, ClientUi, render command dispatch, custom payloads, text measurement, IDs, scrolling, or UI architecture boundaries.
---

# Silencer Clay UI Integration

## Sources Of Truth

For Silencer work, read local repo sources before upstream examples:

- `clients/silencer/src/ui/CLAUDE.md` for generic runtime/primitives boundaries.
- `clients/silencer/src/client/ui/CLAUDE.md` for screens, HUD, navigation, input, and feedback ownership.
- `clients/silencer/third_party/clay/clay.h` for the exact Clay API version in this repo.
- `clients/silencer/src/ui/runtime/ClayService.*` and `clients/silencer/src/client/ui/ClientUi.*` for the production frame path.

Read [references/sources.md](references/sources.md) only when you need deeper Clay references or upstream comparisons. Upstream `main` is useful research, not the API contract for this checkout.

## Core Model

Clay is a frame-based flex layout and render-command generator. It is not the event loop, screen stack, renderer, widget toolkit, application state owner, or audio/feedback policy owner.

Silencer owns those responsibilities around Clay:

- `ClientUi` owns one visible UI frame and screen/modal navigation.
- `ClayService` owns the production Clay frame lifecycle.
- `UiInteractionRegistry` owns semantic metadata, focus, text editing, pointer hit testing, keyboard/gamepad navigation, automation, and typed actions.
- The compositor/render layer owns sprite banks, palette effects, clipping, text drawing, and custom payload dispatch.

Screens, modals, HUD, overlays, and primitives declare UI into the current frame. They must not begin/end Clay layout, set pointer state, update scroll containers, render commands, or mutate domain state from inside Clay declaration.

## Layout Principles

Root new UI work in good flexbox layout, not legacy coordinates.

Use Clay sizing, grow/fit/fixed constraints, padding, gaps, child alignment, clipping, and stable containers to express the layout. Translate legacy screenshots or UI intent into relationships and constraints, not copied x/y offsets.

Treat these as migration debt unless there is a narrow renderer or asset reason:

- Absolute coordinates and root-attached floating elements for ordinary layout.
- Sprite-offset nudges, hand-measured widths, and magic gap stacks.
- Repeated option bundles instead of named `variant` / `size` choices.
- Public primitive APIs that expose sprite banks, palette indices, or one-screen presets.

Use floating/absolute layout only for true overlays, popovers, HUD chrome, or compatibility seams that cannot yet be expressed as normal layout. Keep the reason local and remove it when the surrounding layout becomes flexible.

## Primitive API

Generic primitives live under `src/ui/primitives`; Silencer-specific composition lives under `src/client/ui`.

Target public primitives are plain nouns such as `Button`, `TextInput`, `Toggle`, `Panel`, and `Text`. Runtime/service types keep the `Ui` prefix. Text primitives expose semantic size/tone/effect intent; sprite banks, font IDs, and cell widths stay behind the text/compositor boundary.

Prefer shadcn-style API shape over raw knobs: `variant + size`, composition, and named defaults. If several call sites repeat padding, min/max width, wrapping, or effect-color values, make a named variant or size instead of normalizing the escape hatch.

Screen-specific components stay under the owning screen directory. Promote a component only after real reuse exists.

## Lifecycle

Production code should go through `ClientUi` / `ClayService`, not direct Clay lifecycle calls from screens or primitives.

The production frame order is:

1. Normalize input at the platform boundary into `UiInputState`.
2. `ClientUi::BeginFrame` resets frame arenas and starts `ClayService`.
3. Visible layers declare the complete UI tree.
4. `ClientUi::EndFrame` ends Clay layout and resolves Clay bounds into `UiInteractionRegistry`.
5. The Clay compositor renders commands.
6. The client layer dispatches typed UI actions.

For tests or adapter code that call Clay directly, match the vendored API in `clients/silencer/third_party/clay/clay.h`; this checkout uses the single-declaration `CLAY({ .id = ... })` form and `CLAY_TEXT(text, CLAY_TEXT_CONFIG(...))`.

Do not synthesize false pointer releases between frames. Clay and `UiInteractionRegistry` both depend on a continuous pointer timeline.

## Input And Actions

Normalize input once, before UI code sees it. The same coordinate space must feed layout dimensions, pointer state, registry hit testing, and compositor output.

In Silencer, prefer stable element registration plus `UiInteractionRegistry` typed actions over ad hoc `Clay_OnHover` callbacks for application behavior. If adapter/test code uses `Clay_OnHover`, use the vendored signature (`intptr_t userData`) and queue work for after layout.

UI feedback should be declared by the primitive/widget and executed by the client layer from normalized transitions. Existing `ClientUi` button/toggle audio inference is migration debt; do not extend it.

## IDs And Lifetime

Every interactive, animated, scrollable, custom-rendered, tested, or automation-visible element needs an explicit stable Clay ID. A visible label must never double as the element ID.

Use literal IDs for singleton elements, indexed IDs for repeated rows, and local/dynamic ID helpers when composing reusable components under different parents. Avoid duplicate IDs and avoid anonymous elements where transitions, focus, automation, retained rendering, or debug queries matter.

Clay does not copy arbitrary string or payload memory. Dynamic strings, text slices, image data, text `userData`, and custom payloads must live until after render command consumption. Use Silencer's per-frame primitive arenas through `UiFrameContext`; do not reset them inside screens, modals, HUD blocks, or overlays.

## Rendering Backend

Render by iterating `Clay_RenderCommandArray` and dispatching all command types the vendored Clay can emit: rectangle, border, text, image, custom, and scissor start/end.

Keep renderer-specific work behind the compositor and payload bridge. UI screens and ordinary primitives should not call SDL, `Renderer`, or `Surface` APIs directly unless they are the adapter layer.

Custom payloads are just pointers. Tag payload types, own lifetime explicitly, and keep sprite-bank details inside payloads or existing bridge primitives rather than new public primitive APIs.

## Text And Scrolling

Text measurement must match actual rendering. In Silencer, bank-text measurement and rendering are a contract with the Clay compositor; do not add layout assumptions that the renderer cannot reproduce.

Do not call `Clay_ResetMeasureTextCache` from screens or primitives. The frame backend owns that policy for this checkout.

For scrollable UI, choose one owner. If Clay owns scrolling, feed pointer state and wheel deltas before layout and use `.clip.childOffset`. If screen state owns scrolling, route scroll as typed `UiAction`s and keep row/line offsets in screen state.

## Verification

Before calling a Clay UI change correct, verify the relevant surface:

- Build through `clients/silencer/build.ps1` or `clients/silencer/build.sh`.
- Run `tests/lobby-ui/clay_ui_checks/run.sh` for the retained primitive/control checks when primitive behavior changes.
- Run `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` when ownership boundaries change.
- Use runtime screenshots or the control socket when layout, clipping, interaction, audio feedback, or visual behavior is at risk.

Compile success is not enough for UI work. A change can build cleanly while
rendering at the wrong bounds, dropping input, hiding focus, or leaking state
between screens. When behavior or layout matters, drive the real binary through
`shared/skills/cli/SKILL.md`: inspect registered widgets, click or type through
the control socket, wait for the expected state transition, and capture a
screenshot for visual confirmation.
