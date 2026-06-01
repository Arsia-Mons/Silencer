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

// ---- Surface backgrounds ----
constexpr ::ui::Color kSurfaceMenu = {8, 14, 18, 255};
constexpr ::ui::Color kSurfaceGame = {10, 16, 18, 255};
constexpr ::ui::Color kSurfaceOverlay = {14, 22, 28, 245};
constexpr ::ui::Color kSurfacePanel = {17, 24, 30, 255};
constexpr ::ui::Color kSurfaceHeroPanel = {18, 27, 32, 245};
constexpr ::ui::Color kSurfaceHudBand = {18, 24, 28, 245};

// ---- Borders ----
constexpr ::ui::Color kBorderPanel = {78, 96, 108, 255};
constexpr ::ui::Color kBorderHeroPanel = {83, 108, 118, 255};
constexpr ::ui::Color kBorderHudBand = {82, 106, 118, 255};

// ---- Accent / semantic action colors (AppButton variant fills) ----
// Each variant owns a base + hover (lighter) + pressed (darker) on-palette
// triple so the AppButton variant patch can supply its own interaction states
// rather than reverting to the theme's slate hover/pressed deltas.
constexpr ::ui::Color kAccent = {96, 165, 250, 255};        // slate accent blue
constexpr ::ui::Color kAccentBorder = {130, 188, 255, 255}; // brighter accent edge
constexpr ::ui::Color kAccentHover = {124, 184, 255, 255};  // lighter on hover
constexpr ::ui::Color kAccentHoverBorder = {158, 206, 255, 255};
constexpr ::ui::Color kAccentPressed = {72, 138, 222, 255}; // darker on press
constexpr ::ui::Color kAccentPressedBorder = {110, 168, 235, 255};
constexpr ::ui::Color kDanger = {220, 60, 60, 255};         // destructive red
constexpr ::ui::Color kDangerBorder = {240, 110, 110, 255}; // brighter danger edge
constexpr ::ui::Color kDangerHover = {236, 84, 84, 255};    // lighter on hover
constexpr ::ui::Color kDangerHoverBorder = {250, 134, 134, 255};
constexpr ::ui::Color kDangerPressed = {190, 44, 44, 255};  // darker on press
constexpr ::ui::Color kDangerPressedBorder = {220, 92, 92, 255};

// ---- Text ----
constexpr ::ui::Color kTextTitle = {236, 246, 242, 255};
constexpr ::ui::Color kTextHeroTitle = {235, 246, 242, 255};
constexpr ::ui::Color kTextSubtitle = {154, 177, 184, 255};
constexpr ::ui::Color kTextDialogTitle = {240, 248, 244, 255};
constexpr ::ui::Color kTextBody = {226, 238, 236, 255};
constexpr ::ui::Color kTextBodyMuted = {202, 218, 216, 255};
constexpr ::ui::Color kTextWeaponName = {238, 246, 244, 255};
constexpr ::ui::Color kTextWeaponDetail = {184, 204, 204, 255};
constexpr ::ui::Color kTextWeaponDetailOff = {142, 148, 150, 255};
constexpr ::ui::Color kTextHud = {224, 238, 236, 255};

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
