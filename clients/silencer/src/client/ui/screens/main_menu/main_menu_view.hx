#pragma once

#include "ui/components/common.h"

#include <functional>

namespace silencer::client_ui {

struct MainMenuViewProps {
	const char * key = nullptr;
	const char * version = "";
	std::function<void(const ::ui::ActivationEvent&)> on_tutorial = {};
	std::function<void(const ::ui::ActivationEvent&)> on_lobby = {};
	std::function<void(const ::ui::ActivationEvent&)> on_options = {};
	std::function<void(const ::ui::ActivationEvent&)> on_exit = {};
};

::ui::UiElement MainMenuView(const MainMenuViewProps& props);

}  // namespace silencer::client_ui
