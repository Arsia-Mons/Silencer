# src/render

All rendering primitives and backends for the Silencer client. The game
world uses an **8-bit indexed-pixel** pipeline internally; the display
backend palettizes and upscales to native resolution. The retained cppx UI
is composited on top as a separate **premultiplied-RGBA** layer.

Windowed production renders that UI layer **on the GPU as geometry** (SIL-240):
the cppx draw IR is lowered to a vertex stream + texture manifest and drawn by
SDL_GPU, replacing the old per-frame CPU software-raster + full-window upload.
`SILENCER_GPU_UI=0` is a kill switch back to the legacy CPU-raster path; headless
(no GPU device) and TUI always use the CPU raster (it owns the parity goldens).

## Files

| File | Purpose |
|---|---|
| `surface.h/cpp` | Indexed-pixel (8-bit) bitmap. The universal in-memory canvas for the world. |
| `palette.h/cpp` | 256-entry color table + team-color remapping tables. |
| `camera.h/cpp` | Viewport: world→screen coordinate transform, scroll tracking. |
| `sprite.h/cpp` | Sprite definition: bank/index into the packed resource sheet. |
| `renderer.h/cpp` | Main world-drawing class. Blit, effects, lighting, minimap. |
| `renderdevice.h` | Abstract interface for the display backend. |
| `sdl3gpubackend.h/cpp` | SDL3 GPU backend: palettized upscale, point lights, GPU particles, panel blur, the cppx **UI geometry pass** (`SubmitUiFrame`) + legacy RGBA UI composite (`UploadUiFrame`). |
| `tuibackend.h/cpp` | Terminal backend (renders to stdout via ANSI). |
| `cppx_ui/` | The cppx UI renderer bridge: executes the SDL-free RGBA draw IR (`src/ui`) to pixels and feeds it to the backend (see below). |

## `cppx_ui/` bridge

The seam between the SDL-free retained UI runtime (`src/ui`) and SDL. It has two
lowerings of the UI's tagged-union `DrawCommandList` IR: the **CPU executor**
(software-raster to a packed RGBA buffer — the headless/test reference that owns
the parity goldens) and the **GPU emitter** (geometry + a texture manifest the
backend draws directly — the windowed production path, SIL-240). Both share the
parity-critical device-rect/UV math (`ui_draw_geometry`).

