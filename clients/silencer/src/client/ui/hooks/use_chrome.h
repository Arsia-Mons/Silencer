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
//   bank 6  idx 7..11  oval_md ramp   196x33 ×5 (SIL-89 + hover phases)
//   bank 6  idx 28..32 oval_sm ramp   112x33 ×5
//   bank 6  idx 23..27 oval_lg ramp   220x33 ×5
//   bank 6  idx 2..6   row_plate ramp 236x27 ×5
//   bank 7  idx 24   chrome_btn ramp   156x21 ×5 (SIL-90, same art, brightness ramp)
//   bank 7  idx 5    chrome_panel     ~628x441 (SIL-91, plain native)
//   bank 7  idx 7    chrome_controls  ~628x441 (Options·Controls single-pane, stretch)
//   bank 40 idx 4    dialog_msg       ~352x178 (SIL-91, plain native)
//   bank 40 idx 2    dialog_pw        ~284x277 (SIL-91, plain native)
//   bank 6  idx 0    starfield        full-bleed (SIL-92, stretch)
//   bank 95 idx 2    hud_bezel_top    native       (in-game HUD top bezel)
//   bank 95 idx 11   hud_bezel_bottom native       (in-game HUD bottom dash)
//   bank 94 idx 0    hud_radar        native       (in-game minimap frame)
//   ------------------------------------------------------------------
//   total baked: ~72 / 256 (incl. full logo reveal frames + toggles + 5
//   emblems + the 16 hover-ramp phase frames)
struct ChromeTextures {
  // The green oval menu button (bank 6), per legacy size: Md/Sm/Lg, as the
  // 5-frame hover/focus ramp (origin button.cpp SpriteIndexForFrame +
  // FrameForPhase): [p] = sprite (base index + p) baked at brightness
  // 128 + p*2. [0] is the rest frame every rest-state golden shows.
  static constexpr int kOvalPhases = 5;
  uint32_t oval_md[kOvalPhases] = {}; // idx 7..11  — 196x33
  uint32_t oval_sm[kOvalPhases] = {}; // idx 28..32 — 112x33
  uint32_t oval_lg[kOvalPhases] = {}; // idx 23..27 — 220x33
  // The legacy LIST-ROW plate (bank 6 idx 2..6): origin ButtonVariant::
  // LegacyRow, 236x27, used for the character-create roster + agency list
  // rows (a flat wide row plate, NOT the stadium oval); same 5-phase ramp.
  uint32_t row_plate[kOvalPhases] = {};
  uint16_t row_plate_w = 0, row_plate_h = 0;
  // The metal-chrome button (bank 7 idx 24), nine-sliced. origin never swaps
  // art on focus — only brightness ramps the one frame.
  uint32_t chrome_btn[kOvalPhases] = {};
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
  // Full-screen starfield+planet background (bank 6 idx0), stored as the native
  // source texture. ScreenLayout chooses Cover for menus and Stretch for
  // Options·Controls so startup does not bake/upload full-screen RGBA copies.
  uint32_t starfield = 0;
  uint32_t starfield_stretched = 0;
  // Lobby backdrop (bank 7 idx1): dim Mars + circuit HUD, native source texture
  // drawn with origin's stretch fit (PackImageStretch(7,1)).
  uint32_t lobby_backdrop = 0;
  // Static SILENCER logo (bank 208 frame 60 — the final frame of the legacy
  // reveal animation; SIL-195 animates the full 29..60 sequence).
  uint32_t logo = 0;
  uint16_t logo_w = 0, logo_h = 0;
  // SIL-195: bank-208 reveal frames 29..60 as individual native-size textures.
  // `x/y` are the frame's offset inside the fixed union stage, matching
  // origin/main SilencerLogo::EnsureBounds and draw positioning. count == 0 or
  // missing stage dimensions => static `logo` fallback.
  struct LogoFrame {
    uint32_t id = 0;
    uint16_t w = 0, h = 0;
    int16_t x = 0, y = 0;
  };
  static constexpr int kLogoFirstFrame = 29;
  static constexpr int kLogoHeldFrame = 60;
  static constexpr int kLogoFrames = kLogoHeldFrame - kLogoFirstFrame + 1;
  LogoFrame logo_frame[kLogoFrames] = {};
  int logo_frame_count = 0;
  uint16_t logo_stage_w = 0, logo_stage_h = 0;
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
  // ---- In-game HUD sprites (origin ui/hud/*, base palette page 0) --------
  // A baked sprite + its native size + authored sheet offsets. Origin places
  // HUD sprites at SpriteX/Y(bank,idx, anchor) = anchor - offset; screens do
  // the same arithmetic from `x`/`y` (the offsets), never hardcoded coords.
  struct Sprite {
    uint32_t id = 0;
    uint16_t w = 0, h = 0;
    int16_t x = 0, y = 0; // sprite-sheet offsets (subtract from the anchor)
  };
  struct HudChrome {
    Sprite minimap_frame; // 94/0
    Sprite team_frame;    // 94/1 (single-team strip)
    Sprite inv_frame;     // 94/2
    Sprite health_bar;    // 95/0 (vertical fill, bottom-up)
    Sprite shield_bar;    // 95/1
    Sprite health_warn;   // 95/3
    Sprite shield_warn;   // 95/4
    Sprite fuel_mask;     // 95/5
    Sprite fuel_bar;      // 95/6 (horizontal fill)
    Sprite files_bar;     // 95/7
    Sprite fuel_low;      // 95/8
    Sprite poison;        // 97/5
    Sprite weapon_bracket;  // 96/0 (selector, +weapon*14)
    Sprite weapon_face[4];  // 96/1..4 (Blaster/Laser/Rocket/Flamer)
    Sprite weapon_glow[4];  // 96/5..8
    // Team strip (bank 103): multi-team frame cap/body, secret slots, peers.
    Sprite team_cap;        // 103/0
    Sprite team_body;       // 103/1
    Sprite secret_full;     // 103/2
    Sprite secret_empty;    // 103/3
    Sprite secret_beaming;  // 103/3 EffectColor 224
    Sprite peer_alive[4];   // 103/4..7
    Sprite peer_dead[4];    // 103/8..11
    // Inventory icons (bank 97 by Renderer::InvIdToResIndex), normal
    // (brightness 128) + unselected-dim (brightness 32) flavors.
    static constexpr int kInvIcons = 19; // bank 97 idx 0..18
    Sprite inv_icon[kInvIcons];
    Sprite inv_icon_dim[kInvIcons];
    // Buy/tech overlay (bank 102) + chat chrome strips (bank 188 idx0..8).
    Sprite buy_bg;        // 102/0
    Sprite buy_highlight; // 102/1
    Sprite chat_edge[9];  // 188/0..8 (top cap/tile/cap, ..., bottom cap/tile/cap)
    // Secret hack panel (bank 187: 0 normal / 1 beaming) + highlight flashes
    // (bank 86: 1 minimap / 2 secrets — pulse variants resolve per frame,
    // these carry dims/offsets).
    Sprite secret_bg;         // 187/0
    Sprite secret_bg_beaming; // 187/1
    Sprite highlight_minimap; // 86/1
    Sprite highlight_secrets; // 86/2
    // System camera inset frames; y anchors to bank 92's offset at the same
    // index (origin BuildHudSystemCameraFrame offsetBank=92).
    Sprite syscam_frame[2];        // 95/2 (slot 0), 95/11 (slot 1)
    int16_t syscam_oy[2] = {0, 0}; // spriteoffsety[92][2], [92][11]
  };
  HudChrome hud;
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
