#pragma once

// sprite->RGBA bake. Flattens an 8-bit indexed sprite + the active palette into
// premultiplied RGBA8, the form the UI draw model speaks. Uploaded via
// TextureRegistry::upload_rgba; the SDL-free ui/ runtime never sees indices.

#include <SDL3/SDL.h>

#include <stdint.h>

namespace silencer::cppx_ui {

// Bake w*h palette indices + a 256-entry palette into `out_rgba` (>= w*h*4
// bytes, premultiplied RGBA8). Index 0 is transparent; every other index is
// opaque (premultiplied == straight). Tint is applied at draw time.
void bake_indexed_rgba(const uint8_t *indices, int w, int h,
                       const SDL_Color *palette256, uint8_t *out_rgba);

// ---- Canonical-phase element bakes -------------------------
// Origin whole-frame-magnified the virtual frame (src = int(dx/s)), so a
// sprite's device pixels (and size) depended on its absolute position. These
// bakes evaluate the same NEAREST chain at phase 0: one variant per sprite (and
// per virtual box size for sized fits), pixel-identical everywhere — the chunky
// duplication bands kept, the positional unevenness gone. tex_w/tex_h =
// ceil(vw*s) x ceil(vh*s) (callers compute it).

// Cell fit (sprite drawn 1:1 in virtual space): magnify the sprite itself.
void bake_canonical_cell_rgba(const uint8_t *indices, int sw, int sh,
                              const SDL_Color *palette256, float s, int tex_w,
                              int tex_h, uint8_t *out_rgba);

// Stretch fit: hop 1 stretches the sprite into a bw x bh virtual box (box-local
// int arithmetic), then the canonical magnify.
void bake_canonical_stretch_rgba(const uint8_t *indices, int sw, int sh,
                                 const SDL_Color *palette256, int bw, int bh,
                                 float s, int tex_w, int tex_h,
                                 uint8_t *out_rgba);

// Nine-slice fit: hop 1 composites cropped 1:1 corners + TILED (not stretched)
// edge/center bands into the bw x bh virtual box, then the canonical magnify.
// Caps are in sprite (virtual) pixels.
void bake_canonical_nineslice_rgba(const uint8_t *indices, int sw, int sh,
                                   const SDL_Color *palette256, int bw, int bh,
                                   int cap_l, int cap_r, int cap_t, int cap_b,
                                   float s, int tex_w, int tex_h,
                                   uint8_t *out_rgba);

// Contain fit: hop 1 scales by min(bw/sw, bh/sh) and int-centers in the virtual
// box, then the canonical magnify.
void bake_canonical_contain_rgba(const uint8_t *indices, int sw, int sh,
                                 const SDL_Color *palette256, int bw, int bh,
                                 float s, int tex_w, int tex_h,
                                 uint8_t *out_rgba);

} // namespace silencer::cppx_ui
