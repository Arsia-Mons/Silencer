#pragma once

// AppButton design-system enums + impl-only layout/variant helpers.
//
// Host detail (::ui::LayoutStyle / ::ui::StyleStatePatch) is allowed in this
// component-impl header because it is consumed only by app_button.cppx and the
// leaf components that adapt ui::components::Button directly. Screen authors see
// only the semantic AppButtonVariant / AppButtonSize enums on AppButtonProps.
//
// BASELINE (byte-exact): AppButton reproduces the CURRENT ui Button DEFAULT
// geometry, which has NO border. `selected` does NOT change AppButton layout at
// baseline (the selection-border belongs to weapon tiles / equipment slots,
// which are authored later and adapt Button directly). The `selected` param is
// kept on the API for forward-compat but is intentionally ignored here.

#include "ui/components/common.h" // ::ui::LayoutStyle, ::ui::StyleStatePatch
#include "ui/style/style_patch.h" // ::ui::patch()
#include "client/ui/components/tokens.h" // silencer::tokens accent/danger

namespace silencer {

enum class AppButtonVariant { Primary, Secondary, Danger, Ghost, Oval };
enum class AppButtonSize { Md, Sm, Lg };

// Layout-only baseline geometry. Mirrors ui::components::ButtonProps default
// LayoutStyle (button.hx:23-29) for Md; Sm shrinks height/padding to the
// loadout tab metrics. border_width stays 0 (Button's own resolved chrome owns
// paint).
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

// Fixed legacy oval-sprite geometry (origin/main button.cpp): Md 196x33 (idx7),
// Sm 112x33 (idx28), Lg 220x33 (idx23). Height is 33 for every size. The label
// rides centered on top of the plain-stretched sprite.
inline ::ui::LayoutStyle app_button_oval_layout(AppButtonSize size) {
  float w = 196.0f;
  if (size == AppButtonSize::Sm)
    w = 112.0f;
  else if (size == AppButtonSize::Lg)
    w = 220.0f;
  return {
      .align_items = ::ui::AlignItems::Center,
      .justify_content = ::ui::JustifyContent::Center,
      .width = ::ui::Length::points(w),
      .height = ::ui::Length::points(33.0f),
      .padding = {16.0f, 16.0f, 8.0f, 8.0f},
  };
}

// Green oval sprite-button paint (SIL-89). Image-only patch over a baked bank-6
// oval texture (use_chrome()), with the label in the Title face centered on top.
// STATIC 2-state v1: idle is dimmer, focus/hover brightens via a discrete tint
// (NOT the legacy smooth 5-phase 24fps ramp — that needs the clock seam, SIL-107).
// texture_id 0 (not-yet-baked frame or seam slip) falls back to a vector
// stadium-radius oval (corner_radius = h/2, cool-blue outline, dark fill).
inline ::ui::StyleStatePatch app_button_oval_patch(uint32_t tex) {
  const ::ui::TextVisual label{.color = tokens::kTextTitle,
                               .font_id = tokens::kFaceTitle,
                               .font_size = tokens::kFontTitle,
                               .align = ::ui::TextAlign::Center,
                               .line_height = tokens::kLineTitle};
  ::ui::StyleStatePatch ov{};
  if (!tex) {
    ::ui::StylePatch p =
        ::ui::patch()
            .background(::ui::Color{16, 20, 28, 255})
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
  ov.base = oval(::ui::Color{150, 150, 150, 255});         // idle (dimmed)
  ov.hover = oval(::ui::Color{255, 255, 255, 255});        // lit on hover
  ov.focus_visible = oval(::ui::Color{255, 255, 255, 255}); // lit on focus
  return ov;
}

// State-aware variant paint overlay over the theme's slate Button role. The
// returned StyleStatePatch owns its OWN base/hover/pressed slots, which
// resolve() layers OVER the matching theme-role slot per active interaction
// flag. Because each non-Secondary variant supplies its own hover/pressed
// patch, the variant keeps its branded look across interaction states instead
// of reverting to the slate hover/pressed chrome. Each solid fill sets a FLAT
// 2-stop gradient (top==bottom) so the slate base gradient never bleeds through.
//
// - Ghost: transparent base (no fill, no border); hover/pressed paint a subtle
//   white wash so the control stays borderless yet reacts to interaction.
// - Primary: accent fill+border base, with on-palette lighter hover / darker
//   pressed accents (tokens::kAccentHover / kAccentPressed).
// - Danger: same treatment on the danger palette.
// - Secondary: returns {} on purpose — the empty patch IS the theme default
//   slate button, so a plain AppButton with no variant paints unchanged
//   (including the theme's own slate hover/pressed deltas).
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
    ov.base = solid(transparent, transparent, 0.0f);
    // Own subtle washes as FLAT fills (not bare backgrounds): a resolved
    // gradient IS the fill (draw_command_builder.cpp:258), so the wash must be
    // emitted as a flat 2-stop gradient to REPLACE the theme-role's slate
    // hover/pressed gradient rather than sit invisibly behind it. Border stays
    // zero so a hovered/pressed Ghost remains borderless.
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
