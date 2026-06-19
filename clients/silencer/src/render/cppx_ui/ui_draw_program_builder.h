#pragma once

// The cppx-side lowering of the DrawCommandList IR into a GpuUiProgram. Split
// from ui_draw_program.h so the data types stay SDL-free for the backend
// boundary while the builder (which needs the SDL-bound registries) lives here.

#include "ui_draw_program.h"

#include "font_registry.h"
#include "glyph_fonts.h"
#include "texture_registry.h"
#include "ui/runtime/draw_command.h"

#include <SDL3/SDL.h>

namespace silencer::cppx_ui {

// Lower one DrawCommandList IR into a GpuUiProgram. Mirrors
// execute_draw_commands op-for-op but emits geometry + a texture manifest; the
// parity-critical math stays CPU-side, identical to the software executor.
//
// `legacy_renderer` is the software SDL_Renderer the registries bake legacy
// variants against — used only for resolve_legacy baking + size queries, never
// to render. `scale` is device px per UI point; target_w/h is the geometry's
// device resolution; `texture_generation` is stamped in for cache flushing.
// Clears and refills `out`.
void build_ui_draw_program(const ::ui::DrawCommandList &list,
                           FontRegistry *fonts, TextureRegistry *textures,
                           GlyphFonts *glyphs, SDL_Renderer *legacy_renderer,
                           float scale, int target_w, int target_h,
                           uint64_t texture_generation, GpuUiProgram &out);

} // namespace silencer::cppx_ui
