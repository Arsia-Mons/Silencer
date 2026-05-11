#ifndef SILENCER_UI_THEME_H
#define SILENCER_UI_THEME_H

// Silencer UI theme — Clay-native palette + font + corner-radius
// constants for values currently hardcoded across screens. No theme
// system here, just file-scope `inline constexpr` declarations meant
// to be referenced directly by screens emitting `CLAY()` blocks (or,
// transitionally, by Node-IR call sites that still pass raw banks /
// widths to legacy factories).
//
// Silencer renders with an indexed palette (0..255), not RGBA. The
// transitional convention: a `Clay_Color`'s R channel carries the
// palette index; G/B/A are ignored. The render-command consumer
// introduced in P02 reads R as the palette index — until then these
// constants stand as the single source of truth for the palette
// indices currently scattered through the screen code.
//
// Sprite-fonts are fixed-width per bank: `fontId` carries the bank,
// `fontSize` carries the per-glyph horizontal advance in logical
// pixels (consumed by `MeasureSpriteText` in layout.cpp), and
// `lineHeight` carries the line-box height.

#include "clay/clay.h"

namespace ui {

// ----- Palette-indexed colors -----
// R = palette index (0..255). G/B/A are ignored by the Silencer
// render path; values supplied as floats so the struct stays a
// valid Clay_Color literal.
inline constexpr Clay_Color kColorPanelBg       = {   0.0f, 0.0f, 0.0f, 255.0f };
inline constexpr Clay_Color kColorCaret         = { 140.0f, 0.0f, 0.0f, 255.0f };
inline constexpr Clay_Color kColorScrollbarFill = { 180.0f, 0.0f, 0.0f, 255.0f };
inline constexpr Clay_Color kColorSelectHi      = { 110.0f, 0.0f, 0.0f, 255.0f };

// ----- Sprite-font configs -----
// fontId = sprite-font bank; fontSize = per-glyph advance. textColor
// is unused by the Silencer text path (glyphs ship pre-shaded in the
// font sprite); kept zero-initialised so the struct is a Clay_Color
// literal. Designated initialisers require C++20 (set in
// clients/silencer/CMakeLists.txt).
inline constexpr Clay_TextElementConfig kFontTitle = {
    .userData = nullptr,
    .textColor = { 0.0f, 0.0f, 0.0f, 0.0f },
    .fontId = 135, .fontSize = 12, .letterSpacing = 0, .lineHeight = 17,
};
inline constexpr Clay_TextElementConfig kFontHeading = {
    .userData = nullptr,
    .textColor = { 0.0f, 0.0f, 0.0f, 0.0f },
    .fontId = 134, .fontSize = 9, .letterSpacing = 0, .lineHeight = 17,
};
inline constexpr Clay_TextElementConfig kFontBody = {
    .userData = nullptr,
    .textColor = { 0.0f, 0.0f, 0.0f, 0.0f },
    .fontId = 133, .fontSize = 6, .letterSpacing = 0, .lineHeight = 11,
};
inline constexpr Clay_TextElementConfig kFontStat = {
    .userData = nullptr,
    .textColor = { 0.0f, 0.0f, 0.0f, 0.0f },
    .fontId = 136, .fontSize = 15, .letterSpacing = 0, .lineHeight = 17,
};

// ----- Corner radius -----
inline constexpr Clay_CornerRadius kCornerSmall = { 4.0f, 4.0f, 4.0f, 4.0f };

}  // namespace ui

#endif
