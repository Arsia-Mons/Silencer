#pragma once

// Shared client-UI design tokens (the shadcn "design tokens" analogue for this
// retained UI). One palette + the three generic visual builders that app-authored
// surfaces and text paint resolve through. This is the single source of app paint,
// replacing the former per-screen `silencer::theme` namespace.
//
// Tokens-only by design: no component exports, no recipe layer. Each semantic
// component owns its variant->token switch locally. Colors are STRAIGHT alpha;
// the IR premultiplies at emit. Header-only (constexpr + inline), not transpiled.

#include "ui/components/common.h"
#include "ui/style/style_patch.h"

#include <cstdint>

namespace silencer::tokens {

// ---- Surface backgrounds (green-phosphor: black menu/game roots, near-black
// faint-green panels so the green chrome reads as glass over the starfield) ----
constexpr ::ui::Color kSurfaceMenu = {0, 0, 0, 255};       // #000000
constexpr ::ui::Color kSurfaceGame = {0, 0, 0, 255};       // #000000
constexpr ::ui::Color kSurfaceOverlay = {6, 16, 8, 238};   // near-black green glass
constexpr ::ui::Color kSurfacePanel = {6, 16, 8, 235};     // near-black green glass
constexpr ::ui::Color kSurfaceHeroPanel = {6, 16, 8, 238};
constexpr ::ui::Color kSurfaceHudBand = {4, 12, 6, 238};

// ---- Borders (green-phosphor: green-dim #2E7D45 idle frame stroke) ----
constexpr ::ui::Color kBorderPanel = {46, 125, 69, 255};   // #2E7D45 green-dim
constexpr ::ui::Color kBorderHeroPanel = {46, 139, 46, 255}; // #2E8B2E chrome-border
constexpr ::ui::Color kBorderHudBand = {46, 125, 69, 255};

// ---- Accent / semantic action colors (AppButton variant fills) ----
// Each variant owns a base + hover (lighter) + pressed (darker) on-palette
// triple so the AppButton variant patch can supply its own interaction states
// rather than reverting to the theme's slate hover/pressed deltas.
// Legacy accent #9FC9FF. In origin/main the accent edge was a baked sprite, not
// a fill ramp; these hover/pressed/border stops are re-derived on the cool-blue
// palette as a vector interim (the oval sprite button replaces them in SIL-89).
// Green-phosphor accent (focus ring + selection wash). origin/main's accent edge
// was a baked sprite, not a fill ramp; these stops are the green-phosphor family.
constexpr ::ui::Color kAccent = {92, 208, 92, 255};         // #5CD05C green-bright
constexpr ::ui::Color kAccentBorder = {60, 255, 60, 255};   // #3CFF3C focus stroke
constexpr ::ui::Color kAccentHover = {136, 232, 136, 255};  // lighter on hover
constexpr ::ui::Color kAccentHoverBorder = {60, 255, 60, 255};
constexpr ::ui::Color kAccentPressed = {79, 184, 103, 255}; // darker on press
constexpr ::ui::Color kAccentPressedBorder = {92, 208, 92, 255};
constexpr ::ui::Color kDanger = {221, 80, 72, 255};         // #DD5048
constexpr ::ui::Color kDangerBorder = {240, 120, 112, 255}; // brighter danger edge
constexpr ::ui::Color kDangerHover = {236, 108, 100, 255};  // lighter on hover
constexpr ::ui::Color kDangerHoverBorder = {248, 150, 142, 255};
constexpr ::ui::Color kDangerPressed = {190, 58, 52, 255};  // darker on press
constexpr ::ui::Color kDangerPressedBorder = {220, 96, 88, 255};

// ---- Text (green-phosphor family — measured from the v00058 goldens) ----
// The dominant UI text green is (24,124,20) with a darker AA ramp; it appears for
// titles, button labels, body, and log lines. Description prose runs lighter and
// desaturated (84,156,104). The glyph atlas is a white coverage mask, so these
// token colors ARE the rendered text color (tinted at draw time).
constexpr ::ui::Color kTextTitle = {24, 124, 20, 255};        // standard green
constexpr ::ui::Color kTextHeroTitle = {48, 168, 44, 255};    // brand/hero (brighter)
constexpr ::ui::Color kTextSubtitle = {84, 156, 104, 255};    // description prose (lighter)
constexpr ::ui::Color kTextDialogTitle = {24, 124, 20, 255};
constexpr ::ui::Color kTextBody = {24, 124, 20, 255};
constexpr ::ui::Color kTextBodyMuted = {16, 96, 8, 255};      // dim (footer/OR/inactive)
constexpr ::ui::Color kTextWeaponName = {24, 124, 20, 255};
constexpr ::ui::Color kTextWeaponDetail = {24, 124, 20, 255};
constexpr ::ui::Color kTextWeaponDetailOff = {10, 72, 8, 255}; // disabled
constexpr ::ui::Color kTextHud = {61, 232, 61, 255};          // #3DE83D hud-green

// origin/main UI text is multi-color, not uniform green (measured from v00058).
constexpr ::ui::Color kTextBrand = {152, 28, 28, 255};     // "Silencer" wordmark (red)
constexpr ::ui::Color kTextVersion = {140, 64, 8, 255};    // build version (amber)
constexpr ::ui::Color kTextAgentName = {40, 96, 200, 255}; // agent names (cornflower blue)
constexpr ::ui::Color kTextProse = {198, 198, 198, 255};   // white prose (agency detail/description)

// ---- In-game HUD LCD palette (overlay over live world; spec §1.1) ----
// SEPARATE from the menu green family above: these read against the live world,
// not the starfield, so they run brighter/saturated. own-data=green,
// economy=blue, warnings=red, radar schematic=amber, lozenge/scoreboard=black.
constexpr ::ui::Color kHudGreen = {61, 232, 61, 255};    // #3DE83D own data/chat/scoreboard
constexpr ::ui::Color kHudGreenDim = {30, 122, 30, 255}; // #1E7A1E chat body / inactive
constexpr ::ui::Color kHudBlue = {58, 107, 255, 255};    // #3A6BFF economy/files/credits/dots
constexpr ::ui::Color kHudRed = {224, 48, 48, 255};      // #E03030 FATIGUE/health/2ndary ammo
constexpr ::ui::Color kHudAmber = {160, 86, 30, 255};    // #A0561E radar schematic
constexpr ::ui::Color kHudBlack = {0, 0, 0, 210};        // ~85% scoreboard bar / radar viewport
constexpr ::ui::Color kHudPanelFill = {0, 8, 2, 200};    // translucent green-black HUD well
constexpr ::ui::Color kBlipAlly = {255, 255, 255, 255};
constexpr ::ui::Color kBlipEnemy = {224, 48, 48, 255};

// ---- Font faces (font_id; see render/cppx_ui/font_registry.h FaceId) ----
// The four bitmap-derived legacy OTF faces. The product layer sets font_id per
// role so titles/headings/body/tiny each render in their own face (everything
// previously collapsed to Body face 0 at arbitrary point sizes).
constexpr uint16_t kFaceBody = 0;    // silencer-ui      (bank 133)
constexpr uint16_t kFaceLarge = 1;   // silencer-ui-large(bank 134) — headings
constexpr uint16_t kFaceTitle = 2;   // silencer-title   (bank 136) — titles
constexpr uint16_t kFaceTiny = 3;    // silencer-tiny    (bank 132) — HUD/tiny
constexpr uint16_t kFaceHeading = 4; // silencer-135     (bank 135) — the dominant
                                     // legacy title/heading face (SIL-95)

// ---- Glyph cell sizes (device px) + line heights ----
// Text renders from the legacy bitmap glyph banks (origin/main parity), NOT TTF.
// font_size here is the target DEVICE cell height. The golden is origin's
// 640-space bank glyphs (line heights 23/19/15/11/7) upscaled ~1.5x to the 960
// window, so each token = native bank lineHeight * 1.5 (rounded). The glyph
// executor scales the native art by font_size/lineHeight (nearest-neighbor),
// reproducing the chunky upscaled-bitmap look. kLine* match the cell height.
constexpr uint16_t kFontTitle = 34;  // bank 136 hero/big prompt (23 * 1.5)
constexpr float kLineTitle = 34.f;
constexpr uint16_t kFontHeading = 28; // bank 135 titles + button labels (19 * 1.5)
constexpr float kLineHeading = 28.f;
constexpr uint16_t kFontLarge = 22;  // bank 134 sub-headings (15 * 1.5)
constexpr float kLineLarge = 22.f;
constexpr uint16_t kFontBodyEm = 16; // bank 133 body/log/detail (11 * 1.5)
constexpr float kLineBody = 16.f;
constexpr uint16_t kFontTiny = 10;   // bank 132 tiny (7 * 1.5)
constexpr float kLineTiny = 10.f;

// ---- Border widths ----
constexpr float kBorderWidth = 1.0f;
constexpr float kBorderWidthSelected = 2.0f;

// ---- Patch builders (sparse overrides; verbatim semantics from the former
// per-screen theme, now emitting StylePatch instead of dense VisualStyle) ----

// Solid-fill surface (no border).
inline ::ui::StylePatch fill_patch(::ui::Color background) {
  return ::ui::patch().background(background);
}

// Solid-fill surface with a uniform border (width feeds layout via
// LayoutStyle.border_width; the color is carried here on all four sides).
inline ::ui::StylePatch panel_patch(::ui::Color background, ::ui::Color border,
                                    float border_width = kBorderWidth,
                                    float corner_radius = 0.0f) {
  return ::ui::patch()
      .background(background)
      .corner_radius(corner_radius)
      .border(::ui::Border{
          {border_width, border_width, border_width, border_width},
          {border, border, border, border}});
}

// Sprite-backed surface paint (SIL-88). Emits a baked legacy sprite (texture_id
// from use_chrome()) and EXPLICITLY clears the fill/gradient/border/rounding so
// the role's opaque control paint can't slab behind it: the sprite's index-0
// transparent corners must reveal the background, not a rectangle. EVERY
// sprite-backed variant (oval/chrome button, sprite panel, dialog frame) MUST
// paint through this helper — never through the gradient-painting solid() path.
// A texture_id of 0 yields a fully transparent patch (screens tolerate the
// not-yet-baked frame without a flash).
inline ::ui::StylePatch image_patch(::ui::BackgroundImage image) {
  return ::ui::patch()
      .image(image)
      .background(::ui::Color{0, 0, 0, 0}) // no opaque fill under the sprite
      .gradient(::ui::Gradient{})          // defeat the role's control gradient
      .border(::ui::Border{})              // sprite carries its own edge
      .outline(::ui::Outline{})            // no vector focus ring; focus = sprite brightness
      .corner_radius(0.f);                 // the sprite shape is authored, not rounded
}
inline ::ui::StylePatch image_patch(uint32_t texture_id,
                                    ::ui::Color tint = {255, 255, 255, 255},
                                    ::ui::SideWidths nine_slice = {},
                                    bool flip_h = false) {
  ::ui::BackgroundImage img{texture_id, tint, nine_slice};
  img.flip_h = flip_h;
  return image_patch(img);
}

// Atlas / partial-fill variant: samples only the given source sub-rect (texture
// pixels) of the texture — for packed-frame atlases and drained HUD bars.
// w==0 || h==0 falls back to the whole texture (SIL-93).
inline ::ui::StylePatch image_patch_sub(uint32_t texture_id, float src_x,
                                        float src_y, float src_w, float src_h,
                                        ::ui::Color tint = {255, 255, 255,
                                                            255}) {
  return image_patch(::ui::BackgroundImage{texture_id, tint, {}, src_x, src_y,
                                           src_w, src_h});
}

// Text paint (color + face + native-em size + legacy line height). font_id
// selects the OTF face; line_height 0 falls back to the face's natural skip.
inline ::ui::StylePatch text_patch(::ui::Color color, uint16_t font_size,
                                   uint16_t font_id = kFaceBody,
                                   float line_height = 0.f) {
  return ::ui::patch().text(::ui::TextVisual{.color = color,
                                             .font_id = font_id,
                                             .font_size = font_size,
                                             .line_height = line_height});
}

} // namespace silencer::tokens
