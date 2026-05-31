#pragma once

#include "ui/components/common.h"

namespace silencer::client_ui {

struct MainMenuViewProps {
	const char * key = nullptr;
	const char * version = "";
};

::ui::UiElement MainMenuView(const MainMenuViewProps& props);

}  // namespace silencer::client_ui
