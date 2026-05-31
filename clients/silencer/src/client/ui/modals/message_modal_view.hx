#pragma once

#include "ui/components/common.h"

#include <functional>

namespace silencer::client_ui {

struct MessageModalViewProps {
	const char * key = nullptr;
	const char * message = "";
	bool show_ok = true;
	std::function<void(const ::ui::ActivationEvent&)> on_ok = {};
};

::ui::UiElement MessageModalView(const MessageModalViewProps& props);

}  // namespace silencer::client_ui
