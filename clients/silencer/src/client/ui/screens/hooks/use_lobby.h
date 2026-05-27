#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class ScreenContext;

namespace silencer::client_ui::hooks {

struct LobbyGameJoinRosterRow {
	bool ready = false;
	uint8_t agency = 0;
	uint8_t teamNumber = 0;
	uint8_t peerSlot = 0;
	bool drawEmblem = false;
	std::string name;
	std::string level;
};

struct LobbyTechItemDetails {
	bool found = false;
	std::string title;
	std::array<std::string, 8> descriptionLines{};
};

struct LobbyTechGridCell {
	bool draw = false;
	bool selected = false;
	uint8_t brightness = 64;
};

struct LobbyTechGridRow {
	int itemIndex = -1;
	std::array<LobbyTechGridCell, 4> cells{};
	std::string label;
	uint8_t labelBrightness = 64;
};

struct LobbyTechSnapshot {
	std::string slotsLeft;
	std::array<std::string, 3> peerNames{};
	std::vector<LobbyTechGridRow> rows;
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
	std::function<void(int upgradeIndex)> upgradeMissionSummaryStat = {};
	std::function<void()> completeMissionSummary = {};
	std::function<void()> sendGameJoinReady = {};
	std::function<void()> changeGameJoinTeam = {};
	std::function<void()> beginGameTechSelection = {};
	std::function<LobbyCharacterStats(uint8_t agency)> characterStatsForAgency = {};
	std::function<void(int itemIndex)> toggleGameTechChoice = {};
	std::function<void(uint32_t gameId)> joinLobbyGame = {};
	std::function<void(uint32_t gameId)> spectateLobbyGame = {};
};

void LobbyProvider(ScreenContext & ctx, const std::function<void()> & children);
LobbyUi UseLobby();
std::string UseLobbyGameJoinReadyLabel();
std::vector<LobbyGameJoinRosterRow> UseLobbyGameJoinRosterRows();
LobbyTechSnapshot UseLobbyGameTechSnapshot();
LobbyTechItemDetails UseLobbyTechItemDetails(int itemIndex);
void ReconcileLobbyCharacterAgency(ScreenContext & ctx, int & lastSyncedAgency);
void FlushLobbyCharacterSelectionRequest(ScreenContext & ctx, bool & requested);
void RequestLobbyGameTechPeerList(ScreenContext & ctx);

} // namespace silencer::client_ui::hooks
