#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

struct LobbyGameJoinRosterRow {
	bool ready = false;
	uint8_t agency = 0;
	uint8_t teamNumber = 0;
	uint8_t peerSlot = 0;
	bool drawEmblem = false;
	std::string name;
	std::string level;
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

struct LobbyCharacterStats {
	std::string name;
	bool statsAvailable = false;
	bool maxLevel = false;
	uint16_t wins = 0;
	uint16_t losses = 0;
	uint16_t xpToNextLevel = 0;
	uint8_t level = 0;
	uint8_t endurance = 0;
	uint8_t shield = 0;
	uint8_t jetpack = 0;
	uint8_t techslots = 0;
	uint8_t hacking = 0;
	uint8_t contacts = 0;
};

struct LobbyUi {
	bool authSent = false;
	uint8_t selectedAgency = 0;
	bool agentSelectionLocked = false;
	std::function<void(std::string username, std::string password)> submitCredentials = {};
	std::function<void(std::shared_ptr<LobbyMissionSummaryActions>)> flushMissionSummaryActions = {};
	std::function<void(std::shared_ptr<LobbyGameJoinActions>,
	                   std::function<bool()> gameJoinStillActive,
	                   std::function<void()> showTech)> flushGameJoinActions = {};
	std::function<LobbyTechItemDetails(int itemIndex)> techItemDetailsForIndex = {};
	std::function<LobbyCharacterStats(uint8_t agency)> characterStatsForAgency = {};
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
std::string UseLobbyGameJoinReadyLabel();
std::vector<LobbyGameJoinRosterRow> UseLobbyGameJoinRosterRows();
void ReconcileLobbyCharacterAgency(ScreenContext & ctx, int & lastSyncedAgency);
void FlushLobbyCharacterSelectionRequest(ScreenContext & ctx, bool & requested);

} // namespace silencer::client_ui::hooks
