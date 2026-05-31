#pragma once

#include "ui/components/common.h"

#include <array>
#include <functional>
#include <string>

namespace silencer::client_ui {

constexpr int kLobbyConnectLogLineCount = 15;

struct LobbyConnectViewProps {
	const char * key = nullptr;
	std::array<const char *, kLobbyConnectLogLineCount> log_lines = {};
	const char * username = "";
	const char * password = "";
	bool inactive = false;
	std::function<void(const std::string&)> on_username_change = {};
	std::function<void(const std::string&)> on_password_change = {};
	std::function<void(const ::ui::ActivationEvent&)> on_login = {};
	std::function<void(const ::ui::ActivationEvent&)> on_cancel = {};
};

::ui::UiElement LobbyConnectView(const LobbyConnectViewProps& props);

}  // namespace silencer::client_ui
