#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <array>
#include <functional>
#include <string>

namespace silencer::client_ui {

constexpr int kLobbyConnectLogLineCount = 15;

struct LobbyConnectLog {
	std::array<const char *, kLobbyConnectLogLineCount> log_lines = {};
};

const LobbyConnectLog& UseLobbyConnectLog();

struct LobbyConnectCredentials {
	const char * username = "";
	const char * password = "";
	bool inactive = false;
	std::function<void(const std::string&)> set_username = {};
	std::function<void(const std::string&)> set_password = {};
	std::function<void()> submit = {};
};

const LobbyConnectCredentials& UseLobbyConnectCredentials();

struct LobbyConnectFrameProps {
	const char * key = nullptr;
};

::ui::UiElement LobbyConnectFrame(const LobbyConnectFrameProps& props);

struct LobbyConnectViewProps {
	const char * key = nullptr;
	const LobbyConnectLog * log = nullptr;
	const LobbyConnectCredentials * credentials = nullptr;
};

::ui::UiElement LobbyConnectView(const LobbyConnectViewProps& props);

}  // namespace silencer::client_ui
