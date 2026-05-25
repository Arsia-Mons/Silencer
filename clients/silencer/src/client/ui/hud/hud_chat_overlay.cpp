#include "client/ui/hud/hud_chat_overlay.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "resources.h"
#include "surface.h"
#include "ui/primitives/text.h"
#include "ui/primitives/text_input.h"
#include "ui/runtime/UiInteractionRegistry.h"

#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

namespace {

constexpr int kChatX = 400;
constexpr int kChatY = 280;
constexpr int kChatW = 231;
constexpr int kChatBackgroundInteriorH = 30;
constexpr int kChatChromeH = 70;
constexpr Uint8 kChatBackgroundBank = 188;
constexpr int kTextX = 10;
constexpr int kTextStartY = 10;
constexpr int kLineStepY = 10;
constexpr int kMaxHistoryChars = 36;
constexpr int kChatInputVisibleChars = 28;
constexpr int kRightCapX = 36;

Clay_ElementDeclaration ChatFloatingChild(const char* id, int x, int y, int w, int h) {
	Clay_String idString{
		.isStaticallyAllocated = true,
		.length = static_cast<int32_t>(std::strlen(id)),
		.chars = id,
	};
	return {
		.id = Clay_GetElementId(idString),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
		},
		.floating = {
			.offset = { (float)x, (float)y },
			.attachTo = CLAY_ATTACH_TO_PARENT,
		},
	};
}

void EmitChatBackgroundSprite(const Resources& resources,
                              int& ordinal,
                              Uint16 spriteIndex,
                              int logicalX,
                              int logicalY,
                              int srcW = 0,
                              int srcH = 0) {
	const int spriteW = SpriteWidth(resources, kChatBackgroundBank, spriteIndex);
	const int spriteH = SpriteHeight(resources, kChatBackgroundBank, spriteIndex);
	if(spriteW <= 0 || spriteH <= 0) return;
	if(srcW <= 0) srcW = spriteW;
	if(srcH <= 0) srcH = spriteH;

	CLAY({ .id = CLAY_IDI("InGameChatBackgroundSprite", ordinal++),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)srcW),
	                       CLAY_SIZING_FIXED((float)srcH) },
	       },
	       .floating = {
	           .offset = {
	               (float)SpriteX(resources, kChatBackgroundBank, spriteIndex, logicalX),
	               (float)SpriteY(resources, kChatBackgroundBank, spriteIndex, logicalY),
	           },
	           .attachTo = CLAY_ATTACH_TO_PARENT,
	       },
	       .custom = { .customData = AllocSpriteCustomData({
	           kChatBackgroundBank,
	           spriteIndex,
	           0,
	           0,
	           static_cast<Sint16>(srcW),
	           static_cast<Sint16>(srcH),
	           0,
	           128,
	           0,
	           0,
	       }) } }) {}
}

void EmitChatBackground(const Resources& resources) {
	int ordinal = 0;
	EmitChatBackgroundSprite(resources, ordinal, 0, 0, 0);

	int xOffset = SpriteWidth(resources, kChatBackgroundBank, 0);
	while(xOffset < kChatW - SpriteWidth(resources, kChatBackgroundBank, 2)){
		int tileW = kChatW - xOffset - kRightCapX;
		const int maxTileW = SpriteWidth(resources, kChatBackgroundBank, 1);
		if(maxTileW <= 0) break;
		if(tileW > maxTileW) tileW = maxTileW;
		if(tileW <= 0) break;
		EmitChatBackgroundSprite(
			resources,
			ordinal,
			1,
			xOffset,
			0,
			tileW,
			SpriteHeight(resources, kChatBackgroundBank, 1));
		xOffset += tileW;
	}
	EmitChatBackgroundSprite(resources, ordinal, 2, kChatW - kRightCapX, 0);

	xOffset = SpriteWidth(resources, kChatBackgroundBank, 6);
	EmitChatBackgroundSprite(resources, ordinal, 6, 0, kChatBackgroundInteriorH);
	while(xOffset < kChatW - SpriteWidth(resources, kChatBackgroundBank, 8)){
		int tileW = kChatW - xOffset - kRightCapX;
		const int maxTileW = SpriteWidth(resources, kChatBackgroundBank, 7);
		if(maxTileW <= 0) break;
		if(tileW > maxTileW) tileW = maxTileW;
		if(tileW <= 0) break;
		EmitChatBackgroundSprite(
			resources,
			ordinal,
			7,
			xOffset,
			kChatBackgroundInteriorH,
			tileW,
			SpriteHeight(resources, kChatBackgroundBank, 7));
		xOffset += tileW;
	}
	EmitChatBackgroundSprite(
		resources, ordinal, 8, kChatW - kRightCapX, kChatBackgroundInteriorH);
}

}  // namespace

