#include "lobby_shell.h"

#include "context.h"
#include "node.h"

#include <string>

namespace ui {
namespace v2 {

Node BuildLobby(const Context & ctx, const LobbyHandlers & handlers)
{
	// Mirrors the chrome portion of LobbyScreen::Build
	// (clients/silencer/src/ui/screens/lobby/lobby_screen.cpp). Panels are
	// ported separately (P10–P15). The map-name overlay (uid 8) has empty
	// text at preview gate and renders nothing, so it is omitted here.
	std::string version_label = "v.";
	if(ctx.version) version_label += ctx.version;
	return Background(/*bank=*/7, /*index=*/1, {
		Label("Silencer", /*font_bank=*/135, /*font_width=*/11)
			.at(15, 32)
			.withColor(152),
		Label(version_label, /*font_bank=*/133, /*font_width=*/6)
			.at(115, 39)
			.withColor(189),
		Button("Go Back", ButtonType::B156x21)
			.at(473, 29)
			.onClick(handlers.on_go_back),
	});
}

}  // namespace v2
}  // namespace ui
