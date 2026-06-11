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

enum class AppButtonVariant { Primary, Secondary, Danger, Ghost, Oval, Chrome, Rect };
// List = the compact stadium row used by scrolling rosters (character-create
// agency/agent lists): shorter than the Md menu pill, stretched to the pane width.
enum class AppButtonSize { Md, Sm, Lg, List };

// Per-variant label TextVisual (color + face + size + centered). The runtime only
// paints text on Text-role nodes (draw_command_builder append_text), so a Button's
// own `value` is never painted — the visible label must be a styled Text CHILD.
// AppButton renders this through ui::components::Text so it resolves the green
// app_theme text color + the variant's legacy face, instead of the bare-string
// child fallback (which paints with the engine's neutral kTextFill grey).
inline ::ui::TextVisual app_button_label_visual(AppButtonVariant variant,
                                                bool disabled,
                                                AppButtonSize size = AppButtonSize::Md) {
  const ::ui::Color c =
      disabled ? tokens::kTextBodyMuted : tokens::kTextTitle;
  // Lobby/login rect+chrome labels and the cc List rows are a tier smaller than
  // the chunky menu oval pills (golden login/list caps ~17px vs the menu's ~21px).
  float sz = tokens::kFontHeading;
  if (variant == AppButtonVariant::Rect || variant == AppButtonVariant::Chrome ||
      size == AppButtonSize::List)
    sz = tokens::kFontLarge;
  return {.color = c,
          .font_id = tokens::kFaceHeading,
          .font_size = sz,
          .align = ::ui::TextAlign::Center,
          .line_height = sz};
}

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

// Oval-sprite geometry. The whole sprite STRETCHES to the box (origin blits the
// stadium into the button bounds — a wide label like "Connect To Lobby" gets
// gently elongated end caps exactly as origin does; nine-slicing instead draws
// the caps at texture-pixel size, visibly squarer than the golden). Native cell
// 196x33 (Md) -> 294x49.5 at the x1.5 logical scale; labels size wider ovals.
inline ::ui::LayoutStyle app_button_oval_layout(AppButtonSize size) {
  if (size == AppButtonSize::List) {
    // origin LegacyRow plate (bank 6 idx2, 236x27): 40.5 tall at x1.5,
    // stretched to the pane width (grow-able min, unlike the fixed ovals).
    return {
        .align_items = ::ui::AlignItems::Center,
        .justify_content = ::ui::JustifyContent::Center,
        .min_width = ::ui::Length::points(104.0f),
        .height = ::ui::Length::points(40.5f),
        .padding = {16.0f, 16.0f, 6.0f, 6.0f},
    };
  }
  float w = 294.0f; // Md: native 196x33 cell x1.5
  if (size == AppButtonSize::Sm)
    w = 168.0f; // keybind bind-slot oval (only Oval Sm user): native 112x33
  else if (size == AppButtonSize::Lg)
    w = 330.0f; // native 220x33
  // Origin ovals are FIXED sprite cells — a long label ("Connect To Lobby")
  // squeezes inside the cell, never widens it (golden: all menu pills 441
  // device px wide). Vertical padding asymmetric: origin's label baseline
  // sits ~2 device px lower than symmetric centering.
  return {
      .align_items = ::ui::AlignItems::Center,
      .justify_content = ::ui::JustifyContent::Center,
      .width = ::ui::Length::points(w),
      .height = ::ui::Length::points(49.5f),
      .padding = {16.0f, 16.0f, 7.5f, 4.5f},
  };
}


// Green oval sprite-button paint (SIL-89). Image-only patch over a baked bank-6
// oval texture (use_chrome()), label in the Heading face centered on top.
// texture_id 0 (not-yet-baked frame or seam slip) falls back to a vector
// stadium-radius oval. origin draws every oval frame at full bright with no
// per-button dim/ramp, so a static bright tint keeps captures deterministic.
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
            .background(::ui::Color{6, 16, 8, 255})
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
  ov.focus_visible = oval(bright);
  return ov;
}

