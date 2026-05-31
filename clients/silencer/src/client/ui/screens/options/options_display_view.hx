#pragma once

#include "ui/components/common.h"

#include <functional>

namespace silencer::client_ui {

struct OptionsDisplayViewProps {
	const char * key = nullptr;
	bool fullscreen = false;
	bool smooth_scaling = false;
	std::function<void(bool)> on_fullscreen = {};
	std::function<void(bool)> on_smooth_scaling = {};
	std::function<void(const ::ui::ActivationEvent&)> on_save = {};
	std::function<void(const ::ui::ActivationEvent&)> on_cancel = {};
};

::ui::UiElement OptionsDisplayView(const OptionsDisplayViewProps& props);

}  // namespace silencer::client_ui
