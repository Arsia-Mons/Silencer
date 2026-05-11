#include "lobby_select.h"

#include "context.h"
#include "node.h"

#include <string>

namespace ui {
namespace v2 {

namespace {

// Legacy GameSelectPanel::Build values — kept here as named constants so
// the rendering math reads as a 1:1 mirror of game_select_panel.cpp.
constexpr Sint16 SBOX_X         = 407;
constexpr Sint16 SBOX_Y         = 89;
constexpr Uint16 SBOX_W         = 214;
constexpr Uint16 SBOX_H         = 265;
constexpr Uint8  SBOX_LINEH     = 14;
constexpr Uint8  SBOX_FONT_BANK = 133;
constexpr Uint8  SBOX_FONT_W    = 6;
constexpr Uint8  SBOX_ROW_H     = 11;       // legacy DrawText/highlight height per row.
constexpr Uint8  SBOX_SELECT_C  = 180;      // selected-row highlight fill color.

// Selected-game info overlays. Anchors mirror the legacy Overlay x/y
// (game_select_panel.cpp:78-107).
constexpr Sint16 INFO_X         = 405;
constexpr Sint16 INFO_NAME_Y    = 358;
constexpr Sint16 INFO_MAP_Y     = 370;
constexpr Sint16 INFO_PLAYERS_Y = 382;
constexpr Sint16 INFO_CREATOR_Y = 394;
constexpr Sint16 INFO_LIMITS_Y  = 406;

// Scrollbar chrome anchor. Lives at the right edge of the SelectBox, drawn
// by the legacy renderer when ScrollBar::draw is true. The moving bar
// handle (barres_index 10 sliced via srcrect) is NOT rendered here — v2
// has no NodeKind for srcrect-cropped Sprites yet, so only the chrome
// frame appears at runtime. Acceptable cosmetic gap for this iteration
// (see progress.txt P16g-4 notes); functional select / join still work
// because the Game-side click hit-test does not depend on the bar handle.
constexpr Sint16 SCROLL_X       = 621;      // legacy default x (panel sets only res_index).
constexpr Sint16 SCROLL_Y       = 89;       // matches ScrollBar default y.

}  // namespace

Node BuildGameSelectPanel(const Context & ctx, const GameSelectState & state,
                          const GameSelectHandlers & handlers)
{
	(void)ctx;
	std::vector<Node> children;
	children.reserve(16 + state.games.size());

	// Chrome — right border sprite + header.
	children.push_back(Sprite(/*bank=*/7, /*index=*/8).at(0, 0));
	children.push_back(
		Label("Active Games", /*font_bank=*/134, /*font_width=*/8).at(405, 70));
	children.push_back(
		Button("Create Game", ButtonType::B156x21)
			.at(242, 68)
			.onClick(handlers.on_create));
	children.push_back(
		Button("Join Game", ButtonType::B156x21)
			.at(436, 430)
			.onClick(handlers.on_join));

	// SelectBox visible-window rendering. Same pattern as the legacy
	// renderer (renderer.cpp:871): iterate items from `scrolled`, stop
	// after height/lineheight rows. Selected row gets a 214x11 highlight
	// fill (color 180) under the text.
	if(!state.games.empty()){
		const int max_visible_rows = SBOX_H / SBOX_LINEH;  // 265/14 = 18
		int line = 0;
		for(size_t i = 0; i < state.games.size(); i++){
			if((Uint16)i < state.scrolled) continue;
			if(line > max_visible_rows) break;
			const GameSelectGame & g = state.games[i];
			int row_y = SBOX_Y + line * SBOX_LINEH;
			if((int)i == state.selected_index){
				children.push_back(
					FilledRect(SBOX_W, SBOX_ROW_H, SBOX_SELECT_C).at(SBOX_X, (Sint16)row_y));
			}
			children.push_back(
				Label(g.name, SBOX_FONT_BANK, SBOX_FONT_W).at(SBOX_X, (Sint16)row_y));
			line++;
		}
	}

	// Per-game info overlays — populated by the engine Refresh step from
	// state.selected_index. Empty strings default to no Label emitted.
	if(state.selected_index >= 0 &&
	   state.selected_index < (int)state.games.size()){
		const GameSelectGame & g = state.games[state.selected_index];
		// Name overlay (Overlay uid 1 in legacy).
		children.push_back(
			Label(g.name, SBOX_FONT_BANK, SBOX_FONT_W).at(INFO_X, INFO_NAME_Y));
		// "Map: <mapname>"
		std::string map_text = std::string("Map: ") + g.mapname;
		children.push_back(
			Label(map_text, SBOX_FONT_BANK, SBOX_FONT_W).at(INFO_X, INFO_MAP_Y));
		// "<sec> Security<padding>*PASSWORD LOCK*?" — legacy pads to 21 chars
		// before the optional password-lock suffix.
		std::string sec_label = "No";
		switch(g.securitylevel){
			case 1: sec_label = "Low"; break;
			case 2: sec_label = "Medium"; break;
			case 3: sec_label = "High"; break;
		}
		std::string info_text = sec_label + " Security";
		while(info_text.size() < 21) info_text += " ";
		if(g.passworded) info_text += "*PASSWORD LOCK*";
		children.push_back(
			Label(info_text, SBOX_FONT_BANK, SBOX_FONT_W).at(INFO_X, INFO_PLAYERS_Y));
		// "Creator: <name>"
		std::string creator_text = std::string("Creator: ") + g.creator_name;
		children.push_back(
			Label(creator_text, SBOX_FONT_BANK, SBOX_FONT_W).at(INFO_X, INFO_CREATOR_Y));
		// "MinLv:%d MaxLv:%d MaxPl:%d MaxTm:%d"
		std::string limits_text = "MinLv:" + std::to_string(g.minlevel)
		                        + " MaxLv:" + std::to_string(g.maxlevel)
		                        + " MaxPl:" + std::to_string(g.maxplayers)
		                        + " MaxTm:" + std::to_string(g.maxteams);
		children.push_back(
			Label(limits_text, SBOX_FONT_BANK, SBOX_FONT_W).at(INFO_X, INFO_LIMITS_Y));
	}

	// Scrollbar chrome — only the frame Sprite; bar-handle slicing is not
	// yet in v2 (see comment near SCROLL_X). Default state hides it.
	if(state.show_scrollbar){
		children.push_back(Sprite(/*bank=*/7, /*index=*/9).at(SCROLL_X, SCROLL_Y));
	}

	return Group(std::move(children));
}

}  // namespace v2
}  // namespace ui
