#pragma once

#include "ui/components/common.h"

namespace silencer {

// Full-viewport screen root. All appearances are the same concept (a
// screen/page frame) differing only by an app-approved style+paint bundle, so
// they are variants rather than separate components. Menu/Game adapt a Box;
// Overlay/CenteredOverlay adapt a (modal) Dialog. Hud is a transparent Box that
// floats readouts over the live world (the UI frame composites over the world,
// so its background must clear). The Box-vs-Dialog choice and the modal flag are
// derived from the variant -- never a caller knob.
enum class ScreenLayoutVariant { Menu, Game, Overlay, CenteredOverlay, Hud };

struct ScreenLayoutProps {
  const char *key = nullptr;
  ScreenLayoutVariant variant = ScreenLayoutVariant::Menu;
  // Origin's one backdrop-fit exception: Options·Controls stretches the
  // starfield (PackImageStretch(6,0)) where every other menu covers it. The
  // fit is baked into the chrome texture, so this only picks which bake.
  bool stretched_backdrop = false;
  ::ui::UiChildren children = {};
};

::ui::UiElement ScreenLayout(const ScreenLayoutProps &props);

} // namespace silencer
