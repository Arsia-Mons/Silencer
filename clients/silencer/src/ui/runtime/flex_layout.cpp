#include "flex_layout.h"

namespace ui {

bool compute_flex_layout(const FlexLayoutAdapter &adapter, UiTree &tree,
                         LayoutViewport viewport) {
  if (!adapter.compute)
    return false;

  if (!tree.contains(tree.root_id()))
    return false;

  return adapter.compute(tree, tree.root_id(), viewport, adapter.user);
}

} // namespace ui