// Chrome-button geometry. origin/main draws these nine-sliced and SIZED TO THE
// LABEL (ButtonVariant::Chrome size Auto): native height 21 -> 31.5 logical
// (x1.5), paddingX 10 -> 15; a small min-width keeps short labels (OK) from
// collapsing. The bank-7 sprite's caps keep the metal corners crisp.
inline ::ui::LayoutStyle app_button_chrome_layout(AppButtonSize size = AppButtonSize::Md) {
  // Sm = origin's DEFAULT fixed chrome plate (156x21 native -> 234x31.5): the
  // lobby Go Back. Other sizes keep the Auto label-fit (origin size Auto).
  if (size == AppButtonSize::Sm) {
    return {
        .align_items = ::ui::AlignItems::Center,
        .justify_content = ::ui::JustifyContent::Center,
        .width = ::ui::Length::points(234.0f),
        .height = ::ui::Length::points(31.5f),
        .padding = {15.0f, 15.0f, 6.0f, 6.0f},
    };
  }
  return {
      .align_items = ::ui::AlignItems::Center,
      .justify_content = ::ui::JustifyContent::Center,
      .min_width = ::ui::Length::points(70.0f),
      .height = ::ui::Length::points(32.0f),
      .padding = {15.0f, 15.0f, 6.0f, 6.0f},
  };
}

// Metal-chrome sprite-button paint (SIL-90). Nine-sliced bank-7 sprite (caps
// {l12,r12,t4,b4}) with the label in the Large face. The focus frame (idx28)
// is brightness-ramped by `lit` — the composition root drives it from the
// use_clock() phase so a focused chrome button ramps at the legacy ~24fps
// cadence (SIL-107); idle (idx24) stays full white. texture_id 0 falls back to
// a rounded slate button.
inline ::ui::StyleStatePatch
app_button_chrome_patch(uint32_t idle, uint32_t focus,
                        ::ui::Color lit = {255, 255, 255, 255}) {
  const ::ui::TextVisual label{.color = tokens::kTextTitle,
                               .font_id = tokens::kFaceHeading,
                               .font_size = tokens::kFontHeading,
                               .align = ::ui::TextAlign::Center,
                               .line_height = tokens::kLineHeading};
  ::ui::StyleStatePatch ov{};
  if (!idle) {
    ::ui::StylePatch p =
        ::ui::patch()
            .background(::ui::Color{6, 16, 8, 255})
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
  const uint32_t f = focus ? focus : idle;
  ov.base = chrome(idle, ::ui::Color{255, 255, 255, 255});
  ov.hover = chrome(f, lit);
  ov.focus_visible = chrome(f, lit);
  return ov;
}

// Square-cornered rect button geometry (origin/main lobby/login buttons): thin
// green-hairline rectangles, compact. Sized to label with a per-call min width.
inline ::ui::LayoutStyle app_button_rect_layout() {
  return {
      .align_items = ::ui::AlignItems::Center,
      .justify_content = ::ui::JustifyContent::Center,
      .min_width = ::ui::Length::points(96.0f),
      .height = ::ui::Length::points(30.0f),
      .padding = {14.0f, 14.0f, 4.0f, 4.0f},
  };
}

// Green-hairline rect button paint (lobby/login). idle = near-black fill + dim
// green border, square corners; `selected` fills it (the default/primary action,
// e.g. Login/Create) and hover/focus brightens. The flat 2-stop gradient
// replaces the theme-role's slate gradient so the green reads.
inline ::ui::StyleStatePatch app_button_rect_patch(bool selected,
                                                   ::ui::Color lit = {255, 255,
                                                                      255,
                                                                      255}) {
  const ::ui::TextVisual label{.color = tokens::kTextTitle,
                               .font_id = tokens::kFaceHeading,
                               .font_size = tokens::kFontLarge,
                               .align = ::ui::TextAlign::Center,
                               .line_height = tokens::kLineLarge};
  auto rect = [&](::ui::Color top, ::ui::Color bot,
                  ::ui::Color border) -> ::ui::StylePatch {
    ::ui::StylePatch p =
        ::ui::patch()
            .background(top)
            .gradient(::ui::Gradient{.angle_deg = 0.0f,
                                     .stop_count = 2,
                                     .stops = {{0.0f, top}, {1.0f, bot}}})
            .corner_radius(0.0f)
            .border(::ui::Border{{1, 1, 1, 1}, {border, border, border, border}});
    p.text = ::ui::opt(label);
    return p;
  };
  // origin/main login/lobby rects are a top-lit green vertical gradient
  // (~(8,84,0) down to (0,44,0)) with a kTextBody-green hairline — Login and
  // Cancel share it (origin does not paint a distinct selected fill). `selected`
  // only brightens the interaction states.
  const ::ui::Color top{8, 84, 0, 255}, bot{0, 44, 0, 255};
  const ::ui::Color htop = selected ? ::ui::Color{16, 108, 0, 255}
                                    : ::ui::Color{12, 96, 0, 255};
  const ::ui::Color hbot{4, 64, 0, 255};
  ::ui::StyleStatePatch ov{};
  ov.base = rect(top, bot, tokens::kTextBody);
  ov.hover = rect(htop, hbot, lit);
  ov.focus_visible = rect(htop, hbot, lit);
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
