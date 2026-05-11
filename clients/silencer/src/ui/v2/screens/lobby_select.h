#ifndef SILENCER_UI_V2_SCREENS_LOBBY_SELECT_H
#define SILENCER_UI_V2_SCREENS_LOBBY_SELECT_H

#include "shared.h"

#include <functional>
#include <string>
#include <vector>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct GameSelectHandlers {
	std::function<void()> on_create;
	std::function<void()> on_join;
};

// Snapshot of one lobby game row. Mirrors the legacy SelectBox item +
// LobbyGame fields needed to render the per-game info overlays and to
// drive the Join click (level/password checks).
struct GameSelectGame {
	Uint32 id = 0;
	std::string name;            // legacy SelectBox item text (no [DL] prefix here)
	bool passworded = false;
	Uint32 accountid = 0;        // creator account id (for Creator overlay)
	std::string creator_name;
	std::string mapname;
	Uint8 securitylevel = 0;
	Uint8 minlevel = 0;
	Uint8 maxlevel = 0;
	Uint8 maxplayers = 0;
	Uint8 maxteams = 0;
};

// Engine-supplied state for the game-select panel. Empty defaults preserve
// preview-gate byte-identical PPM (no rows, no overlays, no scrollbar).
struct GameSelectState {
	std::vector<GameSelectGame> games;
	int selected_index = -1;
	Uint16 scrolled = 0;
	bool show_scrollbar = false;  // legacy ScrollBar::draw — true when games > height/lineheight.
};

// Right-side games-list panel on the LobbyScreen. Right border chrome +
// "Active Games" header + Create Game / Join Game buttons. SelectBox rows
// + per-game info overlays + scrollbar render when state.games is non-empty.
Node BuildGameSelectPanel(const Context & ctx, const GameSelectState & state = {},
                          const GameSelectHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
