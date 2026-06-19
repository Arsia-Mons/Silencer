#pragma once

#include "ui/components/common.h"

namespace ui::components {

// Scroll-viewport primitive (SIL-111): a clipped window over a taller content
// track, scrolled by wheel + keyboard (arrows / PageUp·Down / Home·End). The
// viewport flex-GROWS to fill its parent (Yoga sizes it); the component learns
// both its own viewport height and the inner content-track height from the
// layout measurement read-back (use_measured_height), then derives all scroll
// math from those two numbers. No caller-supplied heights — drop the ScrollView
// into a container that gives it a bounded height and let it size itself.
struct ScrollViewProps {
  const char *key = nullptr;
  const char *id = nullptr;       // viewport control id (also keys the read-back)
  float step = 32.0f;             // wheel notch / arrow-key line step
  // Uniform row height. When > 0 the viewport VIRTUALIZES: only the children
  // whose row intersects the window (plus 1 row of overscan each side) are
  // rendered, so a 30+ row table stays within the retained-tree node budget.
  // Children must be a flat, uniform-height list in row order. 0 => render all.
  float row_height = 0.0f;
  bool show_scrollbar = true;   // thin right-edge thumb when content overflows
  // Pin the view to the bottom (newest content) and auto-follow as content grows
  // — e.g. a chat transcript. The view sticks to the bottom until the user
  // scrolls up; it re-sticks once they scroll back to the bottom. Off by default.
  bool stick_to_bottom = false;
  ::ui::LayoutStyle layout = {}; // extra viewport layout (width, border, fill)
  ::ui::StyleStatePatch style = {}; // viewport paint overlay over theme.box
  ::ui::UiChildren children = {};
};

::ui::UiElement ScrollView(const ScrollViewProps &props);

} // namespace ui::components
