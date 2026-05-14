#include "client/ui/hud/hud_secret_overlays.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "render/clay_ui_payloads.h"
#include "resources.h"
#include "surface.h"

#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

void BuildHudSecretSprites(const HudView& view, Surface* surface,
                           const Resources& resources, const TeamHudView& team,
                           int yoffset, Uint8 phase) {
	struct SpriteSpec { int x; int y; Uint8 bank; Uint16 index; Uint8 brightness; };
	std::vector<SpriteSpec> sprites;
	auto addSprite = [&](int x, int y, Uint8 bank, Uint16 index, Uint8 brightness = 128) {
		sprites.push_back(SpriteSpec{x, y, bank, index, brightness});
	};

	Uint16 backgroundIndex = team.beamingTerminalId ? 1 : 0;
	addSprite(SpriteX(resources, 187, backgroundIndex),
	          SpriteY(resources, 187, backgroundIndex, yoffset),
	          187, backgroundIndex);

	Uint8 highlightBrightness = 120;
	if(phase % 32 < 16) highlightBrightness += (phase % 16);
	else highlightBrightness += 16 - (phase % 16);
	if(view.highlightSecrets) {
		addSprite(SpriteX(resources, 86, 2), SpriteY(resources, 86, 2, yoffset), 86, 2, highlightBrightness);
	}
	if(view.highlightMinimap) {
		addSprite(SpriteX(resources, 86, 1), SpriteY(resources, 86, 1), 86, 1, highlightBrightness);
	}

	CLAY({ .id = CLAY_ID("InGameHudSecretSpritesRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		for(unsigned int i = 0; i < sprites.size(); ++i) {
			const SpriteSpec& s = sprites[i];
			int w = SpriteWidth(resources, s.bank, s.index);
			int h = SpriteHeight(resources, s.bank, s.index);
			if(w <= 0 || h <= 0) continue;
			CLAY({ .id = CLAY_IDI("HudSecretSpriteWrap", i),
			       .layout = {
				       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
			       },
			       .floating = {
				       .offset = { (float)s.x, (float)s.y },
				       .attachTo = CLAY_ATTACH_TO_ROOT,
			       },
			}) {
				CLAY({ .id = CLAY_IDI("HudSecretSprite", i),
				       .layout = {
					       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
				       },
				       .custom = { .customData = AllocSpriteCustomData({
					       s.bank, s.index, 0, 0, 0, 0, 0, s.brightness, 0, 0,
				       }) },
				}) {}
			}
		}
	}
}

void BuildHudSecretProgress(const PlayerHudView& player, Surface* surface,
                            int yoffset, int secretprogress, Uint8 phase) {
	static const char* names[] = {
		"Guv Net", "OS", "Protocol", "Cypher Lock 1", "Cypher Lock 2",
		"Cypher Lock 3", "Header", "Schedule", "Location",
	};
	silencer::clay_bridge::BankTextDrawData textData[9];
	Uint8 color[9];
	Uint8 effectColor = 0;
	Uint8 brightness = 136;
	for(int i = 0; i < 9; ++i) {
		secretprogress -= 20;
		bool hackingTick = (player.state == kPlayerStateHacking &&
		                    player.state_i == 16 && phase % 16 < 8);
		if(secretprogress < (hackingTick ? -20 : 0)) {
			effectColor = 114;
			brightness = 96;
		}
		color[i] = effectColor;
		textData[i] = { brightness, false, false };
	}

	CLAY({ .id = CLAY_ID("InGameHudSecretRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		for(int i = 0; i < 9; ++i) {
			CLAY({ .id = CLAY_IDI("HudSecretProgressLine", (uint32_t)i),
			       .layout = {
				       .sizing = { CLAY_SIZING_FIXED(110), CLAY_SIZING_FIXED(12) },
			       },
			       .floating = {
				       .offset = { 10, (float)(54 + (i * 13) + yoffset) },
				       .attachTo = CLAY_ATTACH_TO_ROOT,
			       },
			}) {
				CLAY_TEXT(ClayStringFromCString(names[i]), CLAY_TEXT_CONFIG({
					.userData = &textData[i],
					.textColor = { (float)color[i], 0, 0, 255 },
					.fontId = 133,
					.fontSize = 6,
				}));
			}
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
