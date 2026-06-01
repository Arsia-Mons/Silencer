#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_CREATE_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_CREATE_PANEL_H

// Screen-side lobby GameCreatePanel state and domain glue. The cppx lobby view
// owns retained composition; this file owns CreateGame kickoff, Config
// persistence, and async map upload.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <string>
#include <vector>

class World;
class ScreenContext;
class LobbyScreen;

namespace silencer::client_ui::lobby {

constexpr const char * kGameCreateOptionsScrollId = "lobby.game_create.options";
constexpr const char * kGameCreateOptionsScrollLabel = "Game Options Form";
constexpr int kGameCreateVisibleMapRows = 7;

struct GameCreatePanelState {
	// Form fields — buffers sized to match the legacy TextInput maxchars.
	char name[36]      = {0};   // legacy maxchars = 35.
	char password[21]  = {0};   // legacy maxchars = 20.
	char minLevel[3]   = "0";   // legacy maxchars = 2.
	char maxLevel[3]   = "99";
	char maxPlayers[3] = "24";
	char maxTeams[3]   = "6";

	// Security cycler: 0=Off, 1=Low, 2=Medium (default), 3=High.
	Uint8 securityIndex = 2;
	bool  spectatable   = true;  // hydrated from Config::lastspectatable on init.

	// Map list snapshot. Built once during Init from the local level dirs
	// + community server list. Display strings include any "[DL] " prefix
	// for remote-only maps.
	std::vector<std::string> maps;
	int    mapSelectedIndex = -1;
	Uint16 mapScrollPos     = 0;

	// Per-frame click flags. Set by retained callbacks; consumed once
	// by GameCreatePanelTick on the next frame.
	bool securityClicked    = false;
	bool spectatableClicked = false;
	bool createClicked      = false;
	int  mapRowClickedIndex = -1;

	// Upper "Game Options" viewport state. The panel owns the visible-row
	// window and applies wheel/control-socket scroll in row units.
	Uint16 optionsScrollPosition = 0;
	int    optionsScrollDelta = 0;
	Uint16 optionsMaxScroll = 0;
	Uint8  optionsVisibleRows = 0;

};

// Hydrate state from Config (defaultgamename, lastspectatable) and rebuild
// the map list from disk + the community map API. Mirrors the legacy
// GameCreatePanel::Build's one-time setup.
void GameCreatePanelInit(GameCreatePanelState & state, ScreenContext & ctx);

// Per-frame pump. Consumes click flags (cycle security, toggle spectatable,
// kick off Create). Also pumps the deferred CreateGame state machine
// (map upload → CreateGame → CONNECTED → ShowGameJoin handoff +
// progress-modal spinner update + create-failure unwind). Mirrors the
// legacy LobbyScreen::Tick's `if(gameCreate)` block + GameCreatePanel::Tick
// button switch.
void GameCreatePanelTick(GameCreatePanelState & state,
                         World & world,
                         ScreenContext & ctx,
                         LobbyScreen & owner);
bool GameCreatePanelHandleUiIntent(GameCreatePanelState & state,
                                   const silencer::ui::UiAction & action);
void GameCreatePanelSelectMap(GameCreatePanelState & state, int index);
void GameCreatePanelScrollMaps(GameCreatePanelState & state, int delta);
void GameCreatePanelCycleSecurity(GameCreatePanelState & state);
void GameCreatePanelToggleSpectatable(GameCreatePanelState & state);
void GameCreatePanelRequestCreate(GameCreatePanelState & state);
void GameCreatePanelSetName(GameCreatePanelState & state, const std::string & value);
void GameCreatePanelSetPassword(GameCreatePanelState & state, const std::string & value);
void GameCreatePanelSetMinLevel(GameCreatePanelState & state, const std::string & value);
void GameCreatePanelSetMaxLevel(GameCreatePanelState & state, const std::string & value);
void GameCreatePanelSetMaxPlayers(GameCreatePanelState & state, const std::string & value);
void GameCreatePanelSetMaxTeams(GameCreatePanelState & state, const std::string & value);

}  // namespace silencer::client_ui::lobby

#endif
