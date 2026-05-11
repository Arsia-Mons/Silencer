#ifndef SILENCER_UI_V2_SCREENS_LOBBY_CREATE_H
#define SILENCER_UI_V2_SCREENS_LOBBY_CREATE_H

#include "shared.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct GameCreateHandlers {
	std::function<void()> on_security_toggle;
	std::function<void()> on_create;
};

// Engine-supplied dynamic state for the game-create panel. Mirrors the
// legacy GameCreatePanel::Build's per-widget initial values, plus the
// runtime-loaded map list (legacy reads it via MapDownloader::ListFiles +
// FetchServerMapList; the preview harness replicates that and feeds the
// result here so the v2 builder stays pure of filesystem/network I/O).
struct GameCreateState {
	std::string security_text   = "Medium";
	std::string min_level_text  = "0";
	std::string max_level_text  = "99";
	std::string max_players_text = "24";
	std::string max_teams_text   = "6";
	// Game name: legacy seeds with Config::defaultgamename ("New Game" by
	// default).
	std::string game_name;
	// Password input: empty at preview gate (post-Build, pre-input). The
	// legacy renderer's `if(textinput.password)` masks every char to '*',
	// so empty stays empty.
	std::string password;
	// Map list, in the order the legacy SelectBox::AddItem walks (sorted
	// local files first, then server-only entries with "[DL] " prefix).
	std::vector<std::string> map_items;
	// Names matching the "[DL] foo" form whose underlying map isn't on
	// disk yet. Used to render the [DL] badge column on the right side
	// of each row.
	std::set<std::string> server_maps;
	// Scroll position, in items. Legacy panel forces this to 0 in Build,
	// then the panel's Tick reads it back from the scrollbar — but
	// Screen::Tick / Panel::Tick aren't run at preview gate, so 0 holds.
	int map_scrolled = 0;
	// Selected row index (-1 = none). Legacy MapDownloader::selectedmap is
	// reset to -1 at Build time and only changes on mouse hover.
	int selected_map = -1;
};

// Game-create right-side panel: chrome border + "Game Options" form
// (security / level range / player + team caps / map list) on the left,
// "Select Map" picker on the right, with game name + password inputs and
// the Create button along the bottom.
//
// `name_active` controls whether the name input renders the blinking
// caret. Legacy flow: ShowGameCreate sets lobbyiface->activeobject =
// gameCreate->id then ActiveChanged propagates into gameCreate, which
// has activeobject = nametextinput → nametextinput.showcaret = true.
Node BuildGameCreatePanel(const Context & ctx, const GameCreateState & state,
                          const GameCreateHandlers & handlers = {},
                          bool name_active = true);

}  // namespace v2
}  // namespace ui

#endif
