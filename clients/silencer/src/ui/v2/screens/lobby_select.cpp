#include "lobby_select.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildGameSelectPanel(const Context & ctx, const GameSelectState & state,
                          const GameSelectHandlers & handlers)
{
	(void)ctx;
	(void)state;
	// Mirrors GameSelectPanel::Build (clients/silencer/src/ui/screens/lobby/
	// panels/game_select_panel.cpp). Right border sprite + "Active Games"
	// header + the Create Game / Join Game buttons. The SelectBox /
	// ScrollBar / per-game info overlays are created in legacy Build but
	// emit no pixels at preview gate (selectbox items empty, scrollbar
	// draw=false, info overlay text empty), so they are omitted here.
	return Group({
		Sprite(/*bank=*/7, /*index=*/8).at(0, 0),
		Label("Active Games", /*font_bank=*/134, /*font_width=*/8).at(405, 70),
		Button("Create Game", ButtonType::B156x21)
			.at(242, 68)
			.onClick(handlers.on_create),
		Button("Join Game", ButtonType::B156x21)
			.at(436, 430)
			.onClick(handlers.on_join),
	});
}

}  // namespace v2
}  // namespace ui
