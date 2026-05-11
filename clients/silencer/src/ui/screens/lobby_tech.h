#ifndef SILENCER_UI_V2_SCREENS_LOBBY_TECH_H
#define SILENCER_UI_V2_SCREENS_LOBBY_TECH_H

#include "shared.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct GameTechHandlers {
	std::function<void()>              on_back_to_teams;
	// Local-peer (column x=3) checkbox click: toggles the tech for the
	// item at the given visible-row index. Mirrors the BCHECKBOX clicked
	// branch in GameTechPanel::Tick.
	std::function<void(int)>           on_toggle_tech;
	// Local-peer tech-name overlay click: surfaces tech name + 8-line
	// description in the bottom-of-panel readout.
	std::function<void(int)>           on_select_tech;
};

// Per-row dynamic data for the 12-checkbox tech grid. Items are filtered
// from world.buyableitems (techslots > 0 + agency match) and ordered the
// same way the legacy panel iterates them.
struct GameTechItem {
	// Local-peer column (x=3) row label, e.g. "Item Name (1)".
	std::string label;
	// Per-peer-column rendered checkbox state.
	//   draw[c]    = peer column c is filled (peer present in team).
	//   checked[c] = peer's techchoices include this item.
	//   bright[c]  = effectbrightness for the column's checkbox sprite
	//                (legacy: 64 for usable / unselected local row, 128
	//                otherwise; non-local cols stay at 64 in Build).
	std::array<bool, 4>  draw     = {true, true, true, true};
	std::array<bool, 4>  checked  = {false, false, false, false};
	std::array<Uint8, 4> bright   = {64, 64, 64, 64};
	// Local-peer (col x=3) usability — drives the label brightness too.
	bool                 local_usable = true;
};

// Per-other-peer column header (x=0..2 → drawn at y=112+(2-i)*16). The
// local peer (x=3) has no header overlay — its column is unconditional.
struct GameTechPeerCol {
	bool        draw = false;
	std::string name;
	Sint16      name_x = 0;
};

// Engine-supplied dynamic state for the game-tech panel. Empty defaults
// preserve the preview-gate byte-identical PPM (only the "Back To Teams"
// button emits pixels — no team in preview-gate world.peerlist[localpeerid]
// → legacy Build skips the entire grid; the slots-left / tech-name /
// tech-description overlays exist with empty text and emit nothing).
struct GameTechState {
	std::string                              slots_left_text;
	std::string                              tech_name_text;
	Sint16                                   tech_name_x = 0;
	std::array<std::string, 8>               tech_desc_lines;
	std::array<GameTechPeerCol, 3>           peer_cols;
	std::vector<GameTechItem>                items;
};

// Right-side tech-choice panel on the LobbyScreen. The legacy panel
// creates 12+ checkboxes + tech-name overlays + a slots-left counter +
// 8 description lines, populated only when a team / buyable-item list /
// peer-info reply is in hand. At preview-gate time only the "Back To
// Teams" B156x21 button at (242, 68) emits pixels — the dynamic
// elements stay empty.
Node BuildGameTechPanel(const Context & ctx, const GameTechState & state = {},
                        const GameTechHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
