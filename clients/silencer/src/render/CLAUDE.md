# src/render

All rendering primitives and backends for the Silencer client. The game uses an
8-bit indexed-pixel pipeline internally; the display backend palettizes and
upscales to native resolution.

## Files

| File | Purpose |
|---|---|
| `surface.h/cpp` | Indexed-pixel bitmap, the universal in-memory canvas. |
| `palette.h/cpp` | 256-entry color table and team-color remapping tables. |
| `camera.h/cpp` | Viewport transform and scroll tracking. |
| `sprite.h/cpp` | Sprite definition: bank/index into packed resources. |
| `renderer.h/cpp` | Main world-drawing class. Blit, effects, lighting, minimap. |
| `retained_surface_renderer.h/cpp` | Executes retained cppx draw commands onto `Surface`. |
| `renderdevice.h` | Abstract interface for display backends. |
| `sdl3gpubackend.h/cpp` | SDL3 GPU backend: palettized upscale, point lights, GPU particles, panel blur. |
| `tuibackend.h/cpp` | Terminal backend via ANSI; lighting and particle methods are no-ops. |

## Rules

- All game-world drawing must go through `Surface` (indexed 8-bit) before it
  reaches a display backend.
- `RenderDevice` methods may only be called from the render thread (main thread
  after game tick).
- `SDL3GPUBackend` is the only file allowed to include `<SDL3/SDL_gpu.h>`.
- Retained UI composition is owned by `ClientUi`/`GameUiPipeline`; renderer
  files execute draw commands and must not own screen or HUD lifecycle.
- Legacy virtual resolution constants (`kLegacyRenderWidth = 640`,
  `kLegacyRenderHeight = 480`) live in `game_renderer.h` as `inline constexpr`;
  do not redeclare them locally.
