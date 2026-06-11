#pragma once

#include "ui/style/visual_style.h"

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
//   bank 7  idx 5    chrome_panel     ~628x441 (SIL-91, plain native)
//   bank 7  idx 7    chrome_controls  ~628x441 (Options·Controls single-pane, stretch)
//   bank 40 idx 4    dialog_msg       ~352x178 (SIL-91, plain native)
//   bank 40 idx 2    dialog_pw        ~284x277 (SIL-91, plain native)
//   bank 6  idx 0    starfield        full-bleed (SIL-92, stretch)
//   bank 95 idx 2    hud_bezel_top    native       (in-game HUD top bezel)
//   bank 95 idx 11   hud_bezel_bottom native       (in-game HUD bottom dash)
//   bank 94 idx 0    hud_radar        native       (in-game minimap frame)
//   ------------------------------------------------------------------
//   total baked: ~32 / 64 (incl. logo reveal frames + toggles + 5 emblems)
struct ChromeTextures {
  // The green oval menu button (bank 6), per legacy size: Md/Sm/Lg. Brightness/
  // focus is a draw-time tint of the one sprite, not separate frames.
  uint32_t oval_md = 0; // idx 7  — 196x33
  uint32_t oval_sm = 0; // idx 28 — 112x33
  uint32_t oval_lg = 0; // idx 23 — 220x33
  // The legacy LIST-ROW plate (bank 6 idx 2): origin ButtonVariant::LegacyRow,
  // 236x27, used for the character-create roster + agency list rows (a flat wide
  // row plate, NOT the stadium oval). Nine-sliced to size to the pane.
  uint32_t row_plate = 0; // idx 2 — 236x27
  uint16_t row_plate_w = 0, row_plate_h = 0;
  // The metal-chrome button (bank 7 idx 24), nine-sliced. origin never swaps
  // art on focus — only brightness ramps the one frame.
  uint32_t chrome_btn_idle = 0;
  // Frame sprites rendered as PLAIN images at NATIVE sprite size (w/h carried so
  // the box can be sized exactly, keeping baked wells/borders aligned).
  uint32_t chrome_panel = 0; // bank 7 idx 5  (character_create / mission_summary)
  uint16_t chrome_panel_w = 0, chrome_panel_h = 0;
  // Single-pane Options·Controls frame (bank 7 idx 7): a baked title-header notch
  // (top-center) + right-edge scrollbar rail. STRETCHED in origin
  // (PackImageStretch(7,7)), so it is baked at its DEVICE footprint through
  // origin's two-hop chain (like the backdrops) and must be drawn 1:1: the
  // x/y/w/h carry the exact logical-point rect (absolute, screen-relative)
  // the texture was baked for. w == 0 => not baked.
  uint32_t chrome_controls = 0; // bank 7 idx 7
  float chrome_controls_x = 0, chrome_controls_y = 0;
  float chrome_controls_w = 0, chrome_controls_h = 0;
  uint32_t dialog_msg = 0; // bank 40 idx 4 (message modal)
  uint16_t dialog_msg_w = 0, dialog_msg_h = 0;
  uint32_t dialog_pw = 0; // bank 40 idx 2 (password modal + cc-alias confirm)
  uint16_t dialog_pw_w = 0, dialog_pw_h = 0;
  uint32_t dialog_connect = 0; // bank 7 idx 2 (lobby-connect dialog — frame+glow+wells baked)
  uint16_t dialog_connect_w = 0, dialog_connect_h = 0;
  // 116x24 stipple patch covering the dialog's baked 52px button wells
  // (origin lobby_connect EnsureButtonPatch); the measured-width chrome
  // buttons draw on top.
  uint32_t dialog_btn_patch = 0;
  // Full-screen starfield+planet background (bank 6 idx0), baked at DEVICE
  // resolution through origin's two-hop menu compositing (fit into the
  // virtual canvas, then magnify) so the uneven scanline striping matches the
  // golden exactly; drawn 1:1 full-bleed. `starfield` carries origin's cover
  // fit (PackImage(6,0), the menus); `starfield_stretched` carries the
  // stretch fit (PackImageStretch(6,0), Options·Controls only).
  uint32_t starfield = 0;
  uint32_t starfield_stretched = 0;
  // Lobby backdrop (bank 7 idx1): dim Mars + circuit HUD, same device-res bake
  // with origin's stretch fit (PackImageStretch(7,1)).
  uint32_t lobby_backdrop = 0;
  // Static SILENCER logo (bank 208 frame 60 — the final frame of the legacy
  // reveal animation; SIL-107 animates the full 29..60 sequence).
  uint32_t logo = 0;
  uint16_t logo_w = 0, logo_h = 0;
  // SIL-94: a few bank-208 reveal frames for the animated logo. [0] is the
  // final/full frame (== `logo`); later entries step back through the reveal so
  // the main menu ping-pongs them via use_clock. count == 0 => static `logo`.
  // SIL-94/107: bank-208 reveal frames as individual textures. [0] is the full
  // logo; later entries step back through the legacy reveal so main_menu can
  // play a reveal/hold/retract loop on the wall clock.
  static constexpr int kLogoFrames = 8;
  uint32_t logo_frame[kLogoFrames] = {};
  int logo_frame_count = 0;
  // Boolean-toggle indicator cells (bank 6), per origin boolean_setting_row:
  // LEFT cell = idx12 (on) / idx13 (off), RIGHT cell = idx15 (on) / idx14
  // (off). Four distinct sprites — never mirror one cell to fake the other.
  uint32_t toggle_l_on = 0, toggle_l_off = 0; // idx12, idx13
  uint32_t toggle_r_off = 0, toggle_r_on = 0; // idx14, idx15
  uint16_t toggle_w = 0, toggle_h = 0;
  // SIL-102: the five agency emblems (bank 181 idx0..4), shown in the Character
  // Create detail column for the previewed agency. Indexed by agency 0..4.
  // Per-agency native size + sprite-sheet offsets (origin's roster anchors
  // subtract the offsets when placing the native-size sprite).
  uint32_t agency_emblem[5] = {};
  uint16_t agency_emblem_w = 0, agency_emblem_h = 0;
  uint16_t agency_emblem_ws[5] = {}, agency_emblem_hs[5] = {};
  int16_t agency_emblem_ox[5] = {}, agency_emblem_oy[5] = {};
  // Staging roster ready checks (bank 7 idx18 ready / idx19 not-ready),
  // native size + offsets.
  uint32_t ready_on = 0, ready_off = 0;
  // Brightness-64 copies for the tech grid's non-interactable rows.
  uint32_t ready_on_dim = 0, ready_off_dim = 0;
  uint16_t ready_w = 0, ready_h = 0;
  int16_t ready_ox = 0, ready_oy = 0;
  // Advantage metadata brackets. Bank 133's '['/']' glyph art is wrong (dash-
  // shaped), so origin borrows bank 134's bracket glyphs cropped to a 4x11
  // sub-rect (character_create_layout.cpp AdvantageBracket: srcX 0 left /
  // 1 right). Baked whole-glyph; the consumer applies the sub-rect crop.
  uint32_t bracket_l = 0, bracket_r = 0; // bank 134, '['-33 / ']'-33
  // In-game HUD console chrome (banks 94/95, base palette page). Whole-sprite,
  // edge-anchored over the live world; sized to native (_w/_h carried so the
  // bezel paints at its authored width, not stretched). Radar viewport (94/0) is
  // the bottom-center minimap frame. id of 0 => fall back to a green wire-rect.
  uint32_t hud_bezel_top = 0; // bank 95 idx2  (top status bezel)
  uint16_t hud_bezel_top_w = 0, hud_bezel_top_h = 0;
  uint32_t hud_bezel_bottom = 0; // bank 95 idx11 (bottom dash bezel)
  uint16_t hud_bezel_bottom_w = 0, hud_bezel_bottom_h = 0;
  uint32_t hud_radar = 0; // bank 94 idx0  (radar/minimap frame)
  uint16_t hud_radar_w = 0, hud_radar_h = 0;
  // origin TextInput caret color = legacy palette idx 140 resolved against the
  // presenting screen's palette page (ResetPresentation): menus/cc page 1
  // (yellow), lobby cluster page 2 (sage), in-game page 0. Screens feed this
  // into their input field's style caret.
  ::ui::Color caret_menu{};
  ::ui::Color caret_lobby{};
  ::ui::Color caret_game{};
};

// Read the baked chrome ids for the current frame. Requires a
// ChromeTexturesProvider above the caller (the composition root installs it);
// returns a default (all-zero) table if absent.
ChromeTextures use_chrome();

} // namespace client::ui
