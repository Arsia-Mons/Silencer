# src/render

All rendering primitives and backends for the Silencer client. The game uses an **8-bit indexed-pixel** pipeline internally; the display backend palettizes and upscales to native resolution.

## Files

| File | Purpose |
|---|---|
| `surface.h/cpp` | Indexed-pixel (8-bit) bitmap. The universal in-memory canvas. |
| `palette.h/cpp` | 256-entry color table + team-color remapping tables. |
| `camera.h/cpp` | Viewport: world→screen coordinate transform, scroll tracking. |
| `sprite.h/cpp` | Sprite definition: bank/index into the packed resource sheet. |
| `renderer.h/cpp` | Main world-drawing class. Blit, effects, lighting, minimap. |
| `renderdevice.h` | Abstract interface for the display backend. |
| `sdl3gpubackend.h/cpp` | SDL3 GPU backend: palettized upscale, point lights, GPU particles, panel blur. |
| `tuibackend.h/cpp` | Terminal backend (renders to stdout via ANSI). |
| `clay_ui_compositor.h/cpp` | Bridges the Clay layout engine to `Surface` for in-game UI. |
| `clay_ui_payloads.h` | Custom Clay element payload types (button, scroll box, text input, toggle). |
| `clay_ui_tests/` | Visual regression tests for individual Clay UI widgets. |

## Pipeline

```
World tick
  └─ Renderer::Draw(surface)          ← indexed-pixel world render into Surface
       └─ all Draw*/Blit*/Effect* calls operate on Surface (8-bit)
  └─ ClayUICompositor::Compose(surface) ← overlays Clay UI on top
  └─ RenderDevice::UploadFrame(pixels, w, h)  ← hand off to backend
       └─ SDL3GPUBackend: palette lookup → RGBA texture → GPU upscale → present
       └─ TUIBackend: quantize palette → ANSI output
```

## Key classes

### `Surface`
Plain pixel buffer: `width`, `height`, `uint8_t* pixels`. All renderer operations write into surfaces. No SDL dependency.

### `Renderer`
Owns the world-draw loop. Key methods:
```cpp
renderer.Draw(surface, frametime);          // full frame (calls DrawWorld + UI)
renderer.DrawWorld(surface, camera, …);    // world layers only
renderer.DrawBackground / DrawForeground / DrawForegroundLuminance
renderer.DrawLight(…);                     // dynamic light cone / radial
renderer.EffectTeamColor / EffectBrightness / EffectHit / …
renderer.DrawText(surface, x, y, text, bank, width, …);
```

### `RenderDevice` (interface)
```cpp
device->Init(window);
device->SetPalette(colors, 256);
device->UploadFrame(indexed_pixels, w, h);
device->Present();
device->BeginLighting(); device->AddPointLight(…); device->EndLighting();
device->AllocParticleBuffer / DispatchParticleUpdate / DrawParticles;  // GPU-only
```

### `SDL3GPUBackend`
The production backend. Uses SDL3 GPU API. Implements lighting (deferred point lights), GPU-simulated particles, and lobby panel border blur as GPU passes on top of the palettized upscale.

### `TUIBackend`
Headless / terminal backend. Implements the same `RenderDevice` interface; lighting and particle methods are no-ops.

### `ClayUICompositor`
Calls Clay layout, then walks the render command list and dispatches to `Surface`-based drawing helpers. Custom payload types defined in `clay_ui_payloads.h` (button, scroll list, text input, toggle).

## Rules

- All game-world drawing must go through `Surface` (indexed 8-bit) — never write raw RGBA into the game layer.
- `RenderDevice` methods may only be called from the render thread (main thread after game tick).
- `SDL3GPUBackend` is the only file allowed to include `<SDL3/SDL_gpu.h>`.
- The Clay UI test binaries in `clay_ui_tests/` are standalone executables; do not link them into the game binary.
- Legacy virtual resolution constants (`kLegacyRenderWidth = 640`, `kLegacyRenderHeight = 480`) live in `game_renderer.h` as `inline constexpr` — do not redeclare them locally.
