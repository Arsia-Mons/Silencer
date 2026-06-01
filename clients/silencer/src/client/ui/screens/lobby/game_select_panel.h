#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H

// Screen-side lobby GameSelectPanel: owns the per-frame games snapshot, selected
// game info strings, and the Join/Spectate/Create click flags. Domain glue
// (JoinGame / SpectateGame / level checks / password modal / ShowGameCreate)
// lives here in the screen; the cppx view owns retained composition.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <string>
#include <vector>

class World;
class ScreenContext;
class LobbyScreen;

namespace silencer::client_ui::lobby {

constexpr int kGameSelectVisibleRows = 8;

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

	// Per-frame click flags. Set by retained callbacks; consumed
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
bool GameSelectPanelHandleUiIntent(GameSelectPanelState & state,
                                   const silencer::ui::UiAction & action);
void GameSelectPanelSelectRow(GameSelectPanelState & state, int index);
void GameSelectPanelScrollRows(GameSelectPanelState & state, int delta);
void GameSelectPanelRequestCreate(GameSelectPanelState & state);
void GameSelectPanelRequestJoin(GameSelectPanelState & state);
void GameSelectPanelRequestSpectate(GameSelectPanelState & state);

}  // namespace silencer::client_ui::lobby

#endif
