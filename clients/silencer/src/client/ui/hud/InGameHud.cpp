#include "client/ui/hud/InGameHud.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "render/renderer.h"
#include "render/clay_ui_payloads.h"
#include "resources.h"
#include "surface.h"
#include "ui/primitives/bank_text.h"
#include "ui/primitives/box.h"
#include "ui/runtime/UiAutomationRegistry.h"

#include <SDL3/SDL_timer.h>

#include <cstring>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

namespace {

Clay_String ClayStringFromStd(const std::string& text) {
	return Clay_String{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(text.size()),
		.chars = text.c_str(),
	};
}

Clay_String ClayStringFromCString(const char* text) {
	return Clay_String{
		.isStaticallyAllocated = true,
		.length = static_cast<int32_t>(std::strlen(text)),
		.chars = text,
	};
}

Clay_ElementDeclaration HudFloatingElement(const char* id, int x, int y, int w, int h) {
	Clay_String idString{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(std::strlen(id)),
		.chars = id,
	};
	return {
		.id = Clay_GetElementId(idString),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
			.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
		},
		.floating = {
			.offset = { (float)x, (float)y },
			.attachTo = CLAY_ATTACH_TO_ROOT,
		},
	};
}

int SpriteWidth(const Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spritebank.size()) return 0;
	if(index >= res.spritebank[bank].size()) return 0;
	Surface* sprite = res.spritebank[bank][index].get();
	return sprite ? sprite->w : 0;
}

int SpriteHeight(const Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spritebank.size()) return 0;
	if(index >= res.spritebank[bank].size()) return 0;
	Surface* sprite = res.spritebank[bank][index].get();
	return sprite ? sprite->h : 0;
}

int SpriteOffsetX(const Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spriteoffsetx.size()) return 0;
	if(index >= res.spriteoffsetx[bank].size()) return 0;
	return res.spriteoffsetx[bank][index];
}

int SpriteOffsetY(const Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spriteoffsety.size()) return 0;
	if(index >= res.spriteoffsety[bank].size()) return 0;
	return res.spriteoffsety[bank][index];
}

int SpriteX(const Resources& res, Uint8 bank, Uint16 index, int logicalX = 0) {
	return logicalX - SpriteOffsetX(res, bank, index);
}

int SpriteY(const Resources& res, Uint8 bank, Uint16 index, int logicalY = 0) {
	return logicalY - SpriteOffsetY(res, bank, index);
}

// PLAYER state machine equivalents from the legacy code. The HUD only checks
// against HACKING/DEAD/DYING values; mirror those numeric tokens directly so
// the HUD doesn't need to include player.h.
constexpr Uint8 kPlayerStateHacking = 15;  // matches Player::HACKING
constexpr Uint8 kPlayerStateDying = 20;     // matches Player::DYING
constexpr Uint8 kPlayerStateDead = 21;      // matches Player::DEAD

