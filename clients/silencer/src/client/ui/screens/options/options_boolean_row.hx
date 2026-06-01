#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace silencer {
namespace client_ui {

struct OptionsBooleanRowProps {
	const char * key = nullptr;
	const char * control_id = nullptr;
	const char * label = nullptr;
	bool selected = false;
	std::function<void()> on_press = {};
};

::ui::UiElement OptionsBooleanRow(const OptionsBooleanRowProps& props);

}  // namespace client_ui
}  // namespace silencer
