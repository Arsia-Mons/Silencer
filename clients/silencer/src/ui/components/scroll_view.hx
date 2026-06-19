#pragma once

#include "ui/components/common.h"

namespace ui::components {

// Scroll-viewport primitive: a clipped window over a taller content track,
// scrolled by wheel + keyboard. The viewport flex-grows and learns its own and
// the content-track height via use_measured_height — no caller-supplied heights;
// drop it into a bounded-height container and let it size itself.
struct ScrollViewProps {
  const char *key = nullptr;
  const char *id = nullptr;       // viewport control id (also keys the read-back)
  float step = 32.0f;
  // Uniform row height. When > 0 the viewport VIRTUALIZES: only rows intersecting
  // the window (plus overscan) render, keeping large tables within the node
  // budget. Children must be a flat, uniform-height list in row order.
  float row_height = 0.0f;
  // Per-row heights, parallel to children (children.count entries). When set the
  // viewport VIRTUALIZES with variable row heights (e.g. a wrapping chat
  // transcript): cumulative offsets map the scroll position to the visible row
  // window. Each row's own laid-out height must equal its row_heights entry so
  // layout and the window math agree. Takes precedence over row_height.
  const float *row_heights = nullptr;
  bool show_scrollbar = true;
  // Pin to the bottom and auto-follow growing content (e.g. a chat transcript);
  // unsticks when the user scrolls up, re-sticks on return to the bottom.
  bool stick_to_bottom = false;
  ::ui::LayoutStyle layout = {};
  ::ui::StyleStatePatch style = {};
  ::ui::UiChildren children = {};
};

::ui::UiElement ScrollView(const ScrollViewProps &props);

} // namespace ui::components
