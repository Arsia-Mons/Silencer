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

// Bake a full-bleed menu backdrop at device resolution dw*dh by replicating
// origin/main's two-hop menu compositing chain: the sprite
// NEAREST-fits (cover, or stretch when `stretch`) into the virtual menu canvas
// int(dw/s) x int(dh/s), s = min(dw/legacy_w, dh/legacy_h), then the canvas
// NEAREST-magnifies by s, centered. A single-hop scale yields uniform pixel
// runs that visibly differ from origin's uneven two-hop striping, so both
// integer mappings are reproduced exactly. legacy_w/h = the 640x480 design
// res (kLegacyRenderWidth/Height — passed in to keep this layer game-free).
// `out_rgba` >= dw*dh*4, zeroed by caller; index 0 stays transparent.
void bake_backdrop_rgba(const uint8_t *indices, int sw, int sh,
                        const SDL_Color *palette256, bool stretch,
                        int legacy_w, int legacy_h, int dw, int dh,
                        uint8_t *out_rgba);

// Element-level variant of the same two-hop chain, for STRETCHED chrome that
// is not full-bleed (e.g. the Options-Controls frame, PackImageStretch(7,7)):
// the sprite stretches into its virtual element box bx,by,bw,bh (virtual-
// canvas px, origin DispatchImage int arithmetic), then the whole-frame
// magnify maps device px back through int(gx/s). The output texture covers
// ABSOLUTE device pixels [dev_x, dev_x+tex_w) x [dev_y, dev_y+tex_h) — a
// device-grid-aligned cover of the element's footprint chosen by the caller —
// so an element's internal dither phase depends on its virtual position
// exactly as on origin's glass. Pixels outside the element box / sprite index
// 0 stay transparent and composite over the separately-baked backdrop.
// `out_rgba` >= tex_w*tex_h*4, zeroed by caller.
void bake_element_rgba(const uint8_t *indices, int sw, int sh,
                       const SDL_Color *palette256, int bx, int by, int bw,
                       int bh, int legacy_w, int legacy_h, int dw, int dh,
                       int dev_x, int dev_y, int tex_w, int tex_h,
                       uint8_t *out_rgba);

} // namespace silencer::cppx_ui
