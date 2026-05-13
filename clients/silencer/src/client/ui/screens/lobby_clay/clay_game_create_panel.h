#ifndef SILENCER_UI_LOBBY_CLAY_GAME_CREATE_PANEL_H
#define SILENCER_UI_LOBBY_CLAY_GAME_CREATE_PANEL_H

// Screen-side Clay reimplementation of the lobby GameCreatePanel — the
// game-options form on the right pane. Composes Panel + LabelValueRow +
// TextInput + BankButton::Inline (security/spectatable cyclers) +
// ScrollList (map list) + BankButton::Chrome (Create).
//
// Domain glue (CreateGame kickoff, Config persistence, async map upload)
// lives in the screen-side GameCreatePanelTick. Primitives stay screen-
// agnostic.

#include "shared.h"

#include <string>
#include <vector>

class World;
class Resources;
class ScreenContext;
class LobbyClayScreen;

namespace silencer::ui::lobby_clay {

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

	// Per-frame click flags. Set by Clay onClick adapters; consumed once
	// by GameCreatePanelTick on the next frame.
	bool securityClicked    = false;
	bool spectatableClicked = false;
	bool createClicked      = false;
	int  mapRowClickedIndex = -1;
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
                         LobbyClayScreen & owner);

// Emits the upper-pane subtree ("Game Options" heading + 6-row form: security
// cycler, min/max level, max players, max teams, spectatable). Called inside
// the LobbyRightUpperBox CLAY block; flex children only (no floating).
// BeginFrame requirements: BankTextBeginFrame, BankButtonBeginFrame,
// TextInputBeginFrame.
void BuildGameCreateUpperTree(GameCreatePanelState & state,
                              Resources & resources);

// Emits the tall-pane subtree ("Select Map" heading + map list + game-name +
// password inputs + Create button). Called inside the LobbyRightTallBox CLAY
// block; flex children only.
// BeginFrame requirements: BankTextBeginFrame, BankButtonBeginFrame,
// ScrollListBeginFrame, TextInputBeginFrame.
void BuildGameCreateTallTree(GameCreatePanelState & state,
                             Resources & resources);

}  // namespace silencer::ui::lobby_clay

#endif
