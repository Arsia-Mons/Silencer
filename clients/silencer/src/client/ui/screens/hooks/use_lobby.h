#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class ScreenContext;

namespace silencer::client_ui::hooks {

struct LobbyMissionSummaryActions {
	int upgradeIndex = -1;
	bool done = false;
};

struct LobbyGameJoinActions {
	bool ready = false;
	bool changeTeam = false;
	bool chooseTech = false;
};

struct LobbyGameTechActions {
	int toggleIndex = -1;
	bool backToTeams = false;
};

struct LobbyGameSelectActions {
	bool create = false;
	bool join = false;
	bool spectate = false;
};

struct LobbyTechItemDetails {
	bool found = false;
	std::string title;
	std::array<std::string, 8> descriptionLines{};
};

struct LobbyUi {
	bool authSent = false;
	std::function<void(std::string username, std::string password)> submitCredentials = {};
	std::function<void(std::shared_ptr<LobbyMissionSummaryActions>)> flushMissionSummaryActions = {};
	std::function<void(std::shared_ptr<LobbyGameJoinActions>,
	                   std::function<bool()> gameJoinStillActive,
	                   std::function<void()> showTech)> flushGameJoinActions = {};
	std::function<LobbyTechItemDetails(int itemIndex)> techItemDetailsForIndex = {};
	std::function<void(std::shared_ptr<LobbyGameTechActions>,
	                   std::function<bool()> gameTechStillActive,
	                   std::function<void()> showTeams)> flushGameTechActions = {};
	std::function<void(std::shared_ptr<LobbyGameSelectActions>,
	                   std::function<bool()> gameSelectStillActive,
	                   std::function<uint32_t()> selectedGameId,
	                   std::function<void()> showCreate)> flushGameSelectActions = {};
};

void LobbyProvider(ScreenContext & ctx, const std::function<void()> & children);
LobbyUi UseLobby();

} // namespace silencer::client_ui::hooks
