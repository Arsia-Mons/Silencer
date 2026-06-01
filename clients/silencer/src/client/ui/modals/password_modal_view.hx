#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>
#include <string>

namespace silencer::client_ui {

struct PasswordModalState {
	const char * password = "";
};

struct PasswordModalActions {
	std::function<void(const std::string&)> set_password = {};
	std::function<void()> submit = {};
};

struct PasswordModalContextValue {
	PasswordModalState state = {};
	PasswordModalActions actions = {};
};

const PasswordModalContextValue& UsePasswordModal();

struct PasswordModalFrameProps {
	const char * key = nullptr;
};

::ui::UiElement PasswordModalFrame(const PasswordModalFrameProps& props);

struct PasswordModalViewProps {
	const char * key = nullptr;
	const PasswordModalContextValue * value = nullptr;
};

::ui::UiElement PasswordModalView(const PasswordModalViewProps& props);

}  // namespace silencer::client_ui
