#include "client/ui/hud/hud_readouts.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/views/HudView.h"
#include "surface.h"
#include "ui/primitives/text.h"

#include <string>

namespace silencer {
namespace client_ui {

void BuildHudReadouts(const PlayerHudView& player, Surface* surface, Uint8 currentammo) {
	std::string currentAmmo = std::string(currentammo < 10 ? " " : "") + std::to_string(currentammo);
	std::string blasterAmmo = "99";
	std::string laserAmmo = player.laserAmmo > 0
		? std::string(player.laserAmmo < 10 ? " " : "") + std::to_string(player.laserAmmo)
		: "";
	std::string rocketAmmo = player.rocketAmmo > 0
		? std::string(player.rocketAmmo < 10 ? " " : "") + std::to_string(player.rocketAmmo)
		: "";
	std::string flamerAmmo = player.flamerAmmo > 0
		? std::string(player.flamerAmmo < 10 ? " " : "") + std::to_string(player.flamerAmmo)
		: "";
	std::string credits = std::to_string(player.credits);
	std::string health = std::to_string(player.health);
	std::string shield = std::to_string(player.shield);
	std::string inventoryCounts[4];
	for(int i = 0; i < 4; ++i) {
		if(player.inventoryItemsNum[i] > 1) {
			inventoryCounts[i] = std::to_string(player.inventoryItemsNum[i]);
		}
	}

	using silencer::ui::primitives::Text;
	using silencer::ui::primitives::TextEffect;
	using silencer::ui::primitives::TextSize;
	auto emitText = [&](const char* id, int x, int y, int w, int h,
	                    const std::string& text, TextSize size,
	                    Uint8 color) {
		if(text.empty()) return;
		CLAY(HudFloatingElement(id, x, y, w, h)) {
			Text(ClayStringFromStd(text),
			     { .size = size,
			       .effect = TextEffect::LegacyPalette(color) });
		}
	};
	auto emitAlphaText = [&](const char* id, int x, int y, int w, int h,
	                         const std::string& text, TextSize size,
	                         Uint8 color) {
		if(text.empty()) return;
		CLAY(HudFloatingElement(id, x, y, w, h)) {
			Text(ClayStringFromStd(text),
			     { .size = size,
			       .effect = TextEffect::LegacyPalette(color, 128, false, true) });
		}
	};

	CLAY({ .id = CLAY_ID("InGameHudReadoutsRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		emitAlphaText("HudCurrentAmmo", 117, 457, 40, 18, currentAmmo, TextSize::HudCounter, 0);
		emitText("HudBlasterAmmo", 4, 414, 20, 8, blasterAmmo, TextSize::Tiny, 0);
		emitText("HudLaserAmmo", 4, 428, 20, 8, laserAmmo, TextSize::Tiny, 0);
		emitText("HudRocketAmmo", 4, 442, 20, 8, rocketAmmo, TextSize::Tiny, 0);
		emitText("HudFlamerAmmo", 4, 456, 20, 8, flamerAmmo, TextSize::Tiny, 0);
		emitText("HudCredits", 572, 456, 60, 18, credits, TextSize::HudCounter, 202);
		emitText("HudHealth", 152, 463, 26, 8, health, TextSize::Tiny, 161);
		emitText("HudShield", 475, 463, 26, 8, shield, TextSize::Tiny, 202);
		const int xoffsets[] = {612, 584, 556, 528};
		const int yoffsets[] = {13, 13, 11, 7};
		emitText("HudInventoryCount0", xoffsets[0] + 20, yoffsets[0] + 20, 32, 10, inventoryCounts[0], TextSize::TinyCounter, 0);
		emitText("HudInventoryCount1", xoffsets[1] + 20, yoffsets[1] + 20, 32, 10, inventoryCounts[1], TextSize::TinyCounter, 0);
		emitText("HudInventoryCount2", xoffsets[2] + 20, yoffsets[2] + 20, 32, 10, inventoryCounts[2], TextSize::TinyCounter, 0);
		emitText("HudInventoryCount3", xoffsets[3] + 20, yoffsets[3] + 20, 32, 10, inventoryCounts[3], TextSize::TinyCounter, 0);
	}
}

void BuildHudTraceTime(Surface* surface, Uint8 tracetime) {
	std::string text = "Government Trace Time: " + std::to_string(tracetime);
	CLAY({ .id = CLAY_ID("InGameHudTraceRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		CLAY(HudFloatingElement("HudTraceTime", 20, 350, 180, 12)) {
			silencer::ui::primitives::Text(
				ClayStringFromStd(text),
				{ .size = silencer::ui::primitives::TextSize::Body,
				  .effect = silencer::ui::primitives::TextEffect::LegacyPalette(0, 136) });
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
