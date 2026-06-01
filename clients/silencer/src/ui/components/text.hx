#pragma once

#include "ui/components/common.h"

namespace ui {
namespace components {

struct TextProps {
	const char * key = nullptr;
	const char * id = nullptr;
	int id_offset = 0;
	bool disabled = false;
	bool focusable = false;
	bool autofocus = false;
	AccessibilityProps accessibility = {};
	std::function<void(const ::ui::FocusEvent&)> on_focus = {};
	std::function<void(const ::ui::BlurEvent&)> on_blur = {};
	std::function<void(const ::ui::ActivationEvent&)> on_activate = {};
	std::function<void(const ::ui::KeyEvent&)> on_key = {};
	std::function<void(const ::ui::TextInputEvent&)> on_text_input = {};
	std::function<void(const ::ui::TextEditingEvent&)> on_text_editing = {};
	const char * value = nullptr;
	::ui::LayoutStyle layout = {};
	::ui::StyleStatePatch style = {};
};

::ui::UiElement Text(const TextProps& props);

}  // namespace components
}  // namespace ui
