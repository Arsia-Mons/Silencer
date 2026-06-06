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

// ---- design tokens (legacy origin/main palette) ----------------------------
// The cool-blue legacy palette: black/Panel #10141C surfaces, PanelBorder
// #565E6F edges, text #E0E7F1, a single cool-blue accent #9FC9FF for the focus
// ring + checked/selection, danger #DD5048. The control gradient is collapsed
// to a flat Panel fill (legacy had no gradient; sprite buttons land in SIL-89).
// STRAIGHT alpha throughout; premultiplied at IR emit.
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

// Accent: the legacy cool blue (#9FC9FF), reused for the focus ring +
// checked/selection. In origin/main the accent was a baked sprite edge, not a
// fill; this constant drives only the focus ring + selection wash here (the
// oval sprite button lands in SIL-89).
constexpr Color kAccent = {92, 208, 92, 255}; // #5CD05C green-bright

// Control surface (green phosphor): near-black green-glass fill with a green-dim
// edge; hover/pressed brighten the green stroke (origin/main never used slate).
// Sprite-backed controls (oval/chrome) override this via image_patch.
constexpr Color kControlTop = {6, 16, 8, 255};      // near-black green glass
constexpr Color kControlBottom = {6, 16, 8, 255};
constexpr Color kControlBorder = {46, 125, 69, 255}; // #2E7D45 green-dim
constexpr Color kHoverTop = {10, 24, 12, 255};
constexpr Color kHoverBottom = {10, 24, 12, 255};
constexpr Color kHoverBorder = {92, 208, 92, 255};   // #5CD05C
constexpr Color kPressedTop = {4, 12, 6, 255};
constexpr Color kPressedBottom = {4, 12, 6, 255};
constexpr Color kPressedBorder = {60, 255, 60, 255}; // #3CFF3C bright

constexpr Color kDisabledTop = {6, 14, 8, 255};
constexpr Color kDisabledBottom = {6, 14, 8, 255};
constexpr Color kDisabledBorder = {30, 74, 44, 255}; // #1E4A2C green-disabled
} // namespace

const Theme &app_theme() {
  static const Theme t = [] {
    Theme th{};
    th.focus_ring = kAccent;
    th.text_default = {92, 208, 92, 255}; // #5CD05C green label/body
    th.text_disabled = {46, 90, 55, 255}; // dim green
    th.caret = {60, 255, 60, 255};        // #3CFF3C bright caret
    th.selection = {92, 208, 92, 96};     // green wash; premultiplied at IR emit

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
    // Base text role: Body face (0) at the legacy native em (11).
    th.text.base.text = TextVisual{th.text_default, 0, 11};

    // Dialog: a rounded, slightly elevated panel a step darker than the controls
    // so stacked controls read as raised against it.
    th.dialog.base.background = {6, 16, 8, 255}; // near-black green glass
    th.dialog.base.corner_radius = kRadius + 2.f;
    th.dialog.base.border.width = {1, 1, 1, 1};
    th.dialog.base.border.color = {{46, 125, 69, 255}, // #2E7D45 green-dim
                                   {46, 125, 69, 255},
                                   {46, 125, 69, 255},
                                   {46, 125, 69, 255}};
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
