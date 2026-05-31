#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <array>
#include <functional>
#include <string>

namespace silencer::client_ui {

constexpr int kLobbyConnectLogLineCount = 15;

struct LobbyConnectState {
	std::array<const char *, kLobbyConnectLogLineCount> log_lines = {};
	const char * username = "";
	const char * password = "";
	bool inactive = false;
};

struct LobbyConnectActions {
	std::function<void(const std::string&)> set_username = {};
	std::function<void(const std::string&)> set_password = {};
	std::function<void()> submit = {};
	std::function<void()> cancel = {};
};

struct LobbyConnectContextValue {
	LobbyConnectState state = {};
	LobbyConnectActions actions = {};
};

const LobbyConnectContextValue& UseLobbyConnect();

struct LobbyConnectViewProps {
	const char * key = nullptr;
	const LobbyConnectContextValue * value = nullptr;
};

::ui::UiElement LobbyConnectView(const LobbyConnectViewProps& props);

}  // namespace silencer::client_ui
