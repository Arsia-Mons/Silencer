#pragma once

#include "ui/components/common.h"

#include <functional>

namespace silencer::client_ui {

struct OptionsMenuViewProps {
	const char * key = nullptr;
	std::function<void(const ::ui::ActivationEvent&)> on_controls = {};
	std::function<void(const ::ui::ActivationEvent&)> on_display = {};
	std::function<void(const ::ui::ActivationEvent&)> on_audio = {};
	std::function<void(const ::ui::ActivationEvent&)> on_back = {};
};

::ui::UiElement OptionsMenuView(const OptionsMenuViewProps& props);

}  // namespace silencer::client_ui
