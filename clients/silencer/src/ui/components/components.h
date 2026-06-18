#pragma once

#include "box.h"
#include "button.h"
#include "checkbox.h"
#include "dialog.h"
#include "input.h"
#include "scroll_view.h"
#include "text.h"

namespace ui::components::elements {

inline ::ui::UiElement Box(const BoxProps &props) {
  return ::ui::component("Box", props, ::ui::components::Box);
}

inline ::ui::UiElement Button(const ButtonProps &props) {
  return ::ui::component("Button", props, ::ui::components::Button);
}

inline ::ui::UiElement Checkbox(const CheckboxProps &props) {
  return ::ui::component("Checkbox", props, ::ui::components::Checkbox);
}

inline ::ui::UiElement Dialog(const DialogProps &props) {
  return ::ui::component("Dialog", props, ::ui::components::Dialog);
}

inline ::ui::UiElement Input(const InputProps &props) {
  return ::ui::component("Input", props, ::ui::components::Input);
}

inline ::ui::UiElement ScrollView(const ScrollViewProps &props) {
  return ::ui::component("ScrollView", props, ::ui::components::ScrollView);
}

inline ::ui::UiElement Text(const TextProps &props) {
  return ::ui::component("Text", props, ::ui::components::Text);
}

} // namespace ui::components::elements
