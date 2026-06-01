#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace silencer {
namespace client_ui {

struct LobbyConnectFrameProps {
	const char * key = nullptr;
	const char * log_text = nullptr;
	const char * username_display = nullptr;
	const char * password_display = nullptr;
	bool inactive = false;
	std::function<void(const char *)> set_username = {};
	std::function<void(const char *)> set_password = {};
	std::function<void()> login = {};
	std::function<void()> cancel = {};
};

::ui::UiElement LobbyConnectFrame(const LobbyConnectFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
