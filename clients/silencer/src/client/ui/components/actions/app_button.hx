#pragma once

// AppButton: the one shared semantic button. Adapts ui::components::Button.
// Public props read as product/UI intent (variant/size/label/on_press); host
// details (id_offset/autofocus/LayoutStyle/ActivationEvent) stay inside the impl.

#include "ui/components/common.h" // ::ui::UiChildren, ::ui::UiElement
#include "client/ui/components/actions/app_button_variant.h"

#include <functional>

namespace silencer {

struct AppButtonProps {
  const char *key = nullptr;
  const char *control_id = nullptr;
  int control_offset = 0;
  // Default Secondary == the theme's slate button (empty patch), so existing
  // call sites that omit `variant` keep the baseline look. Primary (accent
  // fill) is opt-in for prominent/primary actions.
  AppButtonVariant variant = AppButtonVariant::Secondary;
  AppButtonSize size = AppButtonSize::Md;
  bool disabled = false;
  bool selected = false;
  bool default_focused = false;
  // Opt in for chrome actions whose origin behavior visibly ramps on hover or
  // real focus. Dialog chrome can stay static when its backdrop owns the wells.
  bool target_feedback = false;
  const char *label = nullptr;
  std::function<void()> on_press = {};
  std::function<void()> on_focus = {};
  std::function<void()> on_hover = {};
  const char *accessibility_label = nullptr;
  // Sparse padding override of the variant's default content insets (label
  // pen). Lists whose BOX is nudged onto a legacy sprite cell use it to keep
  // the label pen on its own golden cell.
  ::ui::Opt<::ui::EdgeSizes> padding = {};
  ::ui::UiChildren children = {};
};

::ui::UiElement AppButton(const AppButtonProps &props);

} // namespace silencer
