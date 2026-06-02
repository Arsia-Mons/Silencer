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

// ---- Font sizes ----
constexpr uint16_t kFontHeroTitle = 30;
constexpr uint16_t kFontScreenTitle = 26;
constexpr uint16_t kFontDialogTitle = 28;
constexpr uint16_t kFontPopupTitle = 22;
constexpr uint16_t kFontSubtitle = 16;
constexpr uint16_t kFontHud = 18;
constexpr uint16_t kFontStrong = 16;
constexpr uint16_t kFontMessage = 15;
constexpr uint16_t kFontBody = 14;
constexpr uint16_t kFontDetail = 12;

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

// Text paint (color + size). align/wrap/line_height stay defaults.
inline ::ui::StylePatch text_patch(::ui::Color color, uint16_t font_size) {
  return ::ui::patch().text(::ui::TextVisual{.color = color, .font_size = font_size});
}

} // namespace silencer::tokens
