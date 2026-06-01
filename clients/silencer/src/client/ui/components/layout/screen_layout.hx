#pragma once

#include "ui/components/common.h"

namespace silencer {

// Full-viewport screen root. All four appearances are the same concept (a
// screen/page frame) differing only by an app-approved style+paint bundle, so
// they are variants rather than separate components. Menu/Game adapt a Box;
// Overlay/CenteredOverlay adapt a (modal) Dialog. The Box-vs-Dialog choice and
// the modal flag are derived from the variant -- never a caller knob.
enum class ScreenLayoutVariant { Menu, Game, Overlay, CenteredOverlay };

struct ScreenLayoutProps {
  const char *key = nullptr;
  ScreenLayoutVariant variant = ScreenLayoutVariant::Menu;
  ::ui::UiChildren children = {};
};

::ui::UiElement ScreenLayout(const ScreenLayoutProps &props);

} // namespace silencer
