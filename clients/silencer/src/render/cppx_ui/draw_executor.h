#pragma once

// Linear executor over the new tagged-union DrawCommand IR (draw_command.h).
// Consumes a DrawCommandList and emits SDL draw calls — the P3 renderer.
// Colors in the IR are premultiplied (design §8.4), so geometry is drawn under
// SDL_BLENDMODE_BLEND_PREMULTIPLIED. `fonts` may be null (text is then skipped)
// and `textures` may be null (Image commands are then skipped), which keeps
// geometry goldens font- and texture-independent.
//
// `scale` is device pixels per UI point (the window's pixel density). The IR is
// authored in points; the executor multiplies every emitted coordinate by
// `scale` and renders glyphs / feathers at the scaled resolution, so the UI
// fills the native-resolution backbuffer crisply (HiDPI). scale==1 reproduces
// the legacy point==pixel path exactly (headless goldens are unaffected).

#include "render_mode.h"
#include "ui/runtime/draw_command.h"

#include <SDL3/SDL.h>

namespace silencer::cppx_ui {

class FontRegistry;
class TextureRegistry;
class SdfMaskCache;
class GlyphFonts;

// How rounded vector primitives (fills, borders, gradients) are rasterized.
// `mode` and `sdf_cache` are coupled — the cache is only consulted when
// mode==Sdf — so they travel together as one options object rather than two
// loose positional parameters (and so the signature doesn't grow a parameter
// per future mode-specific resource). A default-constructed RasterConfig is the
// legacy path: FringeAa with no cache.
struct RasterConfig {
  // FringeAa at scale 1 is exactly the legacy per-primitive feather path, so a
  // defaulted RasterConfig leaves existing call sites and goldens unchanged.
  RenderMode mode = RenderMode::FringeAa;
  // Only used when mode==Sdf. Null is valid — masks are then generated
  // transiently per call (correct, just uncached); the golden harness uses null.
  SdfMaskCache *sdf_cache = nullptr;
  // Damage clip (device px). When set, it seeds the bottom of the clip stack —
  // every pixel write stays inside it, including layer composites and clip-pop
  // restores — and draw commands whose conservative bounds miss it are skipped.
  // Within the rect the output is byte-identical to an unclipped execute; the
  // caller owns clearing the rect first (execute never clears).
  const SDL_Rect *damage = nullptr;
};

// Execute the IR. `scale` is device px per UI point (HiDPI / SSAA upsample).
// SSAA mode emits hard-edged geometry (its AA comes from a full-scene
// supersample done by UiSurface, NOT here). Shadows, images, text, clips and
// layers are mode-independent and shared across all three modes.
void execute_draw_commands(SDL_Renderer *renderer,
                           const ::ui::DrawCommandList &list,
                           FontRegistry *fonts,
                           TextureRegistry *textures = nullptr,
                           float scale = 1.0f,
                           const RasterConfig &raster = {},
                           GlyphFonts *glyphs = nullptr);

} // namespace silencer::cppx_ui
