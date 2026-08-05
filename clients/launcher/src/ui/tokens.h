#ifndef LAUNCHER_UI_TOKENS_H
#define LAUNCHER_UI_TOKENS_H

// Design tokens for the launcher UI: the phosphor-green palette, the glyph
// faces, the wrap widths the 900x600 layout is tuned around, and the style
// presets the views apply. This is the ONE file that holds raw ::ui::Color
// literals — every .cppx view takes its paint from here (the same rule the
// game's client/ui/components/tokens.h carries).
//
// The `using` declarations below are deliberate: every launcher view file
// opens `namespace launcher` and wants the style vocabulary unqualified.

#include "ui/runtime/element.h"
#include "ui/style/style_patch.h"

#include <string>

namespace launcher {

using ::ui::AlignItems;
using ::ui::Border;
using ::ui::Color;
using ::ui::EdgeSizes;
using ::ui::FlexDirection;
using ::ui::JustifyContent;
using ::ui::LayoutStyle;
using ::ui::Length;
using ::ui::PositionType;
using ::ui::StyleStatePatch;
using ::ui::TextAlign;
using ::ui::TextVisual;
using ::ui::UiElement;

// ---- Phosphor-green palette (anchored to web/website/styles.css) ------------
constexpr Color kBg = {3, 6, 3, 255};          // #030603 screen
constexpr Color kPanel = {8, 15, 9, 235};      // raised panel
constexpr Color kPanelSoft = {6, 12, 7, 200};  // list row
constexpr Color kBorder = {36, 66, 40, 255};   // sage border
constexpr Color kBorderDim = {20, 40, 24, 255};
constexpr Color kText = {197, 225, 197, 255};   // #c5e1c5
constexpr Color kTextDim = {146, 181, 146, 255};// #92b592
constexpr Color kTextFaint = {91, 138, 101, 255};// #5b8a65
constexpr Color kAccent = {96, 205, 120, 255};  // phosphor highlight
constexpr Color kAccentBright = {209, 250, 215, 255}; // #d1fad7
constexpr Color kAccentDeep = {24, 96, 34, 255};      // filled control
constexpr Color kAccentFaint = {24, 96, 34, 70};      // hover wash
constexpr Color kAccentRow = {17, 45, 24, 255};       // selected row (opaque)
constexpr Color kOnline = {120, 220, 130, 255};
constexpr Color kOffline = {176, 96, 96, 255};
constexpr Color kScrim = {3, 6, 3, 170};
constexpr Color kPopover = {8, 15, 9, 250};   // popovers sit over the backdrop
constexpr Color kTransparent = {0, 0, 0, 0};
constexpr Color kPressed = {16, 40, 20, 200};
constexpr Color kHoverDeep = {34, 128, 46, 255};
constexpr Color kPressedDeep = {20, 72, 28, 255};
constexpr Color kDisabledFill = {14, 26, 16, 255};
constexpr Color kDismissWash = {3, 6, 3, 110};
constexpr Color kBackdropTint = {110, 110, 110, 255};

// Glyph faces (baked from the origin sprite banks in bake_glyph_faces):
// Body=0 (bank 133), Large=1 (134), Title=2 (136), Tiny=3 (132), Heading=4
// (135). Text is pixel-perfect ONLY at the face's native line height (or an
// integer multiple): Body 11, Large 15, Title 23, Tiny 7, Heading 19.
//
// The whole UI draws in Title (bank 136) by choice. It is authored at the
// 11/19px sizes the layout was built around, NOT Title's native 23 — so the
// glyph cells nearest-scale down (0.48x / 0.83x) instead of blitting 1:1.
// That is deliberate; going pixel-perfect means re-tuning every wrap width
// and the 900x600 window for Title's 16px advance.
constexpr uint16_t kFaceBody = 2;
constexpr uint16_t kFaceTitle = 2;
constexpr uint16_t kFaceHeading = 2;

// Detail-pane text wrap width (Yoga wraps only at a definite points width).
// 900 window - 36 root pad - 32 panel pad - 220 list - 12 gap - 28 detail pad
// - ~22 scrollbar/track.
constexpr float kDetailTextW = 550.0f;
// SETTINGS pane: one full-width column, no list beside it.
// 900 window - 36 root pad - 32 panel pad - 28 pane pad - ~22 scrollbar/track.
constexpr float kSettingsTextW = 780.0f;
constexpr float kListTextW = 168.0f;
// Channel drop-up: popover width and the wrap widths inside it (popover pad
// 12 each side; channel rows pad another 12).
constexpr float kDropW = 460.0f;
constexpr float kDropInnerTextW = kDropW - 24.0f - 2.0f;  // 434
constexpr float kDropRowTextW = kDropInnerTextW - 24.0f;  // 410
// A JSX `layout` REPLACES the whole LayoutStyle, so every field a control
// relies on has to be spelled out — including ButtonProps' 132pt default
// width, which is what keeps a row of chrome buttons (the tabs, the drop-up
// actions, the channel segment) one size whatever their labels say.
constexpr float kControlW = 132.0f;
// Auth popover body width, and the uninstall dialog's wrapped path.
constexpr float kAuthTextW = 228.0f;
constexpr float kConfirmTextW = 312.0f;

inline TextVisual tv(Color c, float size, uint16_t face,
                     TextAlign align = TextAlign::Left,
                     ::ui::TextWrap wrap = ::ui::TextWrap::None) {
  TextVisual t{};
  t.color = c;
  t.font_id = face;
  t.font_size = size;
  t.align = align;
  t.wrap = wrap;
  return t;
}

inline StyleStatePatch text_style(Color c, float size, uint16_t face,
                                  TextAlign align = TextAlign::Left,
                                  ::ui::TextWrap wrap = ::ui::TextWrap::None) {
  return ::ui::patch().text(tv(c, size, face, align, wrap));
}

inline Border border1(Color c) { return Border{{1, 1, 1, 1}, {c, c, c, c}}; }

inline StyleStatePatch panel_style(Color bg, Color border, float corner) {
  return ::ui::patch().background(bg).border(border1(border)).corner_radius(corner);
}

// Frame-owned copy of a computed string, for the `const char *` props.
inline const char *dup(const std::string &s) { return ::ui::copy_string(s.c_str()); }

// ---- Style presets ---------------------------------------------------------
inline StyleStatePatch ghost_button(Color text_color, float size = 11) {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(kTransparent).border(border1(kBorder))
                .corner_radius(3.0f).text(tv(text_color, size, kFaceBody, TextAlign::Center));
  sp.hover = ::ui::patch().background(kAccentFaint).border(border1(kAccent));
  sp.pressed = ::ui::patch().background(kPressed);
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  sp.disabled = ::ui::patch().border(border1(kBorderDim))
                    .text(tv(kTextFaint, size, kFaceBody, TextAlign::Center));
  return sp;
}

// Origin uses bank 135 for button labels (the game's Heading face).
inline StyleStatePatch accent_button(float size = 19) {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(kAccentDeep).border(border1(kAccent))
                .corner_radius(3.0f).text(tv(kAccentBright, size, kFaceHeading, TextAlign::Center));
  sp.hover = ::ui::patch().background(kHoverDeep).border(border1(kAccentBright));
  sp.pressed = ::ui::patch().background(kPressedDeep);
  // Inset: this face sits flush against the channel segment in the playbar.
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccentBright, -4.0f});
  sp.disabled = ::ui::patch().background(kDisabledFill).border(border1(kBorderDim))
                    .text(tv(kTextFaint, size, kFaceHeading, TextAlign::Center));
  return sp;
}

