#include "update.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildUpdate(const Context & ctx, const UpdateHandlers & handlers,
                 const UpdateState * state)
{
	(void)ctx;
	// Mirrors UpdateScreen::Build (clients/silencer/src/ui/screens/update/
	// update_screen.cpp). At preview gate (state == nullptr) the legacy
	// post-Build pre-Tick state renders:
	//   - background overlay sprite (bank=40, idx=4) at (0, 0)
	//   - status/progress overlays contribute no pixels (empty text)
	//   - all four B156x21 buttons draw because Tick — which gates draw on
	//     Updater state — hasn't run. Three of them stack at (161, 230)
	//     with their texts ("Update", "Retry", "Download") stamped on top
	//     of each other in objectlist order; cancel sits alone at (322, 230).
	if(state == nullptr){
		return Background(/*bank=*/40, /*index=*/4, {
			Button("Update",   ButtonType::B156x21).at(161, 230).onClick(handlers.on_update),
			Button("Cancel",   ButtonType::B156x21).at(322, 230).onClick(handlers.on_cancel),
			Button("Retry",    ButtonType::B156x21).at(161, 230).onClick(handlers.on_retry),
			Button("Download", ButtonType::B156x21).at(161, 230).onClick(handlers.on_download),
		});
	}
	// Live path: render only the buttons UpdateScreen::Tick gates as
	// active. Status/progress overlays use textbank=134, textwidth=8,
	// recentered each frame around x=320.
	std::vector<Node> children;
	switch(state->left){
		case UpdateState::LeftButton::Update:
			children.push_back(Button("Update", ButtonType::B156x21)
				.at(161, 230).onClick(handlers.on_update));
		break;
		case UpdateState::LeftButton::Retry:
			children.push_back(Button("Retry", ButtonType::B156x21)
				.at(161, 230).onClick(handlers.on_retry));
		break;
		case UpdateState::LeftButton::Download:
			children.push_back(Button("Download", ButtonType::B156x21)
				.at(161, 230).onClick(handlers.on_download));
		break;
		case UpdateState::LeftButton::None: break;
	}
	if(state->show_cancel){
		children.push_back(Button("Cancel", ButtonType::B156x21)
			.at(322, 230).onClick(handlers.on_cancel));
	}
	if(!state->status_text.empty()){
		int x = 320 - (int)((state->status_text.length() * 8) / 2);
		children.push_back(Label(state->status_text, /*bank=*/134, /*width=*/8).at(x, 200));
	}
	if(!state->progress_text.empty()){
		int x = 320 - (int)((state->progress_text.length() * 8) / 2);
		children.push_back(Label(state->progress_text, /*bank=*/134, /*width=*/8).at(x, 215));
	}
	return Background(/*bank=*/40, /*index=*/4, std::move(children));
}

}  // namespace v2
}  // namespace ui
