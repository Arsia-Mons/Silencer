#pragma once

#include <cstdint>

namespace client::ui {

// Opaque legacy-sprite chrome surfaced to .cppx screens (SIL-87). The renderer
// bridge bakes curated indexed sprites + the active palette into premultiplied
// RGBA textures and hands their opaque `texture_id`s here, mirroring how
// `font_id` surfaces a baked face. Screens set `VisualStyle.image{texture_id}`
// (via the patch().image(...) authoring path) to draw a sprite through the
// existing Image draw-IR arm — they NEVER see SDL, Surface, or the Palette.
//
// An id of 0 means "not baked yet" (e.g. the one frame before the first bake,
// or if the spritebank entry is missing): screens MUST tolerate it without a
// crash or flash (fall back to the vector look for that frame).
//
// ---- Texture budget (single owning artifact; cap = 64) ------------------
// Running tally of every baked chrome index across all surfaces. Keep this in
// sync as surfaces add sprites; atlasing (source-rect UV) is the relief valve.
//   bank 6  idx 7   oval_button   196x33   (SIL-87 seed / SIL-89)
//   ------------------------------------------------------------------
//   total baked: 1 / 64
struct ChromeTextures {
  // bank 6 idx 7 — the green oval menu button (196x33). Brightness phases are a
  // draw-time tint of this one sprite, not separate frames.
  uint32_t oval_button = 0;
  uint16_t oval_button_w = 0;
  uint16_t oval_button_h = 0;
};

// Read the baked chrome ids for the current frame. Requires a
// ChromeTexturesProvider above the caller (the composition root installs it);
// returns a default (all-zero) table if absent.
ChromeTextures use_chrome();

} // namespace client::ui
