#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>
#include <string>

namespace silencer::client_ui {

struct PasswordModalCredentials {
	const char * password = "";
	std::function<void(const std::string&)> set_password = {};
	std::function<void()> submit = {};
};

const PasswordModalCredentials& UsePasswordModalCredentials();

struct PasswordModalFrameProps {
	const char * key = nullptr;
};

::ui::UiElement PasswordModalFrame(const PasswordModalFrameProps& props);

struct PasswordModalViewProps {
	const char * key = nullptr;
	const PasswordModalCredentials * credentials = nullptr;
};

::ui::UiElement PasswordModalView(const PasswordModalViewProps& props);

}  // namespace silencer::client_ui
