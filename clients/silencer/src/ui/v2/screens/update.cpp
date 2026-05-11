#include "update.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildUpdate(const Context & ctx, const UpdateHandlers & handlers)
{
	(void)ctx;
	// Mirrors UpdateScreen::Build (clients/silencer/src/ui/screens/update/
	// update_screen.cpp). At preview gate (post-Build, pre-Tick) the legacy
	// renders:
	//   - background overlay sprite (bank=40, idx=4) at (0, 0)
	//   - status/progress overlays contribute no pixels (empty text)
	//   - all four B156x21 buttons draw because Tick — which gates draw on
	//     Updater state — hasn't run. Three of them stack at (161, 230)
	//     with their texts ("Update", "Retry", "Download") stamped on top
	//     of each other in objectlist order; cancel sits alone at (322, 230).
	return Background(/*bank=*/40, /*index=*/4, {
		Button("Update",   ButtonType::B156x21).at(161, 230).onClick(handlers.on_update),
		Button("Cancel",   ButtonType::B156x21).at(322, 230).onClick(handlers.on_cancel),
		Button("Retry",    ButtonType::B156x21).at(161, 230).onClick(handlers.on_retry),
		Button("Download", ButtonType::B156x21).at(161, 230).onClick(handlers.on_download),
	});
}

}  // namespace v2
}  // namespace ui
