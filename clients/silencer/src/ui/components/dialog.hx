#pragma once

#include "ui/components/common.h"

namespace ui::components {

struct DialogProps {
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
  bool modal = true;
  ::ui::LayoutStyle layout = {};
  ::ui::StyleStatePatch style = {}; // per-state paint overlay over theme.dialog
  ::ui::UiChildren children = {};
};

::ui::UiElement Dialog(const DialogProps &props);

} // namespace ui::components
