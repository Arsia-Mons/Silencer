#include "main_menu.h"

#include "context.h"
#include "node.h"

#include <string>

namespace ui {
namespace v2 {

Node BuildMainMenu(const Context & ctx, const MainMenuHandlers & handlers)
{
	std::string version_label = "Silencer v";
	if(ctx.version) version_label += ctx.version;

	// Coordinates mirror the legacy MainMenuScreen::Build exactly. Each
	// Button uses the negative-anchor convention where the rendered pill
	// ends up at (x + 310, y + 288) for the default B196x33 chrome.
	return Background(/*bank=*/6, /*index=*/0, {
		// Bank 208's animation table (Overlay::Tick case 208) ramps frames
		// 29..58, holds at 60 for ~1 s, then ramps back. Index 60 is the
		// steady-state "fully revealed" logo — what users see most of the
		// time. Index 0 has a different, pre-animation frame with stale
		// .enabled / www.won.net text baked in.
		Sprite(/*bank=*/208, /*index=*/60),
		Label(version_label, /*font_bank=*/133, /*font_width=*/11)
			.at(10, (Sint16)(ctx.logical_h - 17)),
		Button("Tutorial").at(40, -134).onClick(handlers.on_tutorial),
		Button("Connect To Lobby").at(80, -67).onClick(handlers.on_lobby),
		Button("Options").at(40, 0).onClick(handlers.on_options),
		Button("Exit").at(0, 67).onClick(handlers.on_exit),
	});
}

}  // namespace v2
}  // namespace ui
