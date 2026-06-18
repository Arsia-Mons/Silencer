#pragma once

#include "ui/components/common.h"

namespace ui::components {

struct ButtonProps {
  const char *key = nullptr;
  const char *id = nullptr;
  int id_offset = 0;
  bool disabled = false;
  bool focusable = false;
  bool autofocus = false;
  AccessibilityProps accessibility = {};
  std::function<void(const ::ui::FocusEvent &)> on_focus = {};
  std::function<void(const ::ui::BlurEvent &)> on_blur = {};
  std::function<void(const ::ui::HoverEvent &)> on_hover = {};
  std::function<void(const ::ui::KeyEvent &)> on_key = {};
  std::function<void(const ::ui::WheelEvent &)> on_wheel = {};
  std::function<void(const ::ui::TextInputEvent &)> on_text_input = {};
  std::function<void(const ::ui::TextEditingEvent &)> on_text_editing = {};
  const char *label = nullptr;
  std::function<void(const ::ui::ActivationEvent &)> on_activate = {};
  ::ui::LayoutStyle layout = {
      .align_items = ::ui::AlignItems::Center,
      .justify_content = ::ui::JustifyContent::Center,
      .width = ::ui::Length::points(132.0f),
      .height = ::ui::Length::points(38.0f),
      .padding = {14.0f, 14.0f, 8.0f, 8.0f},
  };
  ::ui::StyleStatePatch style = {}; // per-state paint overlay over theme.button
  // `children` is declared LAST (after layout/style), matching BoxProps and
  // DialogProps, so JSX child lowering (which always appends `.children` last)
  // satisfies -Werror=reorder-init-list. Keep it last when adding fields.
  ::ui::UiChildren children = {};
};

::ui::UiElement Button(const ButtonProps &props);

} // namespace ui::components
