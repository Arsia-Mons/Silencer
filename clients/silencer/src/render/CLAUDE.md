# src/render

All rendering primitives and backends for the Silencer client. The game
world uses an **8-bit indexed-pixel** pipeline internally; the display
backend palettizes and upscales to native resolution. The retained cppx UI
is composited on top as a separate **premultiplied-RGBA** layer.

## Files

| File | Purpose |
|---|---|
| `surface.h/cpp` | Indexed-pixel (8-bit) bitmap. The universal in-memory canvas for the world. |
| `palette.h/cpp` | 256-entry color table + team-color remapping tables. |
| `camera.h/cpp` | Viewport: world→screen coordinate transform, scroll tracking. |
| `sprite.h/cpp` | Sprite definition: bank/index into the packed resource sheet. |
| `renderer.h/cpp` | Main world-drawing class. Blit, effects, lighting, minimap. |
| `renderdevice.h` | Abstract interface for the display backend. |
| `sdl3gpubackend.h/cpp` | SDL3 GPU backend: palettized upscale, point lights, GPU particles, panel blur, RGBA UI composite. |
| `tuibackend.h/cpp` | Terminal backend (renders to stdout via ANSI). |
| `cppx_ui/` | The cppx UI renderer bridge: executes the SDL-free RGBA draw IR (`src/ui`) to pixels and feeds it to the backend (see below). |

## `cppx_ui/` bridge

The seam between the SDL-free retained UI runtime (`src/ui`) and SDL. It
executes the UI's tagged-union `DrawCommandList` IR into a window-sized
premultiplied-RGBA buffer, which the GPU backend composites over the world.

| File | Purpose |
|---|---|
| `pipeline_host.h/cpp` | Drives `client::ui::UiPipeline` for one frame and returns tightly-packed RGBA (`UiSurface` → `execute_draw_commands`). |
| `ui_surface.h/cpp` | Minimal SDL surface the per-frame loop draws into (clear → execute IR → present); supports full-scene SSAA. |
| `draw_executor.h/cpp` | Linear executor over the RGBA IR → SDL draw calls (drawn under `SDL_BLENDMODE_BLEND_PREMULTIPLIED`). |
| `sdf_raster.h/cpp` | Signed-distance-field rasterization of rounded primitives (the SDF render mode). |
| `render_mode.h` | The anti-aliasing strategy (fringe-AA vs SDF) consumed by the frame + primitive stages. |
| `font_registry.h/cpp` | Multi-face SDL_ttf registry; `font_id` → face. The only owner of SDL_ttf. |
| `text_measure.h/cpp` | SDL_ttf-backed `MeasureTextFn` installed into `ui::set_text_measurer` (measure == paint). |
| `texture_registry.h/cpp` | `texture_id` → `SDL_Texture*` map (textures stored premultiplied). |
| `sprite_bake.h/cpp` | Flattens an 8-bit indexed sprite + palette into premultiplied RGBA for upload. |
| `ui_demo.h/cpp` | Flag-gated (`SILENCER_CPPX_UI_DEMO`) end-to-end overlay smoke. |

## Pipeline

```
World tick
  └─ Renderer::Draw(surface)          ← indexed-pixel world render into Surface
       └─ all Draw*/Blit*/Effect* calls operate on Surface (8-bit)
  └─ RenderDevice::UploadFrame(pixels, w, h)  ← hand off indexed world to backend
       └─ SDL3GPUBackend: palette lookup → RGBA texture → GPU upscale
       └─ TUIBackend: quantize palette → ANSI output

cppx UI (separate RGBA layer, native window resolution)
  └─ GameUiPipeline → PipelineHost::render → execute_draw_commands → packed RGBA
  └─ RenderDevice::UploadUiFrame(rgba, w, h)  ← composited over the upscaled world
```

## Key classes

### `Surface`
Plain pixel buffer: `width`, `height`, `uint8_t* pixels`. All world-renderer
operations write into surfaces. No SDL dependency. The UI does NOT draw into
the Surface — it speaks RGBA.

### `Renderer`
Owns the world-draw loop. Key methods:
```cpp
renderer.Draw(surface, frametime);          // full world frame
renderer.DrawWorld(surface, camera, …);     // world layers only
renderer.DrawBackground / DrawForeground / DrawForegroundLuminance
renderer.DrawLight(…);                       // dynamic light cone / radial
renderer.EffectTeamColor / EffectBrightness / EffectHit / …
renderer.DrawText(surface, x, y, text, bank, width, …);  // world text only
```

### `RenderDevice` (interface)
```cpp
device->Init(window);
device->SetPalette(colors, 256);
device->UploadFrame(indexed_pixels, w, h);   // 8-bit world frame
device->UploadUiFrame(rgba, w, h);           // premultiplied-RGBA cppx UI layer
device->Present();
device->BeginLighting(); device->AddPointLight(…); device->EndLighting();
device->AllocParticleBuffer / DispatchParticleUpdate / DrawParticles;  // GPU-only
```

### `SDL3GPUBackend`
The production backend. Uses SDL3 GPU API. Implements lighting (deferred point
lights), GPU-simulated particles, lobby panel border blur, and the
premultiplied-RGBA UI composite, all as GPU passes on top of the palettized
upscale.

### `TUIBackend`
Headless / terminal backend. Implements the same `RenderDevice` interface;
lighting, particle, and UI-composite methods are no-ops.

## Rules

- All game-world drawing must go through `Surface` (indexed 8-bit) — never
  write raw RGBA into the world layer. Only the cppx UI speaks RGBA, and it
  goes through `UploadUiFrame`, not the Surface.
- `RenderDevice` methods may only be called from the render thread (main
  thread after game tick).
- `SDL3GPUBackend` is the only file allowed to include `<SDL3/SDL_gpu.h>`.
- `cppx_ui/` owns all SDL coupling for the UI (SDL_ttf, SDL_Texture). The
  `src/ui` runtime stays SDL-free; the bridge installs the text measurer and
  resolves texture/font ids.
- Legacy virtual resolution constants (`kLegacyRenderWidth = 640`,
  `kLegacyRenderHeight = 480`) live in `game_renderer.h` as `inline constexpr`
  — do not redeclare them locally.
