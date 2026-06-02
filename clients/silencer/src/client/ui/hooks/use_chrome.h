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
//   bank 6  idx 7    oval_md          196x33   (SIL-89)
//   bank 6  idx 28   oval_sm          112x33   (SIL-89)
//   bank 6  idx 23   oval_lg          220x33   (SIL-89)
//   bank 7  idx 24   chrome_btn_idle  156x21   (SIL-90, nine-slice {l12 r12 t4 b4})
//   bank 7  idx 28   chrome_btn_focus 156x21   (SIL-90, phase-4 frame)
//   bank 7  idx 5    chrome_panel     ~628x441 (SIL-91, plain native)
//   bank 40 idx 4    dialog_msg       ~352x178 (SIL-91, plain native)
//   bank 40 idx 2    dialog_pw        ~284x277 (SIL-91, plain native)
//   bank 6  idx 0    starfield        full-bleed (SIL-92, stretch)
//   ------------------------------------------------------------------
//   total baked: 9 / 64
struct ChromeTextures {
  // The green oval menu button (bank 6), per legacy size: Md/Sm/Lg. Brightness/
  // focus is a draw-time tint of the one sprite, not separate frames.
  uint32_t oval_md = 0; // idx 7  — 196x33
  uint32_t oval_sm = 0; // idx 28 — 112x33
  uint32_t oval_lg = 0; // idx 23 — 220x33
  // The metal-chrome button (bank 7), nine-sliced. 2-state = two authored frames
  // (idx24 phase 0 idle / idx28 phase 4 focused).
  uint32_t chrome_btn_idle = 0;  // idx 24
  uint32_t chrome_btn_focus = 0; // idx 28
  // Frame sprites rendered as PLAIN images at NATIVE sprite size (w/h carried so
  // the box can be sized exactly, keeping baked wells/borders aligned).
  uint32_t chrome_panel = 0; // bank 7 idx 5  (character_create / mission_summary)
  uint16_t chrome_panel_w = 0, chrome_panel_h = 0;
  uint32_t dialog_msg = 0; // bank 40 idx 4 (message modal)
  uint16_t dialog_msg_w = 0, dialog_msg_h = 0;
  uint32_t dialog_pw = 0; // bank 40 idx 2 (password / lobby-connect modal)
  uint16_t dialog_pw_w = 0, dialog_pw_h = 0;
  // Full-screen starfield+planet background (bank 6 idx0), stretched to the
  // screen root (no aspect-cover in v1 — legacy stretched too).
  uint32_t starfield = 0;
  // Static SILENCER logo (bank 208 frame 60 — the final frame of the legacy
  // reveal animation; SIL-107 animates the 29..60 sequence).
  uint32_t logo = 0;
  uint16_t logo_w = 0, logo_h = 0;
};

// Read the baked chrome ids for the current frame. Requires a
// ChromeTexturesProvider above the caller (the composition root installs it);
// returns a default (all-zero) table if absent.
ChromeTextures use_chrome();

} // namespace client::ui
