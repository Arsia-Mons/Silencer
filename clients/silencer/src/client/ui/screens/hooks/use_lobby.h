#pragma once

#include <functional>
#include <memory>
#include <string>

class ScreenContext;

namespace silencer::client_ui::hooks {

struct LobbyMissionSummaryActions {
	int upgradeIndex = -1;
	bool done = false;
};

struct LobbyUi {
	bool authSent = false;
	std::function<void(std::string username, std::string password)> submitCredentials = {};
	std::function<void(std::shared_ptr<LobbyMissionSummaryActions>)> flushMissionSummaryActions = {};
};

void LobbyProvider(ScreenContext & ctx, const std::function<void()> & children);
LobbyUi UseLobby();

} // namespace silencer::client_ui::hooks
