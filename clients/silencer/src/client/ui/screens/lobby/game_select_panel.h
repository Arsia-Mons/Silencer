#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H

// Screen-side lobby GameSelectPanel: the always-on right-side games list
// surface (active when no Create/Join/Tech panel is up). Composes ScrollList
// + BankText + BankButton primitives and owns the per-frame info-block
// strings + the Join/Spectate/Create button click flags. Domain glue
// (JoinGame / SpectateGame / level checks / password modal / ShowGameCreate)
// lives here in the screen; primitives stay screen-agnostic.

#include "shared.h"

#include <string>
#include <vector>

class World;
class Resources;
class ScreenContext;
class LobbyScreen;

namespace silencer::client_ui::lobby {

struct GameSelectPanelState {
	// Snapshot of the games list. Rebuilt from world.lobby.games whenever
	// world.lobby.gamesprocessed flips false. Held in screen-side storage
	// so the layout pass can hold pointers into the std::strings safely.
	struct Row {
		std::string name;
		Uint32      gameid = 0;
	};
	std::vector<Row> rows;
	int    selectedIndex = -1;  // -1 = no selection.
	Uint16 scrollPos     = 0;

	// Per-frame click flags. Set by widget onClick adapters; consumed
	// once by GameSelectPanelTick on the next frame.
	bool   joinClicked     = false;
	bool   spectateClicked = false;
	bool   createClicked   = false;

	// Per-row click flag — the row that was last pressed. -1 = none.
	int    rowClickedIndex = -1;

	// Cached info-block strings (recomputed each Tick from the selected
	// game). Static-lifetime so the layout pass can hold raw c_str()s.
	std::string infoName;
	std::string infoMap;
	std::string infoSecurity;
	std::string infoCreator;
	std::string infoLimits;

	// Cached visibility for Join/Spectate (recomputed each Tick).
	bool joinVisible     = false;
	bool spectateVisible = false;
};

// One-time init. Clears state. The legacy panel ran a one-time games-list
// rebuild on Build; we just clear here — the first Tick that observes
// `gamesprocessed=false` will populate from world.lobby.games.
void GameSelectPanelInit(GameSelectPanelState & state);

// Per-frame pump:
//   - Rebuilds `rows` from world.lobby.games whenever
//     `world.lobby.gamesprocessed == false`.
//   - Recomputes infoName/infoMap/.../joinVisible/spectateVisible from the
//     selected game (or clears them when no game is selected).
//   - Consumes joinClicked / spectateClicked / createClicked flags — runs
//     the legacy GameSelectPanel::Tick's Join/Spectate flows (level checks,
//     password modal, JoinGame/SpectateGame) and the Create flow (calls
//     owner.ShowGameCreate).
void GameSelectPanelTick(GameSelectPanelState & state,
                         World & world,
                         ScreenContext & ctx,
                         LobbyScreen & owner);

// Emits the upper-pane subtree (Create Game button). Must be called inside
// the LobbyRightUpperBox CLAY block; emits flex children only (no floating).
// BeginFrame requirements: BankButtonBeginFrame.
void BuildGameSelectUpperTree(GameSelectPanelState & state,
                              Resources & resources);

// Emits the tall-pane subtree ("Active Games" header + games list +
// info-block + Spectate/Join buttons). Must be called inside the
// LobbyRightTallBox CLAY block; emits flex children only.
// BeginFrame requirements: BankTextBeginFrame, BankButtonBeginFrame,
// ScrollListBeginFrame.
void BuildGameSelectTallTree(GameSelectPanelState & state,
                             Resources & resources);

}  // namespace silencer::client_ui::lobby

#endif
