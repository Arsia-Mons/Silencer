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

	// MainMenu uses absolute `.at()` positions matching the legacy
	// MainMenuScreen exactly. The staggered button arrangement (per-row
	// horizontal offset, logo overlapping the column on the left) is not
	// naturally container-shaped, so the design doc's `.at()` escape
	// hatch is the right tool. The Layout pass detects no containers in
	// this subtree and writes no rects → render+dispatch fall through to
	// the absolute path, producing byte-identical output to legacy.
	//
	// When containers do land (Options, LobbyConnect, etc.) they emit
	// Clay scopes and the rect-driven path takes over for those screens.
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
