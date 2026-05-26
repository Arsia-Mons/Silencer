#pragma once

#include "ui/focus/UiFocus.h"

#include <functional>

namespace silencer::ui::primitives {

using FocusableRender = std::function<void(const silencer::ui::UiFocusableState& focus)>;

struct FocusableProps {
	Clay_ElementId id = {};
	bool disabled = false;
	silencer::ui::UiNavRules nav = {};
	std::function<void()> onConfirm = {};
	std::function<void()> onFocus = {};
};

void Focusable(const FocusableProps& props, FocusableRender render);

}  // namespace silencer::ui::primitives
