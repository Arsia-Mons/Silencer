#pragma once

#include "ui/components/common.h"

namespace silencer {

// Surface panel: a bordered/filled container that holds screen content.
// Variant selects the app-approved appearance bundle (background, border,
// padding, gap, alignment, and canonical width). Size optionally overrides the
// canonical width with one of the design-system width tiers.
enum class PanelVariant { Hero, Overlay, Sunken };

// Auto = the variant's canonical width. Sm/Md/Lg are the three design-system
// width tiers (220 / 260 / 340 points), matching the canonical widths of the
// Overlay / Sunken / Hero variants respectively.
enum class PanelSize { Auto, Sm, Md, Lg };

struct PanelProps {
  const char *key = nullptr;
  PanelVariant variant = PanelVariant::Overlay;
  PanelSize size = PanelSize::Auto;
  ::ui::UiChildren children = {};
};

::ui::UiElement Panel(const PanelProps &props);

} // namespace silencer
