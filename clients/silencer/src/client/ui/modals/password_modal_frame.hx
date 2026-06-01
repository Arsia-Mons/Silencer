#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace silencer {
namespace client_ui {

struct PasswordModalFrameProps {
	const char * key = nullptr;
	const char * password_display = nullptr;
	std::function<void(const char *)> set_password = {};
	std::function<void(const char *)> submit_password = {};
	std::function<void()> submit = {};
};

::ui::UiElement PasswordModalFrame(const PasswordModalFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
