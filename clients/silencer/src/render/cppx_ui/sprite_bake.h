#pragma once

// sprite->RGBA bake. Flattens an 8-bit indexed sprite + the
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

// ---- Canonical-phase element bakes -------------------------
// Origin whole-frame-magnified the composed virtual frame (src = int(dx/s)),
// so a sprite's device pixels — and even its size — depended on its absolute
// screen position. The canonical bakes evaluate the same NEAREST chain at
// phase 0 (device lx from 0): ONE variant per sprite (and per virtual box
// size for the sized fits), pixel-identical at every position. The retro
// chunkiness of the 2-or-3-px duplication bands is kept; the positional
// unevenness is gone. tex_w/tex_h = ceil(vw*s) x ceil(vh*s), the chain's
// natural footprint (callers compute it). Sprite index 0 stays transparent.

// Cell fit (sprite drawn 1:1 in virtual space): magnify the sprite itself.
void bake_canonical_cell_rgba(const uint8_t *indices, int sw, int sh,
                              const SDL_Color *palette256, float s, int tex_w,
                              int tex_h, uint8_t *out_rgba);

// Stretch fit (origin DispatchImage Stretch): hop 1 stretches the sprite into
// a bw x bh virtual box (origin's int arithmetic, box-local), then the
// canonical magnify.
void bake_canonical_stretch_rgba(const uint8_t *indices, int sw, int sh,
                                 const SDL_Color *palette256, int bw, int bh,
                                 float s, int tex_w, int tex_h,
                                 uint8_t *out_rgba);

// Nine-slice fit (origin DispatchButtonNineSlice — the metal-chrome buttons):
// hop 1 composites cropped 1:1 corners + TILED (not stretched) edge/center
// bands into the bw x bh virtual box, then the canonical magnify. Caps are in
// sprite (virtual) pixels.
void bake_canonical_nineslice_rgba(const uint8_t *indices, int sw, int sh,
                                   const SDL_Color *palette256, int bw, int bh,
                                   int cap_l, int cap_r, int cap_t, int cap_b,
                                   float s, int tex_w, int tex_h,
                                   uint8_t *out_rgba);

// Contain fit (origin DispatchImage Contain — the agency emblems): hop 1
// scales by min(bw/sw, bh/sh) (float) and centers in the virtual box with
// origin's int arithmetic, then the canonical magnify.
void bake_canonical_contain_rgba(const uint8_t *indices, int sw, int sh,
                                 const SDL_Color *palette256, int bw, int bh,
                                 float s, int tex_w, int tex_h,
                                 uint8_t *out_rgba);

} // namespace silencer::cppx_ui
