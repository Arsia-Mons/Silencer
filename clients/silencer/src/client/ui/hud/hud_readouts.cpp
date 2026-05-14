#include "client/ui/hud/hud_readouts.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/views/HudView.h"
#include "render/clay_ui_payloads.h"
#include "surface.h"

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

	auto string = [](const std::string& text) {
		return Clay_String{
			.isStaticallyAllocated = false,
			.length = static_cast<int32_t>(text.size()),
			.chars = text.c_str(),
		};
	};
	auto emitText = [&](const char* id, int x, int y, int w, int h,
	                    const std::string& text, Uint8 bank, Uint8 width,
	                    Uint8 color) {
		if(text.empty()) return;
		CLAY(HudFloatingElement(id, x, y, w, h)) {
			CLAY_TEXT(string(text), CLAY_TEXT_CONFIG({
				.userData = nullptr,
				.textColor = { (float)color, 0, 0, 255 },
				.fontId = bank,
				.fontSize = width,
			}));
		}
	};
	silencer::clay_bridge::BankTextDrawData alphaText{128, false, true};
	auto emitAlphaText = [&](const char* id, int x, int y, int w, int h,
	                         const std::string& text, Uint8 bank, Uint8 width,
	                         Uint8 color) {
		if(text.empty()) return;
		CLAY(HudFloatingElement(id, x, y, w, h)) {
			CLAY_TEXT(string(text), CLAY_TEXT_CONFIG({
				.userData = &alphaText,
				.textColor = { (float)color, 0, 0, 255 },
				.fontId = bank,
				.fontSize = width,
			}));
		}
	};

	CLAY({ .id = CLAY_ID("InGameHudReadoutsRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		emitAlphaText("HudCurrentAmmo", 117, 457, 40, 18, currentAmmo, 135, 12, 0);
		emitText("HudBlasterAmmo", 4, 414, 20, 8, blasterAmmo, 132, 4, 0);
		emitText("HudLaserAmmo", 4, 428, 20, 8, laserAmmo, 132, 4, 0);
		emitText("HudRocketAmmo", 4, 442, 20, 8, rocketAmmo, 132, 4, 0);
		emitText("HudFlamerAmmo", 4, 456, 20, 8, flamerAmmo, 132, 4, 0);
		emitText("HudCredits", 572, 456, 60, 18, credits, 135, 12, 202);
		emitText("HudHealth", 152, 463, 26, 8, health, 132, 4, 161);
		emitText("HudShield", 475, 463, 26, 8, shield, 132, 4, 202);
		const int xoffsets[] = {612, 584, 556, 528};
		const int yoffsets[] = {13, 13, 11, 7};
		emitText("HudInventoryCount0", xoffsets[0] + 20, yoffsets[0] + 20, 32, 10, inventoryCounts[0], 132, 6, 0);
		emitText("HudInventoryCount1", xoffsets[1] + 20, yoffsets[1] + 20, 32, 10, inventoryCounts[1], 132, 6, 0);
		emitText("HudInventoryCount2", xoffsets[2] + 20, yoffsets[2] + 20, 32, 10, inventoryCounts[2], 132, 6, 0);
		emitText("HudInventoryCount3", xoffsets[3] + 20, yoffsets[3] + 20, 32, 10, inventoryCounts[3], 132, 6, 0);
	}
}

void BuildHudTraceTime(Surface* surface, Uint8 tracetime) {
	std::string text = "Government Trace Time: " + std::to_string(tracetime);
	silencer::clay_bridge::BankTextDrawData textData{136, false, false};
	CLAY({ .id = CLAY_ID("InGameHudTraceRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		CLAY(HudFloatingElement("HudTraceTime", 20, 350, 180, 12)) {
			CLAY_TEXT(ClayStringFromStd(text), CLAY_TEXT_CONFIG({
				.userData = &textData,
				.textColor = { 0, 0, 0, 255 },
				.fontId = 133,
				.fontSize = 6,
			}));
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
