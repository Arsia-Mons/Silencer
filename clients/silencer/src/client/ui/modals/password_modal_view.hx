#pragma once

#include "ui/components/common.h"

#include <functional>
#include <string>

namespace silencer::client_ui {

struct PasswordModalViewProps {
	const char * key = nullptr;
	const char * password = "";
	std::function<void(const std::string&)> on_password_change = {};
	std::function<void(const ::ui::ActivationEvent&)> on_submit = {};
};

::ui::UiElement PasswordModalView(const PasswordModalViewProps& props);

}  // namespace silencer::client_ui
