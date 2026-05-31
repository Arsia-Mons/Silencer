#pragma once

#include "ui/components/common.h"

namespace silencer::client_ui {

struct OptionsMenuViewProps {
	const char * key = nullptr;
};

::ui::UiElement OptionsMenuView(const OptionsMenuViewProps& props);

}  // namespace silencer::client_ui
