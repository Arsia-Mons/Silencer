#ifndef SILENCER_UI_V2_SCREENS_LOBBY_JOIN_H
#define SILENCER_UI_V2_SCREENS_LOBBY_JOIN_H

#include "shared.h"

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct GameJoinHandlers {
	std::function<void()> on_ready;
	std::function<void()> on_change_team;
	std::function<void()> on_choose_tech;
};

// Engine-supplied dynamic state for the game-join panel. The legacy Tick
// flips the Ready button label to "Waiting..." while the local peer is the
// host and not all peers have downloaded the map yet — surfaced as
// `waiting` so the v2 builder can mirror it once engine wiring lands. At
// preview gate the legacy Build seeds the label as "Ready", so the default
// (false) is the byte-identical target.
struct GameJoinState {
	bool waiting = false;
};

// Right-side join panel on the LobbyScreen. Three B156x21 buttons (Choose
// Tech / Change Team / Ready) at fixed positions. No chrome border sprite
// — the panel relies entirely on the lobby background sprite for chrome.
Node BuildGameJoinPanel(const Context & ctx, const GameJoinState & state = {},
                        const GameJoinHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
