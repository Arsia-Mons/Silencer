#include "client/ui/hud/hud_chat_overlay.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/views/HudView.h"
#include "surface.h"
#include "ui/primitives/text.h"
#include "ui/primitives/box.h"
#include "ui/runtime/UiInteractionRegistry.h"

#include <SDL3/SDL_timer.h>

#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

void BuildChatOverlay(const HudView& view,
                      Surface* surface,
                      silencer::ui::UiInteractionRegistry& interactions) {
	using namespace silencer::ui::primitives;

	const PlayerHudView& player = view.viewedPlayer;

	std::vector<std::string> lines;
	for(int i = 0; i < (int)view.chatLines.size(); i++) {
		if(player.chatActive && i == 0 && view.chatLines.size() == 5) {
			continue;
		}
		lines.push_back(view.chatLines[i].substr(0, 36));
	}
	std::string inputPrefix;
	std::string inputText;
	if(player.chatActive) {
		inputPrefix = player.chatWithTeam ? "(TEAM):" : "(ALL):";
		inputText = inputPrefix + player.chatText;
		if((SDL_GetTicks() / 50) % 32 < 16) {
			inputText.push_back('|');
		}
		lines.push_back(inputText);
	}
	if(lines.empty()) return;

	int panelH = 22 + ((int)lines.size() * 10);
	if(panelH < 42) panelH = 42;
	if(player.chatActive){
		silencer::ui::UiInteractable chat;
		chat.id = "ingame.chat";
		chat.labelText = "In-game chat";
		chat.kind = silencer::ui::UiInteractableKind::TextInput;
		chat.uid = 9000;
		chat.value = player.chatText;
		chat.maxLength = player.chatTextCapacity - 1;
		chat.clayId = CLAY_ID("InGameChatPanel");
		chat.hasClayId = true;
		chat.cancelOnEscape = true;
		interactions.RegisterInteractable(chat);

		silencer::ui::UiInteractable channel;
		channel.id = "ingame.chat.channel";
		channel.labelText = player.chatWithTeam ? "Team chat" : "All chat";
		channel.kind = silencer::ui::UiInteractableKind::Toggle;
		channel.selected = player.chatWithTeam;
		channel.clayId = CLAY_ID("InGameChatPanel");
		channel.hasClayId = true;
		interactions.RegisterInteractable(channel);

		if(!interactions.HasFocus()){
			interactions.FocusInteractableById("ingame.chat");
		}
	}

	CLAY({ .id = CLAY_ID("InGameChatRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)surface->w),
	                       CLAY_SIZING_FIXED((float)surface->h) },
	           .padding = { 0, 9, 0, 160 },
	           .childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_BOTTOM },
	       },
	}) {
		CLAY(Box(BoxVariants::Chrome, {
		       .id = CLAY_ID("InGameChatPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(231), CLAY_SIZING_FIXED((float)panelH) },
		           .padding = { 10, 10, 8, 8 },
		           .childGap = 1,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .backgroundColor = { 0, 0, 0, 192 },
		})) {
			for(int i = 0; i < (int)lines.size(); i++) {
				Uint8 brightness = (player.chatActive && i == (int)lines.size() - 1) ? 128 : 136;
				Text(ClayStringFromStd(lines[i]),
				     { .size = TextSize::Body,
				       .effect = TextEffect::LegacyPalette(0, brightness) });
			}
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
