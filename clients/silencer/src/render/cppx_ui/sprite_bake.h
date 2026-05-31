#pragma once

// SIL-11 sprite->RGBA bake (doc §3). Flattens an 8-bit indexed sprite + the
// active 256-entry palette into PREMULTIPLIED RGBA8, the form the golden UI
// draw model speaks. The result is uploaded via TextureRegistry::upload_rgba and
// referenced by texture_id in VisualStyle.image. Renderer-side code (knows the
// indexed Surface/Palette); the SDL-free ui/ runtime never sees indices.

#include <SDL3/SDL.h>

#include <stdint.h>

namespace silencer::cppx_ui {

// Bake w*h palette indices (row-major) + a 256-entry active palette into
// `out_rgba` (>= w*h*4 bytes, tightly packed, premultiplied RGBA8).
//
// Palette index 0 is transparent — matches the world renderer's sprite blit,
// which skips pixel==0 — and bakes to (0,0,0,0). Every other index is opaque
// (alpha 255), so premultiplied == straight. Tint is applied at draw time (not
// baked); ramp/team variants call this with a remapped palette.
void bake_indexed_rgba(const uint8_t *indices, int w, int h,
                       const SDL_Color *palette256, uint8_t *out_rgba);

} // namespace silencer::cppx_ui
