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

// ---- Surface backgrounds (legacy palette: Background #000000 for menu/game
// roots, Panel #10141C for panels/overlays/bands) ----
constexpr ::ui::Color kSurfaceMenu = {0, 0, 0, 255};      // Background #000000
constexpr ::ui::Color kSurfaceGame = {0, 0, 0, 255};      // Background #000000
constexpr ::ui::Color kSurfaceOverlay = {16, 20, 28, 245}; // Panel #10141C
constexpr ::ui::Color kSurfacePanel = {16, 20, 28, 255};   // Panel #10141C
constexpr ::ui::Color kSurfaceHeroPanel = {16, 20, 28, 245}; // Panel #10141C
constexpr ::ui::Color kSurfaceHudBand = {16, 20, 28, 245};   // Panel #10141C

// ---- Borders (legacy PanelBorder #565E6F) ----
constexpr ::ui::Color kBorderPanel = {86, 94, 111, 255};
constexpr ::ui::Color kBorderHeroPanel = {86, 94, 111, 255};
constexpr ::ui::Color kBorderHudBand = {86, 94, 111, 255};

// ---- Accent / semantic action colors (AppButton variant fills) ----
// Each variant owns a base + hover (lighter) + pressed (darker) on-palette
// triple so the AppButton variant patch can supply its own interaction states
// rather than reverting to the theme's slate hover/pressed deltas.
// Legacy accent #9FC9FF. In origin/main the accent edge was a baked sprite, not
// a fill ramp; these hover/pressed/border stops are re-derived on the cool-blue
// palette as a vector interim (the oval sprite button replaces them in SIL-89).
constexpr ::ui::Color kAccent = {159, 201, 255, 255};       // #9FC9FF
constexpr ::ui::Color kAccentBorder = {191, 219, 255, 255}; // brighter accent edge
constexpr ::ui::Color kAccentHover = {191, 219, 255, 255};  // lighter on hover
constexpr ::ui::Color kAccentHoverBorder = {214, 233, 255, 255};
constexpr ::ui::Color kAccentPressed = {120, 167, 224, 255}; // darker on press
constexpr ::ui::Color kAccentPressedBorder = {159, 201, 255, 255};
constexpr ::ui::Color kDanger = {221, 80, 72, 255};         // #DD5048
constexpr ::ui::Color kDangerBorder = {240, 120, 112, 255}; // brighter danger edge
constexpr ::ui::Color kDangerHover = {236, 108, 100, 255};  // lighter on hover
constexpr ::ui::Color kDangerHoverBorder = {248, 150, 142, 255};
constexpr ::ui::Color kDangerPressed = {190, 58, 52, 255};  // darker on press
constexpr ::ui::Color kDangerPressedBorder = {220, 96, 88, 255};

// ---- Text (legacy #E0E7F1; muted/subtitle/off re-derived cool-grey) ----
constexpr ::ui::Color kTextTitle = {224, 231, 241, 255};      // #E0E7F1
constexpr ::ui::Color kTextHeroTitle = {224, 231, 241, 255};  // #E0E7F1
constexpr ::ui::Color kTextSubtitle = {150, 160, 178, 255};
constexpr ::ui::Color kTextDialogTitle = {224, 231, 241, 255}; // #E0E7F1
constexpr ::ui::Color kTextBody = {224, 231, 241, 255};        // #E0E7F1
constexpr ::ui::Color kTextBodyMuted = {176, 186, 202, 255};
constexpr ::ui::Color kTextWeaponName = {224, 231, 241, 255};  // #E0E7F1
constexpr ::ui::Color kTextWeaponDetail = {176, 186, 202, 255};
constexpr ::ui::Color kTextWeaponDetailOff = {120, 128, 142, 255};
constexpr ::ui::Color kTextHud = {224, 231, 241, 255};         // #E0E7F1

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

// ---- Native-em sizes + legacy line heights ----
// Authority: legacy text.cpp advance/lineHeight table. The OTFs are bitmap-
// derived, so rendering at the native em keeps glyphs crisp (no fractional
// scaling).
constexpr uint16_t kFontTitle = 24;  // Title face native em
constexpr float kLineTitle = 23.f;
constexpr uint16_t kFontHeading = 17; // Heading face (bank 135) native em
constexpr float kLineHeading = 19.f;  // legacy bank-135 line height
constexpr uint16_t kFontLarge = 13;  // Large face native em (headings/labels)
constexpr float kLineLarge = 15.f;
constexpr uint16_t kFontBodyEm = 11; // Body face native em
constexpr float kLineBody = 11.f;
constexpr uint16_t kFontTiny = 5;    // Tiny face native em (HUD/tiny)
constexpr float kLineTiny = 7.f;

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
                                    float border_width = kBorderWidth) {
  return ::ui::patch()
      .background(background)
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
                                    ::ui::SideWidths nine_slice = {}) {
  return image_patch(::ui::BackgroundImage{texture_id, tint, nine_slice});
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