void BuildHudSystemCameraFrame(const Resources& resources, Surface* surface,
                               Uint8 bank, Uint16 index, Uint8 offsetBank,
                               int logicalY) {
	if(bank >= resources.spritebank.size()) return;
	if(index >= resources.spritebank[bank].size()) return;
	Surface* sprite = resources.spritebank[bank][index].get();
	if(!sprite) return;
	int x = -SpriteOffsetX(resources, bank, index);
	int y = -SpriteOffsetY(resources, offsetBank, index) + logicalY;

	CLAY({ .id = CLAY_IDI("HudSystemCameraFrameRoot", index),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		CLAY({ .id = CLAY_IDI("HudSystemCameraFrame", index),
		       .layout = {
			       .sizing = { CLAY_SIZING_FIXED((float)sprite->w), CLAY_SIZING_FIXED((float)sprite->h) },
		       },
		       .floating = {
			       .offset = { (float)x, (float)y },
			       .attachTo = CLAY_ATTACH_TO_ROOT,
		       },
		       .custom = { .customData = AllocSpriteCustomData({
			       bank, index, 0, 0, 0, 0, 0, 128, 0, 0,
		       }) },
		}) {}
	}
}

void BuildHudReadouts(const PlayerHudView& player, Uint8 currentammo, Surface* surface) {
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

void BuildHudTraceTime(Uint8 tracetime, Surface* surface) {
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

void BuildHudSecretSprites(const Resources& resources, Surface* surface,
                           const HudView& view, const TeamHudView& team,
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

void BuildHudSecretProgress(Surface* surface, const PlayerHudView& player,
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

int BuildHudTeams(const Resources& resources, Surface* surface,
                  const HudView& view, Uint8 phase) {
	if(view.teams.empty()) return 0;

	struct SpriteSpec { int x; int y; Uint8 bank; Uint16 index; Uint8 rampColor; Uint8 rampPlus; };
	std::vector<SpriteSpec> sprites;
	auto addSprite = [&](int x, int y, Uint8 bank, Uint16 index,
	                     Uint8 rampColor = 0, Uint8 rampPlus = 0) {
		sprites.push_back(SpriteSpec{x, y, bank, index, rampColor, rampPlus});
	};

	if(view.teams.size() == 1) {
		addSprite(SpriteX(resources, 94, 1), SpriteY(resources, 94, 1), 94, 1);
	}else{
		addSprite(SpriteX(resources, 103, 0),
		          SpriteY(resources, 103, 0, -133 + ((int)view.teams.size() - 1) * 20),
		          103, 0);
		addSprite(SpriteX(resources, 103, 1), SpriteY(resources, 103, 1), 103, 1);
	}

	int teamyoffset = 5;
	for(const TeamHudView& team : view.teams) {
		for(int i = 0; i < team.numPeers; i++) {
			const TeamHudView::PeerSlot& slot = team.peerSlots[i];
			if(!slot.present) continue;
			Uint8 index = (slot.state == kPlayerStateDead || slot.state == kPlayerStateDying ? 8 : 4);
			Uint8 rampColor = 0;
			Uint8 rampPlus = 0;
			if(slot.inBase || slot.hasSecret) {
				Uint8 time = 4;
				Uint8 shift = 2;
				rampColor = 210;
				if(slot.hasSecret) {
					time = 8;
					rampColor = 114;
					shift = 0;
				}
				if((phase >> shift) % (time * 2) < time) rampPlus += ((phase >> shift) % time);
				else rampPlus += time - ((phase >> shift) % time);
			}
			addSprite(SpriteX(resources, 103, index + i, 25 + (17 * i)),
			          SpriteY(resources, 103, index + i, teamyoffset),
			          103, index + i, rampColor, rampPlus);
		}

		int playerswithsecret = 0;
		for(int i = 0; i < team.numPeers; i++) {
			if(team.peerSlots[i].hasSecret) playerswithsecret++;
		}
		for(int i = 0; i < 3; i++) {
			Uint8 index = team.secrets > i ? 2 : 3;
			Uint8 color = 0;
			if(index == 3 && playerswithsecret > i - team.secrets && view.tickCount % 12 < 6) index = 2;
			if(team.beamingTerminalId && team.secrets == i && index == 3) {
				color = 224;
				index = 3;
			}
			addSprite(SpriteX(resources, 103, index, -(9 * (3 - i)) + 11),
			          SpriteY(resources, 103, index, teamyoffset),
			          103, index, color);
		}
		teamyoffset += 20;
	}

	CLAY({ .id = CLAY_ID("InGameHudTeamsRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		for(unsigned int i = 0; i < sprites.size(); ++i) {
			const SpriteSpec& s = sprites[i];
			int w = SpriteWidth(resources, s.bank, s.index);
			int h = SpriteHeight(resources, s.bank, s.index);
			if(w <= 0 || h <= 0) continue;
			CLAY({ .id = CLAY_IDI("HudTeamSpriteWrap", i),
			       .layout = {
				       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
			       },
			       .floating = {
				       .offset = { (float)s.x, (float)s.y },
				       .attachTo = CLAY_ATTACH_TO_ROOT,
			       },
			}) {
				CLAY({ .id = CLAY_IDI("HudTeamSprite", i),
				       .layout = {
					       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
				       },
				       .custom = { .customData = AllocSpriteCustomData({
					       s.bank, s.index, 0, 0, 0, 0, 0, 128, s.rampColor, s.rampPlus,
				       }) },
				}) {}
			}
		}
		int yoffset = 5;
		for(unsigned int i = 0; i < view.teams.size(); ++i) {
			const TeamHudView& team = view.teams[i];
			CLAY({ .id = CLAY_IDI("HudTeamEmblem", i),
			       .layout = {
				       .sizing = { CLAY_SIZING_FIXED((float)team.emblemW), CLAY_SIZING_FIXED((float)team.emblemH) },
			       },
			       .floating = {
				       .offset = { 5, (float)(yoffset + 1) },
				       .attachTo = CLAY_ATTACH_TO_ROOT,
			       },
			       .custom = { .customData = AllocTeamEmblemCustomData({
				       181, team.agency, team.color, 17, true,
			       }) },
			}) {}
			yoffset += 20;
		}
	}
	return (int)view.teams.size();
}

Uint8 BuildHudStatusSprites(Renderer& renderer, const Resources& resources,
                            Surface* surface, const PlayerHudView& player,
                            Uint8 phase) {
	Uint8 currentammo = 0;

	struct SpriteSpec {
		const char* id; int x; int y; Uint8 bank; Uint16 index;
		int srcX; int srcY; int srcW; int srcH; Uint8 brightness;
	};
	std::vector<SpriteSpec> sprites;
	auto addSprite = [&](const char* id, int x, int y, Uint8 bank, Uint16 index,
	                     int srcX = 0, int srcY = 0, int srcW = 0, int srcH = 0,
	                     Uint8 brightness = 128) {
		sprites.push_back(SpriteSpec{id, x, y, bank, index, srcX, srcY, srcW, srcH, brightness});
	};

	addSprite("HudMinimapFrame", SpriteX(resources, 94, 0), SpriteY(resources, 94, 0), 94, 0);
	if(player.fuelLow) addSprite("HudFuelLow", SpriteX(resources, 95, 8), SpriteY(resources, 95, 8), 95, 8);
	int fuelW = player.maxFuel > 0
		? (int)(((float)player.fuel / player.maxFuel) * SpriteWidth(resources, 95, 6))
		: 0;
	addSprite("HudFuelBar", SpriteX(resources, 95, 6), SpriteY(resources, 95, 6), 95, 6,
	          0, 0, fuelW, SpriteHeight(resources, 95, 6));
	addSprite("HudFuelMask", SpriteX(resources, 95, 5), SpriteY(resources, 95, 5), 95, 5);

	int healthH = SpriteHeight(resources, 95, 0);
	int healthY = player.maxHealth > 0
		? healthH - (int)(((float)player.health / player.maxHealth) * healthH)
		: healthH;
	addSprite("HudHealthBar", SpriteX(resources, 95, 0), SpriteY(resources, 95, 0) + healthY, 95, 0,
	          0, healthY, SpriteWidth(resources, 95, 0), healthH - healthY);

	int shieldH = SpriteHeight(resources, 95, 1);
	int shieldY = player.maxShield > 0
		? shieldH - (int)(((float)player.shield / player.maxShield) * shieldH)
		: shieldH;
	if(shieldY < 0) shieldY = 0;
	Uint8 shieldBrightness = 128;
	if(player.shield > player.maxShield) {
		Uint8 time = 6;
		shieldBrightness = 136;
		if(phase % (time * 2) < time) shieldBrightness += (phase % time) * 2;
		else shieldBrightness += (time - (phase % time)) * 2;
	}
	addSprite("HudShieldBar", SpriteX(resources, 95, 1), SpriteY(resources, 95, 1) + shieldY, 95, 1,
	          0, shieldY, SpriteWidth(resources, 95, 1), shieldH - shieldY, shieldBrightness);

	if(player.poisonedBy) addSprite("HudPoisoned", 183, 453, 97, 5);
	int filesW = player.maxFiles > 0
		? (int)(((float)player.files / player.maxFiles) * SpriteWidth(resources, 95, 7))
		: 0;
	addSprite("HudFilesBar", SpriteX(resources, 95, 7), SpriteY(resources, 95, 7), 95, 7,
	          0, 0, filesW, SpriteHeight(resources, 95, 7));

	Uint16 weaponFace = 1;
	Uint16 weaponGlow = 5;
	switch(player.currentWeapon) {
		case 0: currentammo = 99; weaponFace = 1; weaponGlow = 5; break;
		case 1: currentammo = player.laserAmmo; weaponFace = 2; weaponGlow = 6; break;
		case 2: currentammo = player.rocketAmmo; weaponFace = 3; weaponGlow = 7; break;
		case 3: currentammo = player.flamerAmmo; weaponFace = 4; weaponGlow = 8; break;
	}
	addSprite("HudWeaponFace", SpriteX(resources, 96, weaponFace), SpriteY(resources, 96, weaponFace), 96, weaponFace);
	addSprite("HudWeaponGlow", SpriteX(resources, 96, weaponGlow), SpriteY(resources, 96, weaponGlow), 96, weaponGlow);
	addSprite("HudWeaponSelector", SpriteX(resources, 96, 0), SpriteY(resources, 96, 0) + (player.currentWeapon * 14), 96, 0);
	if(player.health && player.maxHealth > 0 &&
	   (float)player.health / player.maxHealth <= 0.5 && phase % 8 <= 3) {
		addSprite("HudHealthWarn", SpriteX(resources, 95, 3), SpriteY(resources, 95, 3), 95, 3);
	}
	if(player.shield && player.maxShield > 0 &&
	   (float)player.shield / player.maxShield <= 0.5 && phase % 8 <= 3) {
		addSprite("HudShieldWarn", SpriteX(resources, 95, 4), SpriteY(resources, 95, 4), 95, 4);
	}
	addSprite("HudInventoryFrame", SpriteX(resources, 94, 2), SpriteY(resources, 94, 2), 94, 2);

	const int xoffsets[] = {612, 584, 556, 528};
	const int yoffsets[] = {13, 13, 11, 7};
	Uint8 inventoryIndex[4];
	Uint8 inventoryBrightness[4];
	const char* inventoryLetter[4];
	for(int i = 0; i < 4; ++i) {
		inventoryIndex[i] = renderer.InvIdToResIndex(player.inventoryItems[i]);
		inventoryLetter[i] = Renderer::InvIdToLetter(player.inventoryItems[i]);
		inventoryBrightness[i] = player.currentInventoryItem == i ? 128 : 32;
	}
	silencer::clay_bridge::BankTextDrawData letterData[4];
	for(int i = 0; i < 4; ++i) letterData[i] = { inventoryBrightness[i], false, false };

	CLAY({ .id = CLAY_ID("InGameHudStatusSpritesRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		for(unsigned int i = 0; i < sprites.size(); ++i) {
			const SpriteSpec& s = sprites[i];
			int w = s.srcW > 0 ? s.srcW : SpriteWidth(resources, s.bank, s.index);
			int h = s.srcH > 0 ? s.srcH : SpriteHeight(resources, s.bank, s.index);
			if(w <= 0 || h <= 0) continue;
			CLAY(HudFloatingElement(s.id, s.x, s.y, w, h)) {
				CLAY({ .id = CLAY_IDI("HudStatusSprite", i),
				       .layout = {
					       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
				       },
				       .custom = { .customData = AllocSpriteCustomData({
					       s.bank, s.index,
					       (Sint16)s.srcX, (Sint16)s.srcY,
					       (Sint16)s.srcW, (Sint16)s.srcH,
					       0, s.brightness, 0, 0,
				       }) },
				}) {}
			}
		}
		for(int i = 0; i < 4; ++i) {
			Uint8 invindex = inventoryIndex[i];
			int x = SpriteX(resources, 97, invindex, xoffsets[i]);
			int y = SpriteY(resources, 97, invindex, yoffsets[i]);
			int w = SpriteWidth(resources, 97, invindex);
			int h = SpriteHeight(resources, 97, invindex);
			if(w > 0 && h > 0) {
				CLAY({ .id = CLAY_IDI("HudInventoryIconWrap", (uint32_t)i),
				       .layout = {
					       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
				       },
				       .floating = {
					       .offset = { (float)x, (float)y },
					       .attachTo = CLAY_ATTACH_TO_ROOT,
				       },
				}) {
					CLAY({ .id = CLAY_IDI("HudInventoryIcon", (uint32_t)i),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
					       },
					       .custom = { .customData = AllocSpriteCustomData({
						       97, invindex, 0, 0, 0, 0, 0, inventoryBrightness[i], 0, 0,
					       }) },
					}) {}
				}
			}
			CLAY({ .id = CLAY_IDI("HudInventoryLetter", (uint32_t)i),
			       .layout = {
				       .sizing = { CLAY_SIZING_FIXED(8), CLAY_SIZING_FIXED(8) },
			       },
			       .floating = {
				       .offset = { (float)(xoffsets[i] - 2), (float)yoffsets[i] },
				       .attachTo = CLAY_ATTACH_TO_ROOT,
			       },
			}) {
				CLAY_TEXT(ClayStringFromCString(inventoryLetter[i]), CLAY_TEXT_CONFIG({
					.userData = &letterData[i],
					.textColor = { 0, 0, 0, 255 },
					.fontId = 132,
					.fontSize = 4,
				}));
			}
		}
	}
	return currentammo;
}

void BuildBuyTechOverlay(Surface* surface, const BuyTechOverlayView& view) {
	if(view.rows.empty()) return;

	using namespace silencer::ui::primitives;

	CLAY({ .id = CLAY_ID("InGameBuyTechRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
		       .padding = { 0, 0, 120, 0 },
		       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       } }) {
		CLAY(Box(BoxVariants::Chrome, {
		       .id = CLAY_ID("InGameBuyTechPanel"),
		       .layout = {
			       .sizing = { CLAY_SIZING_FIXED(345), CLAY_SIZING_FIT(0) },
			       .padding = { 10, 10, 10, 10 },
			       .childGap = 8,
			       .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .backgroundColor = { 0, 0, 0, 200 },
		})) {
			for(unsigned int i = 0; i < view.rows.size(); ++i) {
				const BuyTechRowView& row = view.rows[i];
				const Clay_ElementId rowClayId = CLAY_IDI("InGameBuyTechRow", row.index);
				silencer::ui::automation::Widget widget;
				widget.id = "ingame.buytech.row." + std::to_string(row.index);
				widget.labelText = row.name;
				widget.kind = silencer::ui::automation::WidgetKind::ListRow;
				widget.index = row.index;
				widget.selected = row.selected;
				widget.clayId = rowClayId;
				widget.hasClayId = true;
				silencer::ui::automation::Register(widget);
				if(row.selected){
					silencer::ui::automation::FocusWidgetById(widget.id);
				}
				CLAY({ .id = CLAY_IDI("InGameBuyTechRow", row.index),
				       .layout = {
					       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(25) },
					       .padding = { 6, 8, 2, 2 },
					       .childGap = 10,
					       .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
				       },
				       .backgroundColor = row.selected
					       ? Clay_Color{ 38, 0, 0, 255 }
					       : Clay_Color{ 0, 0, 0, 0 },
				}) {
					CLAY({ .id = CLAY_IDI("InGameBuyTechIcon", i),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED(33), CLAY_SIZING_FIXED(21) },
					       },
					       .custom = { .customData = AllocSpriteCustomData({
						       row.spriteBank, row.spriteIndex,
						       0, 0, 0, 0, 0, row.brightness, 0, 0,
					       }) },
					}) {}
					CLAY({ .id = CLAY_IDI("InGameBuyTechName", i),
					       .layout = {
						       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
					       } }) {
						BankText(ClayStringFromStd(row.name), BankTextVariant::Heading,
						         { .brightness = row.brightness });
					}
					CLAY({ .id = CLAY_IDI("InGameBuyTechPrice", i),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIT(0) },
						       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
					       } }) {
						BankText(ClayStringFromStd(row.price), BankTextVariant::Heading,
						         { .brightness = row.brightness });
					}
				}
			}
			CLAY({ .id = CLAY_ID("InGameBuyTechFooter"),
			       .layout = {
				       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
				       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
			       } }) {
				BankText(ClayStringFromStd(view.footer), BankTextVariant::Heading);
			}
		}
	}
}

void BuildChatOverlay(const HudView& view, Surface* surface) {
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
		silencer::ui::automation::Widget chat;
		chat.id = "ingame.chat";
		chat.labelText = "In-game chat";
		chat.kind = silencer::ui::automation::WidgetKind::TextInput;
		chat.uid = 9000;
		chat.value = player.chatText;
		chat.maxLength = player.chatTextCapacity - 1;
		chat.clayId = CLAY_ID("InGameChatPanel");
		chat.hasClayId = true;
		chat.cancelOnEscape = true;
		silencer::ui::automation::Register(chat);

		silencer::ui::automation::Widget channel;
		channel.id = "ingame.chat.channel";
		channel.labelText = player.chatWithTeam ? "Team chat" : "All chat";
		channel.kind = silencer::ui::automation::WidgetKind::Toggle;
		channel.selected = player.chatWithTeam;
		channel.clayId = CLAY_ID("InGameChatPanel");
		channel.hasClayId = true;
		silencer::ui::automation::Register(channel);

		if(!silencer::ui::automation::HasFocus()){
			silencer::ui::automation::FocusWidgetById("ingame.chat");
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
				BankText(ClayStringFromStd(lines[i]), BankTextVariant::Body,
				         { .brightness = brightness });
			}
		}
	}
}

