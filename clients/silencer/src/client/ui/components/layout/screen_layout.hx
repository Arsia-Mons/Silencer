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
  // Backdrop sprite fit. Origin covers (aspect-preserving centered crop) every
  // menu/overlay backdrop EXCEPT Options-Controls and the lobby cluster, which
  // it stretches — callers replicate that exception, never invent new fits.
  ::ui::ImageFit backdrop_fit = ::ui::ImageFit::Cover;
  ::ui::UiChildren children = {};
};

::ui::UiElement ScreenLayout(const ScreenLayoutProps &props);

} // namespace silencer
