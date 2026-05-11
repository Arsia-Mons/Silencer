#ifndef SILENCER_UI_V2_SCREENS_LOBBY_TECH_H
#define SILENCER_UI_V2_SCREENS_LOBBY_TECH_H

#include "shared.h"

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct GameTechHandlers {
	std::function<void()> on_back_to_teams;
};

// Engine-supplied dynamic state for the game-tech panel. At preview gate
// the legacy Build skips the entire per-team-peer-per-item loop (no team
// when localpeerid isn't in peerlist), and the slots-left / tech-name /
// tech-description overlays all have empty text, so this struct stays
// empty for now. Fields land here as engine integration lands (P16).
struct GameTechState {};

// Right-side tech-choice panel on the LobbyScreen. The legacy panel
// creates 12 checkboxes + tech-name overlays + a slots-left counter +
// 8 description lines, but every one of those is dynamic — populated
// only when a team / buyable-item list / peer-info reply is in hand.
// At preview-gate time only the "Back To Teams" B156x21 button at
// (242, 68) emits pixels.
Node BuildGameTechPanel(const Context & ctx, const GameTechState & state = {},
                        const GameTechHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
