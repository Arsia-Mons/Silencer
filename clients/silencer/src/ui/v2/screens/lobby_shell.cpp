#include "lobby_shell.h"

#include "context.h"
#include "lobby_character.h"
#include "lobby_chat.h"
#include "lobby_create.h"
#include "node.h"

#include <string>

namespace ui {
namespace v2 {

Node BuildLobby(const Context & ctx, const LobbyHandlers & handlers, const LobbyState & state)
{
	// Mirrors LobbyScreen::Build (clients/silencer/src/ui/screens/lobby/
	// lobby_screen.cpp). Sub-panels are ported one at a time — character
	// landed with P10, chat with P11, game_create with P12. Other
	// right-side panels (game_select/game_join/game_tech) follow in
	// P13–P15. The map-name overlay (uid 8) has empty text at preview
	// gate and renders nothing, so it is omitted here.
	std::string version_label = "v.";
	if(ctx.version) version_label += ctx.version;
	// Chat caret is ON only when the chat sub-interface is the lobby's
	// active object. Once a right-side panel takes focus, legacy
	// ShowGameCreate / ShowGameTech clear chatiface->activeobject and
	// re-run ActiveChanged → showcaret flips to false.
	const bool chat_active = (state.active_panel == LobbyActivePanel::None);

	std::vector<Node> children = {
		Label("Silencer", /*font_bank=*/135, /*font_width=*/11)
			.at(15, 32)
			.withColor(152),
		Label(version_label, /*font_bank=*/133, /*font_width=*/6)
			.at(115, 39)
			.withColor(189),
		Button("Go Back", ButtonType::B156x21)
			.at(473, 29)
			.onClick(handlers.on_go_back),
		BuildCharacterPanel(ctx, state.selected_agency),
		BuildChatPanel(ctx, chat_active),
	};
	if(state.active_panel == LobbyActivePanel::GameCreate){
		children.push_back(BuildGameCreatePanel(ctx, state.game_create, handlers.game_create, /*name_active=*/true));
	}
	return Background(/*bank=*/7, /*index=*/1, std::move(children));
}

}  // namespace v2
}  // namespace ui
