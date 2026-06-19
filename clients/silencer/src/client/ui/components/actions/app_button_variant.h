#pragma once

// AppButton design-system enums + impl-only layout/variant helpers.

#include "ui/components/common.h" // ::ui::LayoutStyle, ::ui::StyleStatePatch
#include "ui/style/style_patch.h" // ::ui::patch()
#include "client/ui/components/tokens.h" // silencer::tokens accent/danger

namespace silencer {

enum class AppButtonVariant { Primary, Secondary, Danger, Ghost, Oval, Chrome };
// List = the compact stadium row used by scrolling rosters, stretched to the
// pane width.
enum class AppButtonSize { Md, Sm, Lg, List };

// Per-variant label TextVisual (color + face + size + centered).
inline ::ui::TextVisual app_button_label_visual(AppButtonVariant variant,
                                                bool disabled,
                                                AppButtonSize size = AppButtonSize::Md,
                                                int ramp_phase = 0) {
  ::ui::Color c = disabled ? tokens::kTextBodyMuted : tokens::kTextTitle;
  // Chrome labels and cc List rows are a tier smaller than the menu oval pills.
  float sz = tokens::kFontHeading;
  uint16_t face = tokens::kFaceHeading;
  if (variant == AppButtonVariant::Chrome || size == AppButtonSize::List) {
    sz = tokens::kFontLarge;
    face = tokens::kFaceLarge;
  }
  // Hover/focus ramp: sprite-backed labels re-render at brightness
  // 128 + phase*2. Callers pass phase 0 for static chrome.
  if (ramp_phase > 0 && !disabled &&
      (variant == AppButtonVariant::Oval || variant == AppButtonVariant::Chrome))
    c = tokens::hud_text_key(0, (uint8_t)(128 + ramp_phase * 2));
  return {.color = c,
          .font_id = face,
          .font_size = sz,
          .align = ::ui::TextAlign::Center,
          .line_height = sz};
}

// Layout-only baseline geometry. border_width stays 0 (Button's own resolved
// chrome owns paint).
inline ::ui::LayoutStyle app_button_layout(AppButtonSize size, bool /*selected*/) {
  switch (size) {
  case AppButtonSize::Sm:
    return {
        .align_items = ::ui::AlignItems::Center,
        .justify_content = ::ui::JustifyContent::Center,
        .width = ::ui::Length::points(132.0f),
        .height = ::ui::Length::points(34.0f),
        .padding = {12.0f, 12.0f, 7.0f, 7.0f},
    };
  case AppButtonSize::Md:
  default:
    return {
        .align_items = ::ui::AlignItems::Center,
        .justify_content = ::ui::JustifyContent::Center,
        .width = ::ui::Length::points(132.0f),
        .height = ::ui::Length::points(38.0f),
        .padding = {14.0f, 14.0f, 8.0f, 8.0f},
    };
  }
}

// Oval-sprite geometry. The whole sprite STRETCHES to the box (nine-slicing
// would draw the caps at texture-pixel size, visibly squarer than the golden).
inline ::ui::LayoutStyle app_button_oval_layout(AppButtonSize size) {
  if (size == AppButtonSize::List) {
    // LegacyRow plate (bank 6 idx2), stretched to the pane width.
    return {
        .align_items = ::ui::AlignItems::Center,
        .justify_content = ::ui::JustifyContent::Start,
        .height = ::ui::Length::points(40.5f),
        .min_width = ::ui::Length::points(104.0f),
        .padding = {16.0f, 16.0f, 11.0f, 1.5f},
    };
  }
  float w = 294.0f; // Md: native 196x33 cell x1.5
  if (size == AppButtonSize::Sm)
    w = 168.0f; // native 112x33
  else if (size == AppButtonSize::Lg)
    w = 330.0f; // native 220x33
  // FIXED cells: a long label squeezes inside, never widens the oval. The
  // fractional padding bias is floor-tuned against the goldens (glyph device x
  // is floor(1.5 * centered-x)) — do not round it.
  return {
      .align_items = ::ui::AlignItems::Center,
      .justify_content = ::ui::JustifyContent::Center,
      .width = ::ui::Length::points(w),
      .height = ::ui::Length::points(49.5f),
      .padding = {16.75f, 15.25f, 8.0f, 4.0f},
  };
}


// Green oval sprite-button paint. Image-only patch over a baked bank-6 oval
// texture; texture_id 0 falls back to a vector stadium oval. Every state slot
// draws the caller's already-phase-resolved frame at full bright.
inline ::ui::StyleStatePatch app_button_oval_patch(uint32_t tex) {
  const ::ui::TextVisual label{.color = tokens::kTextTitle,
                               .font_id = tokens::kFaceHeading,
                               .font_size = tokens::kFontHeading,
                               .align = ::ui::TextAlign::Center,
                               .line_height = tokens::kLineHeading};
  ::ui::StyleStatePatch ov{};
  if (!tex) {
    ::ui::StylePatch p =
        ::ui::patch()
            .background(tokens::kControlFallbackFill)
            .gradient(::ui::Gradient{})
            .corner_radius(16.5f) // h/2 for the 33px stadium
            .border(::ui::Border{{1, 1, 1, 1},
                                 {tokens::kAccent, tokens::kAccent,
                                  tokens::kAccent, tokens::kAccent}});
    p.text = ::ui::opt(label);
    ov.base = p;
    return ov;
  }
  auto oval = [&](::ui::Color tint) -> ::ui::StylePatch {
    ::ui::StylePatch p = tokens::image_patch(tex, tint);
    p.text = ::ui::opt(label);
    return p;
  };
  const ::ui::Color bright{255, 255, 255, 255};
  ov.base = oval(bright);
  ov.hover = oval(bright);
  ov.pressed = oval(bright); // override theme pressed border so no rectangular
                             // box leaks over the sprite on mouse-down
  ov.focus_visible = oval(bright);
  return ov;
}

// Chrome-button geometry. Nine-sliced and SIZED TO THE LABEL (size Auto), with
// a small min-width so short labels don't collapse.
inline ::ui::LayoutStyle app_button_chrome_layout(AppButtonSize size = AppButtonSize::Md) {
  // Top-aligns the label (Chrome has no centerContentY): justify Start + pad-top.
  if (size == AppButtonSize::Sm) {
    return {
        .align_items = ::ui::AlignItems::Center,
        .justify_content = ::ui::JustifyContent::Start,
        .width = ::ui::Length::points(234.0f),
        .height = ::ui::Length::points(31.5f),
        .padding = {15.0f, 15.0f, 5.0f, 0.0f},
    };
  }
  // Lg = label-fit chrome with no min width. The Button host adds 1 logical px
  // per side, so 14 + 1 = origin's 10 virtual.
  if (size == AppButtonSize::Lg) {
    return {
        .align_items = ::ui::AlignItems::Center,
        .justify_content = ::ui::JustifyContent::Start,
        .height = ::ui::Length::points(31.5f),
        .padding = {14.0f, 14.0f, 5.0f, 0.0f},
    };
  }
  return {
      .align_items = ::ui::AlignItems::Center,
      .justify_content = ::ui::JustifyContent::Start,
      .height = ::ui::Length::points(31.5f),     // origin Chrome plate 21 virtual
      .min_width = ::ui::Length::points(138.0f), // origin kActionButtonMinWidth 92 virtual
      .padding = {18.0f, 18.0f, 5.0f, 0.0f},     // origin paddingX 12 virtual; pad-top
                                                 // 5 lands the label on vy_btn+4
  };
}

// Metal-chrome sprite-button paint. Nine-sliced bank-7 idx24 sprite
// (caps {l12,r12,t4,b4}); texture_id 0 falls back to a rounded slate button.
inline ::ui::StyleStatePatch app_button_chrome_patch(uint32_t tex) {
  const ::ui::TextVisual label{.color = tokens::kTextTitle,
                               .font_id = tokens::kFaceHeading,
                               .font_size = tokens::kFontHeading,
                               .align = ::ui::TextAlign::Center,
                               .line_height = tokens::kLineHeading};
  ::ui::StyleStatePatch ov{};
  if (!tex) {
    ::ui::StylePatch p =
        ::ui::patch()
            .background(tokens::kControlFallbackFill)
            .gradient(::ui::Gradient{})
            .corner_radius(3.0f)
            .border(::ui::Border{{1, 1, 1, 1},
                                 {tokens::kBorderPanel, tokens::kBorderPanel,
                                  tokens::kBorderPanel, tokens::kBorderPanel}});
    p.text = ::ui::opt(label);
    ov.base = p;
    return ov;
  }
  const ::ui::SideWidths caps{.top = 4.0f, .right = 12.0f, .bottom = 4.0f,
                              .left = 12.0f};
  auto chrome = [&](uint32_t tex, ::ui::Color tint) -> ::ui::StylePatch {
    ::ui::StylePatch p = tokens::image_patch(tex, tint, caps);
    p.text = ::ui::opt(label);
    return p;
  };
  const ::ui::Color bright{255, 255, 255, 255};
  ov.base = chrome(tex, bright);
  ov.hover = chrome(tex, bright);
  ov.pressed = chrome(tex, bright); // override theme pressed border so no
                                    // rectangular box leaks over the sprite
  ov.focus_visible = chrome(tex, bright);
  return ov;
}

// State-aware variant paint over the theme's Button role. Each non-Secondary
// variant supplies its own base/hover/pressed slots (each a FLAT 2-stop gradient
// so the base gradient never bleeds through) so it keeps its branded look across
// interaction states. Secondary returns {} on purpose — the empty patch IS the
// theme default, so a plain AppButton paints unchanged.
inline ::ui::StyleStatePatch
app_button_variant_patch(AppButtonVariant variant) {
  auto solid = [](::ui::Color fill, ::ui::Color border,
                  float border_width) -> ::ui::StylePatch {
    return ::ui::patch()
        .background(fill)
        .gradient(::ui::Gradient{.angle_deg = 0.0f,
                                 .stop_count = 2,
                                 .stops = {{0.0f, fill}, {1.0f, fill}}})
        .border(::ui::Border{
            {border_width, border_width, border_width, border_width},
            {border, border, border, border}});
  };

  switch (variant) {
  case AppButtonVariant::Primary: {
    ::ui::StyleStatePatch ov{};
    ov.base = solid(tokens::kAccent, tokens::kAccentBorder, 1.0f);
    ov.hover = solid(tokens::kAccentHover, tokens::kAccentHoverBorder, 1.0f);
    ov.pressed =
        solid(tokens::kAccentPressed, tokens::kAccentPressedBorder, 1.0f);
    return ov;
  }
  case AppButtonVariant::Danger: {
    ::ui::StyleStatePatch ov{};
    ov.base = solid(tokens::kDanger, tokens::kDangerBorder, 1.0f);
    ov.hover = solid(tokens::kDangerHover, tokens::kDangerHoverBorder, 1.0f);
    ov.pressed =
        solid(tokens::kDangerPressed, tokens::kDangerPressedBorder, 1.0f);
    return ov;
  }
  case AppButtonVariant::Ghost: {
    const ::ui::Color transparent = {0, 0, 0, 0};
    ::ui::StyleStatePatch ov{};
    // Truly chromeless: no fill/border AND no focus ring. chromeless(true) gates
    // the builder's focus-ring injection, but the theme's focus_visible Outline
    // still draws regardless of chromeless — so the outline must be cleared in
    // BOTH base and focus_visible, or the auto-focused prompt shows the ring.
    ov.base = solid(transparent, transparent, 0.0f);
    ov.base.chromeless = ::ui::opt(true);
    ov.base.outline = ::ui::opt(::ui::Outline{});
    ov.focus_visible = ::ui::patch().chromeless(true).outline(::ui::Outline{});
    // Washes must be FLAT gradients (a resolved gradient IS the fill) to REPLACE
    // the theme-role's hover/pressed gradient, not sit invisibly behind it.
    ov.hover = solid({255, 255, 255, 18}, transparent, 0.0f);
    ov.pressed = solid({255, 255, 255, 30}, transparent, 0.0f);
    return ov;
  }
  case AppButtonVariant::Secondary:
  default:
    return {};
  }
}

} // namespace silencer
