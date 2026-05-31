#pragma once

#include "tree.h"

namespace ui {

struct LayoutViewport {
  float width = 0.0f;
  float height = 0.0f;
};

using FlexLayoutFn = bool (*)(UiTree &tree, NodeId root_id,
                              LayoutViewport viewport, void *user);

struct FlexLayoutAdapter {
  const char *name = "";
  FlexLayoutFn compute = nullptr;
  void *user = nullptr;
};

bool compute_flex_layout(const FlexLayoutAdapter &adapter, UiTree &tree,
                         LayoutViewport viewport);

} // namespace ui