void BuildChatOverlay(const HudView& view,
                      const Resources& resources,
                      Surface* surface,
                      silencer::ui::UiInteractionRegistry& interactions) {
	using namespace silencer::ui::primitives;

	const PlayerHudView& player = view.viewedPlayer;
	if(!player.chatActive && view.showChatTicks <= 0) return;

	std::vector<std::string> lines;
	for(int i = 0; i < (int)view.chatLines.size(); i++) {
		if(player.chatActive && i == 0 && view.chatLines.size() == 5) {
			continue;
		}
		lines.push_back(view.chatLines[i].substr(0, kMaxHistoryChars));
	}
	if(lines.empty() && !player.chatActive) return;

	const int contentLines = (int)lines.size() + (player.chatActive ? 1 : 0);
	const int panelH = std::max(kChatChromeH,
	                            kTextStartY + contentLines * kLineStepY + 12);

	(void)surface;
	CLAY(HudFloatingElement("InGameChatPanel", kChatX, kChatY, kChatW, panelH)) {
		EmitChatBackground(resources);

		int yoffset = kTextStartY;
		for(int i = 0; i < (int)lines.size(); i++) {
			CLAY({ .id = CLAY_IDI("InGameChatLine", i),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			       },
			       .floating = {
			           .offset = { (float)kTextX, (float)yoffset },
			           .attachTo = CLAY_ATTACH_TO_PARENT,
			       } }) {
				Text(ClayStringFromStd(lines[i]),
				     { .size = TextSize::Body,
				       .effect = TextEffect::LegacyPalette(0, 136) });
			}
			yoffset += kLineStepY;
		}

		if(player.chatActive){
			const char* inputPrefix = player.chatWithTeam ? "(TEAM):" : "(ALL):";
			const int prefixW = static_cast<int>(std::strlen(inputPrefix)) *
			                    TextAdvance(TextSize::Body);
			CLAY(ChatFloatingChild("InGameChatInputPrefix",
			                       kTextX,
			                       yoffset,
			                       prefixW,
			                       TextLineHeight(TextSize::Body))) {
				Text(ClayStringFromCString(inputPrefix),
				     { .size = TextSize::Body,
				       .effect = TextEffect::LegacyPalette(0, 136) });
			}

			CLAY(ChatFloatingChild("InGameChatInputWrap",
			                       kTextX + prefixW,
			                       yoffset,
			                       kChatInputVisibleChars * TextAdvance(TextSize::Body),
			                       TextLineHeight(TextSize::Body))) {
				Clay_String stableChatText = AllocHudString(player.chatText);
				TextInput(CLAY_STRING("InGameChatInput"),
				          stableChatText.chars,
				          { .widthPx = static_cast<Uint16>(
				                kChatInputVisibleChars * TextAdvance(TextSize::Body)),
				            .heightPx = TextLineHeight(TextSize::Body),
				            .textSize = TextSize::Body,
				            .effect = TextEffect::LegacyPalette(0, 128),
				            .caretColor = 140,
				            .showCaret = ((SDL_GetTicks() / 50) % 32 < 16) },
				          { .actionId = "ingame.chat",
				            .label = "In-game chat",
				            .interactions = &interactions,
				            .uid = 9000,
				            .maxLength = player.chatTextCapacity - 1,
				            .cancelOnEscape = true });
			}

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
	}
}

}  // namespace client_ui
}  // namespace silencer
