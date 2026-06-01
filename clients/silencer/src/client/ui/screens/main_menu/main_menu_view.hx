#pragma once

#include "ui/components/common.h"

class Resources;

namespace silencer::client_ui {

struct MainMenuViewProps {
	const char * key = nullptr;
	const char * version = "";
	const Resources * resources = nullptr;
};

::ui::UiElement MainMenuView(const MainMenuViewProps& props);

}  // namespace silencer::client_ui
