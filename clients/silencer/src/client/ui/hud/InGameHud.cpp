#include "client/ui/hud/InGameHud.h"

#include "basedoor.h"
#include "buyableitem.h"
#include "camera.h"
#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "detonator.h"
#include "lobby.h"
#include "objecttypes.h"
#include "player.h"
#include "render/renderer.h"
#include "render/clay_ui_payloads.h"
#include "surface.h"
#include "team.h"
#include "terminal.h"
#include "ui/primitives/bank_text.h"
#include "ui/primitives/box.h"
#include "ui/runtime/UiAutomationRegistry.h"
#include "user.h"
#include "world.h"

#include <SDL3/SDL_timer.h>

#include <cstring>
#include <list>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {
Clay_String ClayStringFromStd(const std::string& text) {
	return Clay_String{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(text.size()),
		.chars = text.c_str(),
	};
}

	constexpr int kSpritePayloadCapacity = 512;
	silencer::clay_bridge::SpritePayload g_spritePayloads[kSpritePayloadCapacity];
	silencer::clay_bridge::ClayCustomData g_spriteCustomData[kSpritePayloadCapacity];
	int g_spritePayloadCount = 0;

	constexpr int kTeamEmblemPayloadCapacity = 64;
	silencer::clay_bridge::TeamEmblemPayload g_teamEmblemPayloads[kTeamEmblemPayloadCapacity];
	silencer::clay_bridge::ClayCustomData g_teamEmblemCustomData[kTeamEmblemPayloadCapacity];
	int g_teamEmblemPayloadCount = 0;

	void HudPayloadBeginFrame() {
		g_spritePayloadCount = 0;
		g_teamEmblemPayloadCount = 0;
	}

	silencer::clay_bridge::ClayCustomData* AllocSpriteCustomData(
		silencer::clay_bridge::SpritePayload payload) {
	if(g_spritePayloadCount >= kSpritePayloadCapacity) return nullptr;
	g_spritePayloads[g_spritePayloadCount] = payload;
	g_spriteCustomData[g_spritePayloadCount] = {
		silencer::clay_bridge::CustomKind::Sprite,
		&g_spritePayloads[g_spritePayloadCount],
		};
		return &g_spriteCustomData[g_spritePayloadCount++];
	}

	silencer::clay_bridge::ClayCustomData* AllocTeamEmblemCustomData(
		silencer::clay_bridge::TeamEmblemPayload payload) {
		if(g_teamEmblemPayloadCount >= kTeamEmblemPayloadCapacity) return nullptr;
		g_teamEmblemPayloads[g_teamEmblemPayloadCount] = payload;
		g_teamEmblemCustomData[g_teamEmblemPayloadCount] = {
			silencer::clay_bridge::CustomKind::TeamEmblem,
			&g_teamEmblemPayloads[g_teamEmblemPayloadCount],
		};
		return &g_teamEmblemCustomData[g_teamEmblemPayloadCount++];
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

	Clay_ElementDeclaration HudFloatingElementI(const char* id, uint32_t index, int x, int y, int w, int h) {
		Clay_ElementDeclaration decl = HudFloatingElement(id, x, y, w, h);
		Clay_String idString{
			.isStaticallyAllocated = false,
			.length = static_cast<int32_t>(std::strlen(id)),
			.chars = id,
		};
		decl.id = Clay_GetElementIdWithIndex(idString, index);
		return decl;
	}

	int SpriteWidth(World& world, Uint8 bank, Uint16 index) {
		if(bank >= world.resources.spritebank.size()) return 0;
		if(index >= world.resources.spritebank[bank].size()) return 0;
		Surface* sprite = world.resources.spritebank[bank][index].get();
		return sprite ? sprite->w : 0;
	}

	int SpriteHeight(World& world, Uint8 bank, Uint16 index) {
		if(bank >= world.resources.spritebank.size()) return 0;
		if(index >= world.resources.spritebank[bank].size()) return 0;
		Surface* sprite = world.resources.spritebank[bank][index].get();
		return sprite ? sprite->h : 0;
	}

	int SpriteX(World& world, Uint8 bank, Uint16 index, int logicalX = 0) {
		return logicalX - world.resources.spriteoffsetx[bank][index];
	}

	int SpriteY(World& world, Uint8 bank, Uint16 index, int logicalY = 0) {
		return logicalY - world.resources.spriteoffsety[bank][index];
	}

	struct BuyTechRow {
		BuyableItem* item;
		int index;
		std::string name;
		std::string price;
		bool selected;
		Uint8 brightness;
	};

	void BuildHudSystemCameraFrame(World& world, Surface* surface,
	                               Uint8 bank, Uint16 index, Uint8 offsetBank,
	                               int logicalY) {
		if(bank >= world.resources.spritebank.size()) return;
		if(index >= world.resources.spritebank[bank].size()) return;
		Surface* sprite = world.resources.spritebank[bank][index].get();
		if(!sprite) return;
		int x = -world.resources.spriteoffsetx[bank][index];
		int y = -world.resources.spriteoffsety[offsetBank][index] + logicalY;

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
				       bank,
				       index,
				       0,
				       0,
				       0,
				       0,
				       0,
				       128,
				       0,
				       0,
			       }) },
			}) {}
		}
	}

	void BuildHudReadouts(Player* player, Uint8 currentammo, Surface* surface) {
		if(!player) return;

		std::string currentAmmo = std::string(currentammo < 10 ? " " : "") + std::to_string(currentammo);
		std::string blasterAmmo = "99";
		std::string laserAmmo = player->laserammo > 0
			? std::string(player->laserammo < 10 ? " " : "") + std::to_string(player->laserammo)
			: "";
		std::string rocketAmmo = player->rocketammo > 0
			? std::string(player->rocketammo < 10 ? " " : "") + std::to_string(player->rocketammo)
			: "";
		std::string flamerAmmo = player->flamerammo > 0
			? std::string(player->flamerammo < 10 ? " " : "") + std::to_string(player->flamerammo)
			: "";
		std::string credits = std::to_string(player->credits);
		std::string health = std::to_string(player->health);
		std::string shield = std::to_string(player->shield);
		std::string inventoryCounts[4];
		for(int i = 0; i < 4; ++i) {
			if(player->inventoryitemsnum[i] > 1) {
				inventoryCounts[i] = std::to_string(player->inventoryitemsnum[i]);
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

	void BuildHudSecretSprites(World& world, Surface* surface, Team* team, int yoffset, Uint8 phase) {
		if(!team) return;
		struct SpriteSpec {
			int x;
			int y;
			Uint8 bank;
			Uint16 index;
			Uint8 brightness;
		};
		std::vector<SpriteSpec> sprites;
		auto addSprite = [&](int x, int y, Uint8 bank, Uint16 index, Uint8 brightness = 128) {
			sprites.push_back(SpriteSpec{x, y, bank, index, brightness});
		};

		Uint16 backgroundIndex = team->beamingterminalid ? 1 : 0;
		addSprite(SpriteX(world, 187, backgroundIndex),
		          SpriteY(world, 187, backgroundIndex, yoffset),
		          187, backgroundIndex);

		Uint8 highlightBrightness = 120;
		if(phase % 32 < 16) highlightBrightness += (phase % 16);
		else highlightBrightness += 16 - (phase % 16);
		if(world.highlightsecrets) {
			addSprite(SpriteX(world, 86, 2), SpriteY(world, 86, 2, yoffset), 86, 2, highlightBrightness);
		}
		if(world.highlightminimap) {
			addSprite(SpriteX(world, 86, 1), SpriteY(world, 86, 1), 86, 1, highlightBrightness);
		}

		CLAY({ .id = CLAY_ID("InGameHudSecretSpritesRoot"),
		       .layout = {
			       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
		       } }) {
			for(unsigned int i = 0; i < sprites.size(); ++i) {
				const SpriteSpec& s = sprites[i];
				int w = SpriteWidth(world, s.bank, s.index);
				int h = SpriteHeight(world, s.bank, s.index);
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

	void BuildHudSecretProgress(Surface* surface, Player* player, int yoffset,
	                            int secretprogress, Uint8 phase) {
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
			if(secretprogress < ((player->state == Player::HACKING && player->state_i == 16 && phase % 16 < 8) ? -20 : 0)) {
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

	int BuildHudTeams(World& world, Surface* surface, Uint8 phase) {
		std::vector<Team*> teams;
		for(std::vector<Uint16>::iterator it = world.objectsbytype[ObjectTypes::TEAM].begin();
		    it != world.objectsbytype[ObjectTypes::TEAM].end(); it++) {
			Team* team = static_cast<Team*>(world.GetObjectFromId((*it)));
			if(team) teams.push_back(team);
		}
		if(teams.empty()) return 0;

		struct SpriteSpec {
			int x;
			int y;
			Uint8 bank;
			Uint16 index;
			Uint8 rampColor;
			Uint8 rampPlus;
		};
		std::vector<SpriteSpec> sprites;
		auto addSprite = [&](int x, int y, Uint8 bank, Uint16 index,
		                     Uint8 rampColor = 0, Uint8 rampPlus = 0) {
			sprites.push_back(SpriteSpec{x, y, bank, index, rampColor, rampPlus});
		};

		if(teams.size() == 1) {
			addSprite(SpriteX(world, 94, 1), SpriteY(world, 94, 1), 94, 1);
		}else{
			addSprite(SpriteX(world, 103, 0),
			          SpriteY(world, 103, 0, -133 + ((int)teams.size() - 1) * 20),
			          103, 0);
			addSprite(SpriteX(world, 103, 1), SpriteY(world, 103, 1), 103, 1);
		}

		int teamyoffset = 5;
		for(std::vector<Team*>::iterator it = teams.begin(); it != teams.end(); ++it) {
			Team* team = *it;
			for(int i = 0; i < team->numpeers; i++) {
				if(world.peerlist[team->peers[i]]) {
					Player* peerplayer = world.GetPeerPlayer(world.peerlist[team->peers[i]]->id);
					if(peerplayer) {
						Uint8 index = (peerplayer->state == Player::DEAD || peerplayer->state == Player::DYING ? 8 : 4);
						Uint8 rampColor = 0;
						Uint8 rampPlus = 0;
						if(peerplayer->InBase(world) || peerplayer->hassecret) {
							Uint8 time = 4;
							Uint8 shift = 2;
							rampColor = 210;
							if(peerplayer->hassecret) {
								time = 8;
								rampColor = 114;
								shift = 0;
							}
							if((phase >> shift) % (time * 2) < time) rampPlus += ((phase >> shift) % time);
							else rampPlus += time - ((phase >> shift) % time);
						}
						addSprite(SpriteX(world, 103, index + i, 25 + (17 * i)),
						          SpriteY(world, 103, index + i, teamyoffset),
						          103, index + i, rampColor, rampPlus);
					}
				}
			}

			int playerswithsecret = 0;
			for(int i = 0; i < team->numpeers; i++) {
				Peer* peer = world.peerlist[team->peers[i]];
				if(peer) {
					Player* peerplayer = world.GetPeerPlayer(peer->id);
					if(peerplayer && peerplayer->hassecret) playerswithsecret++;
				}
			}
			for(int i = 0; i < 3; i++) {
				Uint8 index = team->secrets > i ? 2 : 3;
				Uint8 color = 0;
				if(index == 3 && playerswithsecret > i - team->secrets && world.tickcount % 12 < 6) index = 2;
				if(team->beamingterminalid && team->secrets == i && index == 3) {
					color = 224;
					index = 3;
				}
				addSprite(SpriteX(world, 103, index, -(9 * (3 - i)) + 11),
				          SpriteY(world, 103, index, teamyoffset),
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
				int w = SpriteWidth(world, s.bank, s.index);
				int h = SpriteHeight(world, s.bank, s.index);
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
			for(unsigned int i = 0; i < teams.size(); ++i) {
				Team* team = teams[i];
				Surface* emblem = nullptr;
				if(181 < world.resources.spritebank.size() &&
				   team->agency < world.resources.spritebank[181].size()) {
					emblem = world.resources.spritebank[181][team->agency].get();
				}
				int w = emblem ? emblem->w * 2 : 32;
				int h = emblem ? emblem->h * 2 : 32;
				CLAY({ .id = CLAY_IDI("HudTeamEmblem", i),
				       .layout = {
					       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
				       },
				       .floating = {
					       .offset = { 5, (float)(yoffset + 1) },
					       .attachTo = CLAY_ATTACH_TO_ROOT,
				       },
				       .custom = { .customData = AllocTeamEmblemCustomData({
					       181, team->agency, team->GetColor(), 17, true,
				       }) },
				}) {}
				yoffset += 20;
			}
		}
		return (int)teams.size();
	}

	Uint8 BuildHudStatusSprites(Renderer& renderer, World& world, Surface* surface,
	                            Player* player, Uint8 phase) {
		Uint8 currentammo = 0;
		if(!player) return currentammo;

		struct SpriteSpec {
			const char* id;
			int x;
			int y;
			Uint8 bank;
			Uint16 index;
			int srcX;
			int srcY;
			int srcW;
			int srcH;
			Uint8 brightness;
		};
		std::vector<SpriteSpec> sprites;
		auto addSprite = [&](const char* id, int x, int y, Uint8 bank, Uint16 index,
		                     int srcX = 0, int srcY = 0, int srcW = 0, int srcH = 0,
		                     Uint8 brightness = 128) {
			sprites.push_back(SpriteSpec{id, x, y, bank, index, srcX, srcY, srcW, srcH, brightness});
		};

		addSprite("HudMinimapFrame", SpriteX(world, 94, 0), SpriteY(world, 94, 0), 94, 0);
		if(player->fuellow) addSprite("HudFuelLow", SpriteX(world, 95, 8), SpriteY(world, 95, 8), 95, 8);
		int fuelW = (int)(((float)player->fuel / player->maxfuel) * SpriteWidth(world, 95, 6));
		addSprite("HudFuelBar", SpriteX(world, 95, 6), SpriteY(world, 95, 6), 95, 6,
		          0, 0, fuelW, SpriteHeight(world, 95, 6));
		addSprite("HudFuelMask", SpriteX(world, 95, 5), SpriteY(world, 95, 5), 95, 5);

		int healthH = SpriteHeight(world, 95, 0);
		int healthY = healthH - (int)(((float)player->health / player->maxhealth) * healthH);
		addSprite("HudHealthBar", SpriteX(world, 95, 0), SpriteY(world, 95, 0) + healthY, 95, 0,
		          0, healthY, SpriteWidth(world, 95, 0), healthH - healthY);

		int shieldH = SpriteHeight(world, 95, 1);
		int shieldY = shieldH - (int)(((float)player->shield / player->maxshield) * shieldH);
		if(shieldY < 0) shieldY = 0;
		Uint8 shieldBrightness = 128;
		if(player->shield > player->maxshield) {
			Uint8 time = 6;
			shieldBrightness = 136;
			if(phase % (time * 2) < time) shieldBrightness += (phase % time) * 2;
			else shieldBrightness += (time - (phase % time)) * 2;
		}
		addSprite("HudShieldBar", SpriteX(world, 95, 1), SpriteY(world, 95, 1) + shieldY, 95, 1,
		          0, shieldY, SpriteWidth(world, 95, 1), shieldH - shieldY, shieldBrightness);

		if(player->poisonedby) addSprite("HudPoisoned", 183, 453, 97, 5);
		int filesW = (int)(((float)player->files / player->maxfiles) * SpriteWidth(world, 95, 7));
		addSprite("HudFilesBar", SpriteX(world, 95, 7), SpriteY(world, 95, 7), 95, 7,
		          0, 0, filesW, SpriteHeight(world, 95, 7));

		Uint16 weaponFace = 1;
		Uint16 weaponGlow = 5;
		switch(player->currentweapon) {
			case 0: currentammo = 99; weaponFace = 1; weaponGlow = 5; break;
			case 1: currentammo = player->laserammo; weaponFace = 2; weaponGlow = 6; break;
			case 2: currentammo = player->rocketammo; weaponFace = 3; weaponGlow = 7; break;
			case 3: currentammo = player->flamerammo; weaponFace = 4; weaponGlow = 8; break;
		}
		addSprite("HudWeaponFace", SpriteX(world, 96, weaponFace), SpriteY(world, 96, weaponFace), 96, weaponFace);
		addSprite("HudWeaponGlow", SpriteX(world, 96, weaponGlow), SpriteY(world, 96, weaponGlow), 96, weaponGlow);
		addSprite("HudWeaponSelector", SpriteX(world, 96, 0), SpriteY(world, 96, 0) + (player->currentweapon * 14), 96, 0);
		if(player->health && (float)player->health / player->maxhealth <= 0.5 && phase % 8 <= 3) {
			addSprite("HudHealthWarn", SpriteX(world, 95, 3), SpriteY(world, 95, 3), 95, 3);
		}
		if(player->shield && (float)player->shield / player->maxshield <= 0.5 && phase % 8 <= 3) {
			addSprite("HudShieldWarn", SpriteX(world, 95, 4), SpriteY(world, 95, 4), 95, 4);
		}
		addSprite("HudInventoryFrame", SpriteX(world, 94, 2), SpriteY(world, 94, 2), 94, 2);

		const int xoffsets[] = {612, 584, 556, 528};
		const int yoffsets[] = {13, 13, 11, 7};
		Uint8 inventoryIndex[4];
		Uint8 inventoryBrightness[4];
		const char* inventoryLetter[4];
		for(int i = 0; i < 4; ++i) {
			inventoryIndex[i] = renderer.InvIdToResIndex(player->inventoryitems[i]);
			inventoryLetter[i] = Renderer::InvIdToLetter(player->inventoryitems[i]);
			inventoryBrightness[i] = player->currentinventoryitem == i ? 128 : 32;
		}
		silencer::clay_bridge::BankTextDrawData letterData[4];
		for(int i = 0; i < 4; ++i) letterData[i] = { inventoryBrightness[i], false, false };

		CLAY({ .id = CLAY_ID("InGameHudStatusSpritesRoot"),
		       .layout = {
			       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
		       } }) {
			for(unsigned int i = 0; i < sprites.size(); ++i) {
				const SpriteSpec& s = sprites[i];
				int w = s.srcW > 0 ? s.srcW : SpriteWidth(world, s.bank, s.index);
				int h = s.srcH > 0 ? s.srcH : SpriteHeight(world, s.bank, s.index);
				if(w <= 0 || h <= 0) continue;
				CLAY(HudFloatingElement(s.id, s.x, s.y, w, h)) {
					CLAY({ .id = CLAY_IDI("HudStatusSprite", i),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
					       },
					       .custom = { .customData = AllocSpriteCustomData({
						       s.bank,
						       s.index,
						       (Sint16)s.srcX,
						       (Sint16)s.srcY,
						       (Sint16)s.srcW,
						       (Sint16)s.srcH,
						       0,
						       s.brightness,
						       0,
						       0,
					       }) },
					}) {}
				}
			}
			for(int i = 0; i < 4; ++i) {
				Uint8 invindex = inventoryIndex[i];
				int x = SpriteX(world, 97, invindex, xoffsets[i]);
				int y = SpriteY(world, 97, invindex, yoffsets[i]);
				int w = SpriteWidth(world, 97, invindex);
				int h = SpriteHeight(world, 97, invindex);
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

	void BuildBuyTechOverlay(Surface* surface,
	                         const std::vector<BuyTechRow>& rows,
	                         const std::string& footer) {
		if(rows.empty()) return;

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
				for(unsigned int i = 0; i < rows.size(); ++i) {
					const BuyTechRow& row = rows[i];
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
							       row.item->res_bank,
							       row.item->res_index,
							       0,
							       0,
							       0,
							       0,
							       0,
							       row.brightness,
							       0,
							       0,
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
					BankText(ClayStringFromStd(footer), BankTextVariant::Heading);
				}
			}
		}
	}

	void BuildChatOverlay(World& world, Surface* surface, Player* player) {
		using namespace silencer::ui::primitives;

		std::vector<std::string> lines;
		for(int i = 0; i < (int)world.chatlines.size(); i++) {
			if(player->chatActive && i == 0 && world.chatlines.size() == 5) {
				continue;
			}
			lines.push_back(world.chatlines[i].substr(0, 36));
		}
		std::string inputPrefix;
		std::string inputText;
		if(player->chatActive) {
			inputPrefix = player->chatwithteam ? "(TEAM):" : "(ALL):";
			inputText = inputPrefix + player->chatText;
			if((SDL_GetTicks() / 50) % 32 < 16) {
				inputText.push_back('|');
			}
			lines.push_back(inputText);
		}
		if(lines.empty()) return;

		int panelH = 22 + ((int)lines.size() * 10);
		if(panelH < 42) panelH = 42;
		if(player->chatActive){
			silencer::ui::automation::Widget chat;
			chat.id = "ingame.chat";
			chat.labelText = "In-game chat";
			chat.kind = silencer::ui::automation::WidgetKind::TextInput;
			chat.uid = 9000;
			chat.value = player->chatText;
			chat.maxLength = static_cast<int>(sizeof(player->chatText)) - 1;
			chat.clayId = CLAY_ID("InGameChatPanel");
			chat.hasClayId = true;
			chat.cancelOnEscape = true;
			silencer::ui::automation::Register(chat);

			silencer::ui::automation::Widget channel;
			channel.id = "ingame.chat.channel";
			channel.labelText = player->chatwithteam ? "Team chat" : "All chat";
			channel.kind = silencer::ui::automation::WidgetKind::Toggle;
			channel.selected = player->chatwithteam;
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
					Uint8 brightness = (player->chatActive && i == (int)lines.size() - 1) ? 128 : 136;
					BankText(ClayStringFromStd(lines[i]), BankTextVariant::Body,
					         { .brightness = brightness });
				}
			}
		}
	}

void BuildInGameHudUi(Renderer& renderer, World& world, Surface* surface, float frametime) {
	HudPayloadBeginFrame();
	Uint8 phase = renderer.GetHudAnimationPhase();
	Player* localplayer = world.GetPeerPlayer(world.localpeerid);
	Player* player = nullptr;
	// Drive HUD from the focus peer (viewedpeerid) so spectators see the
	// followed player's health/fuel/shield/files as-is.
	Peer* peer = world.peerlist[world.viewedpeerid];
	if(peer) {
		for(std::list<Uint16>::iterator it = peer->controlledlist.begin(); it != peer->controlledlist.end(); it++) {
			Object* object = world.GetObjectFromId(*it);
			if(object && object->type == ObjectTypes::PLAYER) {
				player = static_cast<Player*>(object);
			}
		}
	}
	Renderer::Rect dstrect;

	if(localplayer) {
		if(world.systemcameraactive[0]) {
			BuildHudSystemCameraFrame(world, surface, 95, 2, 92, 381);

			Surface systemscreen(135, 44, 1);
			Camera camera(135 * 2, 44 * 2);
			Object* followobject = world.GetObjectFromId(world.systemcamerafollow[0]);
			int px = 0;
			int py = 0;
			if(followobject) {
				py = followobject->y + ((followobject->oldy - followobject->y) * frametime);
				px = followobject->x + ((followobject->oldx - followobject->x) * frametime);
			}
			camera.Follow(world, px + world.systemcamerax[0], py + world.systemcameray[0], 0, 0, 0, 0);
			renderer.DrawWorldScaled(&systemscreen, camera, 3, frametime);
			renderer.EffectRampColor(&systemscreen, 0, 190);
			dstrect.x = 5;
			dstrect.y = 349;
			Renderer::BlitSurface(&systemscreen, 0, surface, &dstrect);
		}
		if(world.systemcameraactive[1]) {
			BuildHudSystemCameraFrame(world, surface, 95, 11, 92, 318);

			Surface systemscreen(135, 44, 1);
			Camera camera(135 * 2, 44 * 2);
			Object* followobject = world.GetObjectFromId(world.systemcamerafollow[1]);
			int px = 0;
			int py = 0;
			if(followobject) {
				px = followobject->x + ((followobject->oldx - followobject->x) * frametime);
				py = followobject->y + ((followobject->oldy - followobject->y) * frametime);
				if(followobject->type == ObjectTypes::DETONATOR) {
					Detonator* detonator = static_cast<Detonator*>(followobject);
					if(detonator->HasDetonated() && py < detonator->lowestypos) {
						py = detonator->lowestypos;
					}
				}
			}
			camera.Follow(world, px + world.systemcamerax[1], py + world.systemcameray[1], 0, 0, 0, 0);
			renderer.DrawWorldScaled(&systemscreen, camera, 3, frametime);
			renderer.EffectRampColor(&systemscreen, 0, 190);
			dstrect.x = 500;
			dstrect.y = 348;
			Renderer::BlitSurface(&systemscreen, 0, surface, &dstrect);
		}

		dstrect.x = 235;
		dstrect.y = 419;
		Renderer::BlitSurface(&world.map.minimap.surface, 0, surface, &dstrect);
		if(player) {
			Uint8 currentammo = BuildHudStatusSprites(renderer, world, surface, player, phase);
			BuildHudReadouts(player, currentammo, surface);

			int teamCount = BuildHudTeams(world, surface, phase);
			Team* team = player->GetTeam(world);

			if(team && team->basedoorid) {
				int secretprogress = team->secretprogress;

				int yoffset = 60;
				if(teamCount >= 3) {
					yoffset += (teamCount * 20) - 65;
				}
				BuildHudSecretSprites(world, surface, team, yoffset, phase);
				if(!team->beamingterminalid) {
					BuildHudSecretProgress(surface, player, yoffset, secretprogress, phase);
				}
			}

			Uint8 tracetime = 0;
			if(team && team->beamingterminalid) {
				Terminal* terminal = static_cast<Terminal*>(world.GetObjectFromId(team->beamingterminalid));
				if(terminal && terminal->tracetime > 0) {
					tracetime = terminal->tracetime;
				}
			}
			if(player->tracetime > 0) {
				tracetime = player->tracetime;
			}
			if(tracetime > 0) {
				BuildHudTraceTime(tracetime, surface);
			}

			if(player->isbuying || player->techstationactive) {
				std::vector<BuyableItem*> menuitems;
				player->CollectBuyMenuItems(world, player->techstationactive, menuitems);
				Team* buyTechTeam = static_cast<Team*>(world.GetObjectFromId(player->teamid));
				auto itemName = [&world, buyTechTeam](BuyableItem* item) -> std::string {
					std::string name = item->name;
					int peerIndex = -1;
					if(item->id == World::BUY_GIVE0) peerIndex = 0;
					if(item->id == World::BUY_GIVE1) peerIndex = 1;
					if(item->id == World::BUY_GIVE2) peerIndex = 2;
					if(item->id == World::BUY_GIVE3) peerIndex = 3;
					if(peerIndex >= 0 && buyTechTeam) {
						Peer* targetPeer = world.peerlist[buyTechTeam->peers[peerIndex]];
						if(targetPeer) {
							User* user = world.lobby.GetUserInfo(targetPeer->accountid);
							if(user) {
								name += user->name;
							}
						}
					}
					return name;
				};
				auto itemPrice = [&world, player, buyTechTeam](BuyableItem* item) -> std::string {
					if(player->isbuying) {
						if(buyTechTeam && (buyTechTeam->disabledtech & item->techchoice)) {
							return "DOWN";
						}
						return std::to_string(item->price);
					}
					if(!player->InOwnBase(world)) {
						BaseDoor* basedoor =
							static_cast<BaseDoor*>(world.GetObjectFromId(player->basedoorentering));
						if(basedoor) {
							Team* otherteam = static_cast<Team*>(world.GetObjectFromId(basedoor->teamid));
							if(otherteam && (otherteam->disabledtech & item->techchoice)) {
								return "DOWN";
							}
						}
						return std::to_string(item->repairprice);
					}
					if(buyTechTeam && (buyTechTeam->disabledtech & item->techchoice)) {
						return std::to_string(item->repairprice);
					}
					return "UP";
				};

				int selecteditem = player->isbuying ? player->buyifacelastitem : player->techifacelastitem;
				int scrolled = player->isbuying ? player->buyifacelastscrolled : player->techifacelastscrolled;
				if(selecteditem < 0) selecteditem = 0;
				if(selecteditem >= (int)menuitems.size()) selecteditem = (int)menuitems.size() - 1;
				if(scrolled < 0) scrolled = 0;

				Uint8 uiTick = static_cast<Uint8>(SDL_GetTicks() / 50);
				Uint8 selectedBrightness = 128;
				if(uiTick % 16 >= 8) {
					selectedBrightness += (uiTick % 8);
				}else{
					selectedBrightness += 8 - (uiTick % 8);
				}

				std::vector<BuyTechRow> rows;
				for(int i = scrolled; i < (int)menuitems.size() && (int)rows.size() < 5; ++i) {
					BuyableItem* item = menuitems[i];
					bool selected = i == selecteditem;
					rows.push_back(BuyTechRow{
						item,
						i,
						itemName(item),
						itemPrice(item),
						selected,
						selected ? selectedBrightness : (Uint8)128,
					});
				}

				std::string footer = (player->isbuying || player->InOwnBase(world))
					? "Available Credits: " + std::to_string(player->credits)
					: "Viruses Available: " + std::to_string(player->InventoryItemCount(Player::INV_VIRUS));
				BuildBuyTechOverlay(surface, rows, footer);
			}

			if(world.showchat_i || player->chatActive) {
				BuildChatOverlay(world, surface, player);
			}
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
