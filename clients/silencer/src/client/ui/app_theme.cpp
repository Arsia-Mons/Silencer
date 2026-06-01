#include "client/ui/app_theme.h"

// app_theme(): the v1 product design system (moved verbatim out of
// src/ui/style/default_theme.cpp). src/ui/ now keeps only a neutral, flat
// fallback; the PRODUCT look lives here in client/ and is installed via
// ThemeProvider.
//
// Which patches are populated:
//   base          - always (dense)
//   disabled      - button/input/checkbox (dimmed fill + border)
//   checked       - checkbox / checkbox_mark
//   focus_visible - every focusable role (the ONLY wiring that produces the focus ring)
//   hover/pressed - every focusable role (subtle surface response)

namespace client::ui {

// ---- v1 design tokens (the deliberate new look) ----------------------------
// A cohesive modern dark UI: a deep neutral-slate palette, UNIFORM 8px rounded
// corners on every interactive surface, a subtle top-lit vertical gradient on
// controls, and a single crisp accent (cool blue) for the focus ring and the
// checked/selection state. STRAIGHT alpha throughout; premultiplied at IR emit.
namespace {
using ::ui::Border;
using ::ui::Color;
using ::ui::Gradient;
using ::ui::RoleStyle;
using ::ui::StylePatch;
using ::ui::Theme;
using ::ui::TextVisual;
using ::ui::VisualStyle;
using ::ui::opt;
using ::ui::Outline;
using ::ui::patch;

constexpr float kRadius = 8.f; // uniform corner radius on all control surfaces

// Accent: one cool blue, reused for the focus ring + checked/selection so the
// system reads as a single design language rather than a grab-bag of colors.
constexpr Color kAccent = {96, 165, 250, 255};

// Control surface: a subtle vertical gradient (top a touch lighter than bottom,
// as if lit from above) over a deep slate. 90deg = straight down.
constexpr Color kControlTop = {40, 46, 58, 255};
constexpr Color kControlBottom = {28, 33, 43, 255};
constexpr Color kControlBorder = {70, 80, 98, 255};
constexpr Color kHoverTop = {48, 56, 70, 255};
constexpr Color kHoverBottom = {34, 40, 52, 255};
constexpr Color kHoverBorder = {90, 104, 126, 255};
constexpr Color kPressedTop = {26, 31, 41, 255};
constexpr Color kPressedBottom = {20, 24, 32, 255};
constexpr Color kPressedBorder = {96, 165, 250, 255};

constexpr Color kDisabledTop = {30, 34, 42, 255};
constexpr Color kDisabledBottom = {24, 27, 34, 255};
constexpr Color kDisabledBorder = {52, 58, 68, 255};
} // namespace

const Theme &app_theme() {
  static const Theme t = [] {
    Theme th{};
    th.focus_ring = kAccent;
    th.text_default = {226, 231, 240, 255};
    th.text_disabled = {118, 126, 140, 255};
    th.caret = {226, 231, 240, 255};
    th.selection = {96, 165, 250, 96}; // accent wash; premultiplied at IR emit

    // A vertical 2-stop gradient helper (top -> bottom). angle 90deg = downward.
    auto vgrad = [](Color top, Color bottom) {
      Gradient g{};
      g.angle_deg = 90.f;
      g.stop_count = 2;
      g.stops[0] = {0.f, top};
      g.stops[1] = {1.f, bottom};
      return g;
    };

    // Focus ring patch: ONLY outline is set; everything else inherits from base.
    // A crisp 2px accent ring, outset 2px so it sits just outside the control.
    const StylePatch focus_ring_patch =
        patch().outline(Outline{2.f, th.focus_ring, 2.f});

    auto seed_control = [&](RoleStyle &r) {
      // background is the gradient's bottom stop so any path that reads the flat
      // fill (e.g. a non-gradient executor) still lands on-palette.
      r.base.background = kControlBottom;
      r.base.corner_radius = kRadius;
      r.base.gradient = vgrad(kControlTop, kControlBottom);
      r.base.border.width = {1, 1, 1, 1};
      r.base.border.color = {kControlBorder, kControlBorder, kControlBorder,
                             kControlBorder};
      r.disabled.background = opt(kDisabledBottom);
      r.disabled.gradient = opt(vgrad(kDisabledTop, kDisabledBottom));
      r.disabled.border = opt(Border{{1, 1, 1, 1},
                                     {kDisabledBorder, kDisabledBorder,
                                      kDisabledBorder, kDisabledBorder}});
      r.hover.background = opt(kHoverBottom);
      r.hover.gradient = opt(vgrad(kHoverTop, kHoverBottom));
      r.hover.border =
          opt(Border{{1, 1, 1, 1},
                     {kHoverBorder, kHoverBorder, kHoverBorder, kHoverBorder}});
      r.pressed.background = opt(kPressedBottom);
      r.pressed.gradient = opt(vgrad(kPressedTop, kPressedBottom));
      r.pressed.border = opt(
          Border{{1, 1, 1, 1},
                 {kPressedBorder, kPressedBorder, kPressedBorder,
                  kPressedBorder}});
      r.focus_visible = focus_ring_patch; // the locked focus ring
    };
    seed_control(th.button);
    seed_control(th.input);
    seed_control(th.checkbox);

    // The checkbox BODY does not react to `checked` (only the mark does). The
    // mark inherits the body's 8px radius so the fill sits flush inside.
    th.checkbox_mark.base.background = {0, 0, 0, 0}; // hidden until checked
    th.checkbox_mark.base.corner_radius = 4.f;
    th.checkbox_mark.checked.background = opt(kAccent); // visible accent mark

    th.box.base = VisualStyle{};
    th.text.base.text = TextVisual{th.text_default, 0, 14};

    // Dialog: a rounded, slightly elevated panel a step darker than the controls
    // so stacked controls read as raised against it.
    th.dialog.base.background = {20, 23, 30, 248};
    th.dialog.base.corner_radius = kRadius + 2.f;
    th.dialog.base.border.width = {1, 1, 1, 1};
    th.dialog.base.border.color = {{46, 52, 64, 255},
                                   {46, 52, 64, 255},
                                   {46, 52, 64, 255},
                                   {46, 52, 64, 255}};
    return th;
  }();
  return t;
}

::ui::UiElement ThemeProvider(::ui::UiChildren children) {
  // app_theme() is a function-local `static const Theme` with program-duration
  // lifetime and a stable address, and use_theme() only ever reads it back as
  // const — so we hand the context a pointer to it directly. This intentionally
  // supersedes the spec's copy_value() suggestion: no per-frame copy of the
  // (large) Theme into the element arena is needed when the value is static.
  return ::ui::provider("ThemeProvider", &::ui::ThemeContext,
                        const_cast<::ui::Theme *>(&app_theme()), children);
}

} // namespace client::ui