const TeamHudView* FindTeamById(const HudView& view, Uint16 teamId) {
	for(const TeamHudView& team : view.teams){
		if(team.id == teamId) return &team;
	}
	return nullptr;
}

}  // namespace

void BuildInGameHudUi(Renderer& renderer, const Resources& resources,
                      const HudView& view, Surface* surface) {
	if(!view.mapLoaded) return;
	if(!view.localPlayer.valid) return;

	const PlayerHudView& player = view.viewedPlayer;
	Uint8 phase = renderer.GetHudAnimationPhase();

	if(view.systemCamera[0].active){
		BuildHudSystemCameraFrame(resources, surface, 95, 2, 92, 381);
	}
	if(view.systemCamera[1].active){
		BuildHudSystemCameraFrame(resources, surface, 95, 11, 92, 318);
	}

	if(!player.valid) return;

	Uint8 currentammo = BuildHudStatusSprites(renderer, resources, surface, player, phase);
	BuildHudReadouts(player, currentammo, surface);

	int teamCount = BuildHudTeams(resources, surface, view, phase);
	const TeamHudView* team = FindTeamById(view, player.teamId);

	if(team && team->baseDoorId){
		int yoffset = 60;
		if(teamCount >= 3) yoffset += (teamCount * 20) - 65;
		BuildHudSecretSprites(resources, surface, view, *team, yoffset, phase);
		if(!team->beamingTerminalId){
			BuildHudSecretProgress(surface, player, yoffset, team->secretProgress, phase);
		}
	}

	Uint8 tracetime = 0;
	if(team && team->beamingTerminalId && team->beamingTerminalTraceTime > 0){
		tracetime = team->beamingTerminalTraceTime;
	}
	if(player.tracetime > 0) tracetime = player.tracetime;
	if(tracetime > 0) BuildHudTraceTime(tracetime, surface);

	if(view.buyTech.visible){
		BuildBuyTechOverlay(surface, view.buyTech);
	}

	if(view.showChatTicks || player.chatActive){
		BuildChatOverlay(view, surface);
	}
}

}  // namespace client_ui
}  // namespace silencer
