#pragma once

// ui_draw_program_builder.h — the cppx-side lowering of the DrawCommandList IR
// into a backend-neutral GpuUiProgram. Split from ui_draw_program.h
// so the data types stay SDL-free for the backend boundary while the builder
// (which needs the SDL-bound registries) lives on the cppx side.

#include "ui_draw_program.h"

#include "font_registry.h"
#include "glyph_fonts.h"
#include "texture_registry.h"
#include "ui/runtime/draw_command.h"

#include <SDL3/SDL.h>

namespace silencer::cppx_ui {

// Lower one cppx DrawCommandList IR into a backend-neutral GPU draw program.
// Mirrors execute_draw_commands op-for-op but emits geometry + a texture
// manifest instead of issuing SDL draws — the parity-critical geometry/UV/snap
// math stays CPU-side here, identical to the software executor.
//
// `legacy_renderer` is the software SDL_Renderer the registries bake legacy
// sprite variants against (the same r_ the CPU path uses); used only for
// resolve_legacy baking + size queries, never to render. `scale` is device px
// per UI point; target_w/h is the device resolution the geometry lands in.
// `texture_generation` is stamped into the program so the backend can flush its
// GPU texture cache when the registries reset. Clears and refills `out`.
void build_ui_draw_program(const ::ui::DrawCommandList &list,
                           FontRegistry *fonts, TextureRegistry *textures,
                           GlyphFonts *glyphs, SDL_Renderer *legacy_renderer,
                           float scale, int target_w, int target_h,
                           uint64_t texture_generation, GpuUiProgram &out);

} // namespace silencer::cppx_ui
