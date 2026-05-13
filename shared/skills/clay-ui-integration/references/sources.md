# Clay References

Use these sources for implementation details. Prefer official repository files over third-party summaries when they disagree.

## Official Sources

- Clay README: https://github.com/nicbarker/clay/blob/main/README.md  
  Use for the public lifecycle, conceptual model, IDs, pointer interaction, scrolling, custom elements, retained rendering notes, debug tools, multiple contexts, and API overview.

- `clay.h`: https://github.com/nicbarker/clay/blob/main/clay.h  
  Use as the final source of truth for public structs, enums, function signatures, render command types, pointer states, scroll data, compile-time options, and error types.

- SDL3 simple demo: https://github.com/nicbarker/clay/tree/main/examples/SDL3-simple-demo  
  Use for a compact end-to-end integration: SDL setup, text measurement through SDL_ttf, Clay initialization, frame update, layout declaration, and command rendering.

- SDL3 renderer: https://github.com/nicbarker/clay/blob/main/renderers/SDL3/clay_renderer_SDL3.c  
  Use for an SDL renderer dispatch example covering rectangles, text, borders, scissor clipping, and images.

- Raylib sidebar scrolling example: https://github.com/nicbarker/clay/tree/main/examples/raylib-sidebar-scrolling-container  
  Use for pointer callbacks, scroll containers, custom scrollbar data, and a complete frame loop in a different renderer.

- Raylib renderer: https://github.com/nicbarker/clay/blob/main/renderers/raylib/clay_renderer_raylib.c  
  Use when your renderer needs null-terminated strings, glyph-based measurement, scissor mapping, image rendering through `imageData`, or `CUSTOM` command handling examples.

- Official website WASM example: https://github.com/nicbarker/clay/blob/main/examples/clay-official-website/main.c  
  Use for a strong browser/WASM lifecycle: exported frame update receives window size, pointer, wheel, touch, key, and delta time; then updates layout dimensions, pointer state, scroll containers, custom scrollbars, debug mode, and returns render commands.

- Canvas2D web renderer: https://github.com/nicbarker/clay/blob/main/renderers/web/canvas2d/clay-canvas2d-renderer.html  
  Use for command decoding from WASM memory, Canvas text measurement, font loading before init, image caching, and `save`/`clip`/`restore` scissor handling.

- HTML web renderer: https://github.com/nicbarker/clay/blob/main/renderers/web/html/clay-html-renderer.html  
  Use for retained-mode rendering. It maps stable Clay command IDs to cached DOM elements and diffs command/config data to update persistent objects.

- Renderer directory: https://github.com/nicbarker/clay/tree/main/renderers  
  Use to compare backend patterns across SDL, Raylib, Sokol, web, terminal, Cairo, Win32 GDI, Playdate, and other renderers.

- Examples directory: https://github.com/nicbarker/clay/tree/main/examples  
  Use to find working patterns for transitions, scrolling, video/custom rendering, WebAssembly, PDF, Sokol, SDL, and Raylib integrations.

## Useful Third-Party Indexes

- DeepWiki renderer overview: https://deepwiki.com/nicbarker/clay/5-renderers  
  Use for quick navigation across renderer files and the common render-command dispatch pattern. Verify details against the official GitHub files.

- DeepWiki input handling: https://deepwiki.com/nicbarker/clay/4.4-input-handling  
  Use as a quick explanation of pointer state, `Clay_OnHover`, `Clay_UpdateScrollContainers`, and `Clay_GetScrollContainerData`. Verify details against README and `clay.h`.

- DeepWiki API reference: https://deepwiki.com/nicbarker/clay/4-api-reference  
  Use for quick lookup and source pointers. Verify signatures against `clay.h`.

## Implementation Lessons

- Clay does not own the event loop. Your app collects input and calls Clay APIs in the correct order.
- Clay does not own rendering. Your renderer consumes `Clay_RenderCommandArray`.
- Text measurement is a contract between Clay and your renderer. Wrong measurement creates wrong layout.
- Pointer state must be continuous. Do not synthesize false releases between frames.
- Scroll updates require pointer state, scroll delta, and delta time before layout.
- Stable IDs matter for interaction, queries, transitions, debug tooling, retained rendering, and automation.
- Custom render data is just a pointer. Own its lifetime explicitly.
- Debug tools are render commands; if debug UI is broken, the renderer is often incomplete.
