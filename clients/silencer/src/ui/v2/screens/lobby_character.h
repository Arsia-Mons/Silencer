#ifndef SILENCER_UI_V2_SCREENS_LOBBY_CHARACTER_H
#define SILENCER_UI_V2_SCREENS_LOBBY_CHARACTER_H

#include "shared.h"

#include <functional>
#include <string>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct CharacterPanelHandlers {
	// Fired when the user clicks one of the 5 agency toggle sprites.
	// agency = Team::NOXIS..Team::BLACKROSE (0..4).
	std::function<void(Uint8 agency)> on_select_agency;
};

// Engine-supplied state for the character panel: which agency is
// highlighted, plus the username header + LEVEL/WINS/LOSSES/XP
// readouts. The legacy panel populates the readouts only after the
// lobby's user-info reply lands; before then they hold empty strings
// and contribute no pixels — matching the v2 preview gate output.
struct CharacterPanelState {
	Uint8 selected_agency = 0;
	std::string username;
	std::string level_text;
	std::string wins_text;
	std::string losses_text;
	std::string xp_text;
};

// Character panel: 5 agency toggle sprites at (20 + i*42, 90), bank 181,
// indices 0..4 (Noxis/Lazarus/Caliber/Static/Blackrose). The selected
// agency renders at effect_brightness=128, others at 32; all share
// effect_color=112 (mirrors the legacy Toggle::Tick path).
//
// Username header (effect_color=200) and the 4 stat overlays
// (effect_color=129, brightness=160, ramp=true) emit only when their
// state strings are non-empty — preserving the empty-text byte-identical
// preview gate.
Node BuildCharacterPanel(const Context & ctx, const CharacterPanelState & state);

}  // namespace v2
}  // namespace ui

#endif