inline StyleStatePatch toggle_button(bool selected) {
  StyleStatePatch sp{};
  if (selected) {
    sp.base = ::ui::patch().background(kAccentDeep).border(border1(kAccent))
                  .corner_radius(3.0f).text(tv(kAccentBright, 11, kFaceBody, TextAlign::Center));
  } else {
    sp.base = ::ui::patch().background(kTransparent).border(border1(kBorderDim))
                  .corner_radius(3.0f).text(tv(kTextDim, 11, kFaceBody, TextAlign::Center));
    sp.hover = ::ui::patch().border(border1(kBorder)).text(tv(kText, 11, kFaceBody, TextAlign::Center));
  }
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  return sp;
}

// Flat text link (topbar WEBSITE/DISCORD) — the lowest-traffic controls in the
// bar, so they carry no box at rest and only light up under the cursor.
//
// Built on Box, not Button, for two reasons:
//   1. A Button host paints a faint box regardless of the style patch (a
//      zero-width border does not suppress it), which is exactly the weight
//      these links should not carry. theme.box is blank, so a Box draws only
//      what it is told to.
//   2. Button renders `label` as a plain child, so the resolved visual's text
//      never reaches it and a `.text()` patch is silently dead. Both link
//      styles here therefore live on background, and the label comes in as an
//      explicitly styled Text child.
// Hover is a wash rather than a border so nothing reflows by a pixel.
inline StyleStatePatch link_button() {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(kTransparent).corner_radius(3.0f);
  sp.hover = ::ui::patch().background(kAccentFaint);
  sp.pressed = ::ui::patch().background(kPressed);
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  return sp;
}

inline StyleStatePatch input_style() {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(kPanelSoft).border(border1(kBorder)).corner_radius(3.0f)
                .text(tv(kText, 11, kFaceBody));
  sp.focus_visible = ::ui::patch().border(border1(kAccent));
  return sp;
}

// ---- Layout shorthands -----------------------------------------------------
inline LayoutStyle row(float gap = 0) {
  LayoutStyle s{};
  s.direction = FlexDirection::Row;
  s.align_items = AlignItems::Center;
  if (gap)
    s.gap = gap;
  return s;
}

inline LayoutStyle col(float gap = 0) {
  LayoutStyle s{};
  s.direction = FlexDirection::Column;
  if (gap)
    s.gap = gap;
  return s;
}

// A `row()`/`col()` that fills its parent's cross axis — the launcher's most
// common shape, and the one that keeps wrapped text inside the panel.
inline LayoutStyle row_full(float gap = 0) {
  LayoutStyle s = row(gap);
  s.width = Length::percent(100.0f);
  return s;
}

inline LayoutStyle col_full(float gap = 0) {
  LayoutStyle s = col(gap);
  s.width = Length::percent(100.0f);
  return s;
}

} // namespace launcher

#endif
