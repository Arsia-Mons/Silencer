#pragma once

#include "ui/components/common.h"

#include <string>

namespace ui::components {

struct InputProps {
  const char *key = nullptr;
  const char *id = nullptr;
  int id_offset = 0;
  bool disabled = false;
  bool focusable = false;
  bool autofocus = false;
  AccessibilityProps accessibility = {};
  std::function<void(const ::ui::FocusEvent &)> on_focus = {};
  std::function<void(const ::ui::BlurEvent &)> on_blur = {};
  std::function<void(const ::ui::ActivationEvent &)> on_activate = {};
  std::function<void(const ::ui::KeyEvent &)> on_key = {};
  std::function<void(const ::ui::TextInputEvent &)> on_text_input = {};
  std::function<void(const ::ui::TextEditingEvent &)> on_text_editing = {};
  const char *value = "";
  std::function<void(const std::string &)> on_change = {};
  ::ui::LayoutStyle layout = {
      .align_items = ::ui::AlignItems::Stretch,
      .justify_content = ::ui::JustifyContent::Center,
      .width = ::ui::Length::points(220.0f),
      .height = ::ui::Length::points(36.0f),
      .padding = {8.0f, 8.0f, 8.0f, 8.0f},
  };
  ::ui::StyleStatePatch style = {}; // per-state paint overlay over theme.input
};

::ui::UiElement Input(const InputProps &props);

} // namespace ui::components
