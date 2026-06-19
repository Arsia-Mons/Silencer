#include "client/ui/app_theme.h"

// The product theme, installed via ThemeProvider; src/ui keeps only a neutral
// fallback. STRAIGHT alpha throughout; premultiplied at IR emit.

namespace client::ui {

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

constexpr float kRadius = 8.f;

// Accent: drives the focus ring + selection wash.
constexpr Color kAccent = {92, 208, 92, 255}; // #5CD05C green-bright

// Control surface (green phosphor). Sprite-backed controls (oval/chrome)
// override this via image_patch.
constexpr Color kControlTop = {6, 16, 8, 255};      // near-black green glass
constexpr Color kControlBottom = {6, 16, 8, 255};
constexpr Color kControlBorder = {46, 125, 69, 255}; // #2E7D45 green-dim
constexpr Color kHoverTop = {10, 24, 12, 255};
constexpr Color kHoverBottom = {10, 24, 12, 255};
constexpr Color kHoverBorder = {92, 208, 92, 255};   // #5CD05C
constexpr Color kPressedTop = {4, 12, 6, 255};
constexpr Color kPressedBottom = {4, 12, 6, 255};
// Pressed border sits a step BELOW the dim base border so the press reads as
// recessed; a brighter edge (e.g. #3CFF3C) flashes a jarring green rectangle.
constexpr Color kPressedBorder = {37, 107, 60, 255}; // #256B3C green, recessed

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

    // Vertical 2-stop gradient (top -> bottom).
    auto vgrad = [](Color top, Color bottom) {
      Gradient g{};
      g.angle_deg = 90.f;
      g.stop_count = 2;
      g.stops[0] = {0.f, top};
      g.stops[1] = {1.f, bottom};
      return g;
    };

    const StylePatch focus_ring_patch =
        patch().outline(Outline{2.f, th.focus_ring, 2.f});

    auto seed_control = [&](RoleStyle &r) {
      // background = the gradient's bottom stop so a non-gradient executor still
      // lands on-palette.
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
      r.focus_visible = focus_ring_patch;
    };
    seed_control(th.button);
    seed_control(th.input);
    seed_control(th.checkbox);
    // A text field is not a button: drop the pressed patch so click-and-hold
    // doesn't paint a pressed fill.
    th.input.pressed = {};
    // origin caret: legacy palette idx 140, resolved per screen palette —
    // sprite-chrome screens override the color from use_chrome().
    th.input.base.caret = {th.caret, 1.5f};

    // The checkbox BODY does not react to `checked` (only the mark does).
    th.checkbox_mark.base.background = {0, 0, 0, 0}; // hidden until checked
    th.checkbox_mark.base.corner_radius = 4.f;
    th.checkbox_mark.checked.background = opt(kAccent); // visible accent mark

    th.box.base = VisualStyle{};
    th.text.base.text = TextVisual{th.text_default, 0, 11};

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
  // app_theme() is a static const with a stable address read only as const, so
  // hand the context a direct pointer — no per-frame copy of the large Theme.
  return ::ui::provider("ThemeProvider", &::ui::ThemeContext,
                        const_cast<::ui::Theme *>(&app_theme()), children);
}

} // namespace client::ui
