#include "theme.h"

// default_theme(): a NEUTRAL, unopinionated FALLBACK theme — the look ui/ ships
// with when NO ThemeContext provider is installed. It carries no product
// palette and NO GRADIENTS: flat neutral grays only. The deliberate product
// look (dark slate, accent blue, control gradients) lives in client/, installed
// via client::ui::ThemeProvider; see client/ui/app_theme.{h,cpp}.
//
// A fallback must still be usable/accessible, so it keeps the full functional
// surface: per-state background/border responses, a visible focus ring outline,
// a checked checkbox mark, opaque text, and a flat dialog panel.
//
// Which patches are populated:
//   base          - always (dense)
//   disabled      - button/input/checkbox (dimmed fill + border)
//   checked       - checkbox / checkbox_mark
//   focus_visible - every focusable role (the ONLY wiring that produces the ring)
//   hover/pressed - every focusable role (flat surface response, NO gradient)

namespace ui {

// ---- neutral fallback tokens (flat grays; intentionally not a brand) -------
namespace {
constexpr float kRadius = 4.f; // small, neutral corner radius

// Control surfaces: flat neutral grays. No gradient anywhere.
constexpr Color kControlBg = {54, 56, 60, 255};
constexpr Color kControlBorder = {84, 88, 96, 255};
constexpr Color kHoverBg = {68, 71, 76, 255};
constexpr Color kHoverBorder = {104, 110, 120, 255};
constexpr Color kPressedBg = {42, 44, 48, 255};
constexpr Color kDisabledBg = {38, 40, 44, 255};
constexpr Color kDisabledBorder = {60, 63, 69, 255};

// Focus ring: a visible neutral 2px ring, outset 2px.
constexpr Color kFocusRing = {150, 160, 175, 255};
} // namespace

const Theme &default_theme() {
  static const Theme t = [] {
    Theme th{};
    th.focus_ring = kFocusRing;
    th.text_default = {210, 210, 214, 255};
    th.text_disabled = {120, 122, 128, 255};
    th.caret = {210, 210, 214, 255};
    th.selection = {150, 160, 175, 96}; // neutral wash; premultiplied at IR emit

    // Focus ring patch: ONLY outline is set; everything else inherits from base.
    const StylePatch focus_ring_patch =
        patch().outline(Outline{2.f, th.focus_ring, 2.f});

    auto seed_control = [&](RoleStyle &r) {
      // Flat neutral fill + 1px neutral border. NO gradient.
      r.base.background = kControlBg;
      r.base.corner_radius = kRadius;
      r.base.border.width = {1, 1, 1, 1};
      r.base.border.color = {kControlBorder, kControlBorder, kControlBorder,
                             kControlBorder};
      // hover: lighter fill + brighter border (NOT a gradient).
      r.hover.background = opt(kHoverBg);
      r.hover.border =
          opt(Border{{1, 1, 1, 1},
                     {kHoverBorder, kHoverBorder, kHoverBorder, kHoverBorder}});
      // pressed: darker fill.
      r.pressed.background = opt(kPressedBg);
      // disabled: dim fill + dim border.
      r.disabled.background = opt(kDisabledBg);
      r.disabled.border = opt(Border{{1, 1, 1, 1},
                                     {kDisabledBorder, kDisabledBorder,
                                      kDisabledBorder, kDisabledBorder}});
      r.focus_visible = focus_ring_patch; // the focus ring
    };
    seed_control(th.button);
    seed_control(th.input);
    seed_control(th.checkbox);

    // The checkbox BODY does not react to `checked` (only the mark does).
    th.checkbox_mark.base.background = {0, 0, 0, 0}; // hidden until checked
    th.checkbox_mark.base.corner_radius = 2.f;
    th.checkbox_mark.checked.background =
        opt(Color{200, 204, 210, 255}); // visible neutral mark

    th.box.base = VisualStyle{};
    th.text.base.text = TextVisual{th.text_default, 0, 14};

    // Dialog: a flat neutral panel with a 1px border. No gradient, no shadow.
    th.dialog.base.background = {40, 42, 46, 248};
    th.dialog.base.corner_radius = kRadius + 2.f;
    th.dialog.base.border.width = {1, 1, 1, 1};
    th.dialog.base.border.color = {{72, 76, 84, 255},
                                   {72, 76, 84, 255},
                                   {72, 76, 84, 255},
                                   {72, 76, 84, 255}};
    return th;
  }();
  return t;
}

} // namespace ui
