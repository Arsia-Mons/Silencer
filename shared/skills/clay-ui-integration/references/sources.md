# Silencer Clay References

Use local Silencer sources first. Upstream Clay files are useful for research, but this checkout can lag or differ from upstream `main`.

## Local Sources

- `clients/silencer/third_party/clay/clay.h`
  Final source of truth for the Clay API available to Silencer. Check this before copying upstream examples. This checkout currently uses the single-declaration `CLAY({ .id = ... })` form, `CLAY_TEXT(text, CLAY_TEXT_CONFIG(...))`, and `Clay_OnHover(..., intptr_t userData)`.

- `clients/silencer/src/ui/CLAUDE.md`
  Generic UI runtime/primitives boundaries, primitive API direction, Clay discipline, and verification expectations.

- `clients/silencer/src/client/ui/CLAUDE.md`
  Client UI ownership: `ClientUi`, screens, modals, HUD, navigation, input, action dispatch, and feedback policy.

- `clients/silencer/src/ui/runtime/ClayService.h` / `.cpp`
  Production Clay frame lifecycle and where pointer state, scroll updates, layout begin/end, and bounds resolution happen.

- `clients/silencer/src/client/ui/ClientUi.h` / `.cpp`
  Production UI frame owner, frame arena reset point, screen stack bridge, input dispatch, and current feedback debt.

- `clients/silencer/src/render/clay_ui_compositor.*` and `clients/silencer/src/render/clay_ui_payloads.h`
  Renderer command dispatch, text measurement, sprite/palette payloads, image data packing, custom command handling, clipping, and render tests.

- `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh`
  Executable boundary checks for legacy UI paths, raw event consumption, lifecycle ownership, renderer ownership, and deprecated action patterns.

## Upstream Sources

- Clay README: https://github.com/nicbarker/clay/blob/main/README.md
  Use for the conceptual model, lifecycle overview, IDs, pointer interaction, scrolling, custom elements, debug tools, multiple contexts, and API overview. Verify signatures against the vendored Silencer header.

- Upstream `clay.h`: https://github.com/nicbarker/clay/blob/main/clay.h
  Use to understand API direction and drift. Do not treat upstream `main` as authoritative for Silencer unless the vendored header has been updated.

- SDL3 simple demo: https://github.com/nicbarker/clay/tree/main/examples/SDL3-simple-demo
  Use for an end-to-end SDL integration pattern: setup, text measurement, initialization, frame update, layout declaration, and command rendering.

- SDL3 renderer: https://github.com/nicbarker/clay/blob/main/renderers/SDL3/clay_renderer_SDL3.c
  Use for SDL renderer dispatch examples covering rectangles, text, borders, scissor clipping, and images. Silencer's compositor may intentionally differ for bank sprites and palette effects.

- Raylib sidebar scrolling example: https://github.com/nicbarker/clay/tree/main/examples/raylib-sidebar-scrolling-container
  Use for pointer callbacks, scroll containers, custom scrollbar data, and a complete frame loop in a different renderer.

- Renderer directory: https://github.com/nicbarker/clay/tree/main/renderers
  Use to compare backend patterns across SDL, Raylib, Sokol, web, terminal, Cairo, Win32 GDI, Playdate, and other renderers.

## Implementation Lessons

- Clay does not own the event loop. Silencer normalizes input into `UiInputState`.
- Clay does not own screen navigation. `ClientUi` and `ScreenStack` do.
- Clay does not own rendering. The compositor consumes `Clay_RenderCommandArray`.
- Text measurement is a contract between Clay and the compositor. Wrong measurement creates wrong layout.
- Pointer state must be continuous. Do not synthesize false releases between frames.
- Good Silencer UI layout is flexbox-style constraints, not copied legacy coordinates.
- Stable IDs matter for interaction, bounds queries, focus, transitions, tests, debug tooling, retained rendering, and automation.
- Custom render data is just a pointer. Own its lifetime explicitly.
- Debug tools are render commands; if debug UI is broken, the renderer is often incomplete.
