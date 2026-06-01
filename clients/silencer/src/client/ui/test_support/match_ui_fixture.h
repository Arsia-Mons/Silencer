#pragma once

#include <string>

class World;

namespace silencer {
namespace client_ui {

enum class MatchUiFixtureMode {
	Clear,
	Status,
	Chat,
	Buy,
	Tech,
	PlayerList,
	QuitPrompt,
	TopMessage,
	Message,
	StatusLine,
	All,
};

struct MatchUiFixtureResult {
	MatchUiFixtureMode mode = MatchUiFixtureMode::Status;
	bool available = false;
	std::string error;
	bool chatActive = false;
	std::string chatDraft;
	bool buyActive = false;
	bool techActive = false;
	int showChatTicks = 0;
	bool showPlayerList = false;
	int buyItemCount = 0;
	int techItemCount = 0;
	int buySelectedIndex = 0;
	int techSelectedIndex = 0;
	int quitState = 0;
	int topMessageProgress = 0;
	int messageProgress = 0;
	int statusMessageCount = 0;
};

MatchUiFixtureResult ConfigureMatchUiFixture(
	World& world,
	int viewed_peer_id,
	MatchUiFixtureMode mode);

}  // namespace client_ui
}  // namespace silencer
