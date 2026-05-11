#include "lobby_tech.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildGameTechPanel(const Context & ctx, const GameTechState & state,
                        const GameTechHandlers & handlers)
{
	(void)ctx;
	(void)state;
	// Mirrors GameTechPanel::Build (clients/silencer/src/ui/screens/lobby/
	// panels/game_tech_panel.cpp). At preview gate the per-team-peer-per-item
	// checkbox loop is skipped (no team), and the slots-left / tech-name /
	// tech-description overlays all have empty text, so only the "Back To
	// Teams" button emits pixels.
	return Group({
		Button("Back To Teams", ButtonType::B156x21)
			.at(242, 68)
			.onClick(handlers.on_back_to_teams),
	});
}

}  // namespace v2
}  // namespace ui
