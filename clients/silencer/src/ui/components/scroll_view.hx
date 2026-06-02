#pragma once

#include "ui/components/common.h"

namespace ui::components {

// Scroll-viewport primitive (SIL-111): a clipped window over a taller content
// track, scrolled by wheel + keyboard (arrows / PageUp·Down / Home·End). The
// caller supplies the window height and the total content height (both known
// for row-based lists: rows * row_height) so the offset clamps to
// [0, max(0, content_height - viewport_height)] with no per-frame measurement.
struct ScrollViewProps {
  const char *key = nullptr;
  const char *id = nullptr;
  float viewport_height = 0.0f; // the clipped window height (points)
  float content_height = 0.0f;  // total content extent (points)
  float step = 32.0f;           // wheel notch / arrow-key line step
  // Uniform row height. When > 0 the viewport VIRTUALIZES: only the children
  // whose row intersects the window (plus 1 row of overscan each side) are
  // rendered, so a 30+ row table stays within the retained-tree node budget.
  // Children must be a flat, uniform-height list in row order. 0 => render all.
  float row_height = 0.0f;
  bool show_scrollbar = true;   // thin right-edge thumb when content overflows
  ::ui::LayoutStyle layout = {}; // extra viewport layout (width, border, fill)
  ::ui::StyleStatePatch style = {}; // viewport paint overlay over theme.box
  ::ui::UiChildren children = {};
};

::ui::UiElement ScrollView(const ScrollViewProps &props);

} // namespace ui::components