| File | Purpose |
|---|---|
| `pipeline_host.h/cpp` | Drives `client::ui::UiPipeline` for one frame. `render()` → packed RGBA (CPU path); `build_gpu_frame()` → a `GpuUiProgram` (GPU path). The CPU raster is damage-tracked (#331): the IR is diffed per-command against the previous frame (arena content, not offsets) and only the changed rects are cleared, re-executed clipped, and re-packed; `UiDamage` reports them for partial uploads. `SILENCER_UI_DAMAGE=0` forces full-surface repaints (the unchanged skip stays). |
| `ui_surface.h/cpp` | Minimal SDL surface the CPU per-frame loop draws into (clear → execute IR → present); supports full-scene SSAA. |
| `draw_executor.h/cpp` | Linear CPU executor over the RGBA IR → SDL draw calls (drawn under `SDL_BLENDMODE_BLEND_PREMULTIPLIED`). Solid fills route to span blitters when byte-safe (#331): hard quads on integer device edges → `SDL_RenderFillRect` (NONE-mode overwrite when opaque — SDL's blend-mode fill runs per-pixel arithmetic even at a=255); rounded fills → ring/fringe mesh + span-filled integer interior (`tessellate_rect_fill_ring`). `RasterConfig.span_solid_fills=false` forces the mesh (A/B harness); the parity sweep in `draw_executor_tests` pins span==mesh byte-identical. |
| `ui_draw_program.h` + `ui_draw_program_builder.h` + `ui_draw_program.cpp` | SIL-240 GPU emitter: lowers the IR to a backend-neutral `GpuUiProgram` (de-indexed clip-space verts + a draw/clip/layer command stream + a premultiplied-RGBA texture manifest). The handoff header is SDL-free. |
| `ui_draw_geometry.h/cpp` | SDL-free pure math shared by the CPU executor + GPU emitter: the legacy hairline-border / solid-fill grid snaps, canonical integer glyph cells, and image nine-slice / plain rects — so both paths emit byte-identical device rects/UVs. |
| `ui_texture_key.h` | Stable uint64 GPU-texture-cache keys (image slot / glyph face / glyph color-face), tagged so the small per-source ids can't collide. |
| `sdf_raster.h/cpp` | Signed-distance-field rasterization of rounded primitives (the SDF render mode; CPU path only — not active in production). |
| `render_mode.h` | The anti-aliasing strategy (fringe-AA vs SDF) consumed by the frame + primitive stages. |
| `sdl_ui_input.h` | The ONE shared SDL keycode/modifier → `UiKey`/`UiKeyModifier` translator. Every SDL event loop feeding the UI (game `events.cpp`, launcher) must use it — no per-app copies. |
| `font_registry.h/cpp` | Multi-face SDL_ttf registry; `font_id` → face. The only owner of SDL_ttf. |
| `text_measure.h/cpp` | SDL_ttf-backed `MeasureTextFn` installed into `ui::set_text_measurer` (measure == paint). |
| `texture_registry.h/cpp` | `texture_id` → `SDL_Texture*` map (premultiplied); also retains the source RGBA bytes per slot so the GPU emitter can upload them once. |
| `glyph_fonts.h/cpp` | Bitmap glyph-atlas faces (origin text parity). Each `Face` carries the atlas `SDL_Texture*` + the retained premultiplied atlas bytes + a stable GPU key. |
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

cppx UI (separate premultiplied-RGBA layer)
  ├─ GPU path (windowed default, SIL-240):
  │    GameUiPipeline → PipelineHost::build_gpu_frame → build_ui_draw_program → GpuUiProgram
  │    └─ RenderDevice::SubmitUiFrame(program, fade)  ← drawn as geometry into an
  │         offscreen layer, composited over the world (fade = a GPU uniform)
  └─ CPU path (headless/test reference, or SILENCER_GPU_UI=0):
       GameUiPipeline → PipelineHost::render → execute_draw_commands → packed RGBA
       └─ RenderDevice::UploadUiFrame(rgba, w, h, fade)  ← full-window upload + composite
       (headless screenshots composite this RGBA over the world in Game::CaptureCompositedFrame)
```

### GPU UI geometry path (SIL-240)

The CPU software-raster + full-window CPU→GPU upload of the UI was the SIL-237
perf regression: a changed UI frame cost a full-native-resolution raster + a
multi-MB upload, and screen fades re-ran a per-pixel CPU dim + re-upload every
frame. The GPU path eliminates all of that — `build_ui_draw_program` walks the
SAME IR the executor does and emits clip-space geometry + a texture manifest;
`SDL3GPUBackend` draws it into `ui_scene_tex` (premultiplied), and the screen
fade is a multiply at the `ui_scene_tex → swapchain` composite. Font/sprite
atlases and lazily-baked legacy sprite variants upload once and stay resident
(keyed via `ui_texture_key`, flushed on a `texture_generation` bump). Group
opacity (`LayerPush`/`LayerPop`) renders the subtree into a transient target
composited back at the layer opacity. `draw_executor.cpp` stays the parity
reference: the goldens are captured from it and the GPU output is diffed against
it (never blessed into the golden set).

Only `sdl3gpubackend.cpp` may include `<SDL3/SDL_gpu.h>`; the `GpuUiProgram`
handoff header is SDL-free.

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
device->SubmitUiFrame(program, fade);        // cppx UI as GPU geometry (SIL-240, default)
device->UploadUiFrame(rgba, w, h, fade);     // legacy: full-window premultiplied-RGBA upload
device->SupportsUiGeometry();                // true on SDL3GPUBackend; gates which path runs
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
