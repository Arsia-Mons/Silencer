#pragma once

#include "flex_layout.h"

namespace ui {

enum class YogaErrata : uint32_t {
  None = 0,
  StretchFlexBasis = 1,
  AbsolutePositionWithoutInsetsExcludesPadding = 2,
  AbsolutePercentAgainstInnerSize = 4,
  Classic = 2147483646,
  All = 2147483647,
};

inline YogaErrata operator|(YogaErrata left, YogaErrata right) {
  return static_cast<YogaErrata>(static_cast<uint32_t>(left) |
                                 static_cast<uint32_t>(right));
}

struct YogaLayoutConfig {
  bool use_web_defaults = false;
  StyleFloat point_scale_factor = {};
  bool has_errata = false;
  YogaErrata errata = YogaErrata::None;
  bool experimental_web_flex_basis = false;
  LayoutDirection owner_direction = LayoutDirection::Ltr;
};

FlexLayoutAdapter make_yoga_flex_layout_adapter();
FlexLayoutAdapter make_yoga_flex_layout_adapter(const YogaLayoutConfig *config);

} // namespace ui
