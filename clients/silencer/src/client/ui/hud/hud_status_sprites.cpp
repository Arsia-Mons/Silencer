#include "client/ui/hud/hud_status_sprites.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "render/clay_ui_payloads.h"
#include "render/renderer.h"
#include "resources.h"
#include "surface.h"

#include <vector>

namespace silencer {
namespace client_ui {

Uint8 BuildHudStatusSprites(const PlayerHudView& player, Surface* surface,
                            const Resources& resources, Renderer& renderer,
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

}  // namespace client_ui
}  // namespace silencer
