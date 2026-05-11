#include "lobby_join.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildGameJoinPanel(const Context & ctx, const GameJoinState & state,
                        const GameJoinHandlers & handlers)
{
	(void)ctx;
	// Legacy panel adds buttons in order tech / changeteam / ready then sets
	// buttonenter = ready. Mirror the same declaration order.
	const char * ready_label = state.waiting ? "Waiting..." : "Ready";
	return Group({
		Button("Choose Tech", ButtonType::B156x21)
			.at(242, 68)
			.onClick(handlers.on_choose_tech),
		Button("Change Team", ButtonType::B156x21)
			.at(242, 100)
			.onClick(handlers.on_change_team),
		Button(ready_label, ButtonType::B156x21)
			.at(242, 160)
			.onClick(handlers.on_ready),
	});
}

}  // namespace v2
}  // namespace ui
