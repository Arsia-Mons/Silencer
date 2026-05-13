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
#include "user.h"
#include "world.h"

#include <SDL3/SDL_timer.h>

#include <list>
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

constexpr int kSpritePayloadCapacity = 128;
silencer::clay_bridge::SpritePayload g_spritePayloads[kSpritePayloadCapacity];
silencer::clay_bridge::ClayCustomData g_spriteCustomData[kSpritePayloadCapacity];
int g_spritePayloadCount = 0;

void SpriteBeginFrame() {
	g_spritePayloadCount = 0;
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

struct BuyTechRow {
	BuyableItem* item;
	std::string name;
	std::string price;
	bool selected;
	Uint8 brightness;
};

void DrawBuyTechOverlayClay(Renderer& renderer,
                            World& world,
                            Surface* surface,
                            const std::vector<BuyTechRow>& rows,
                            const std::string& footer) {
	if(rows.empty()) return;

	using namespace silencer::ui::primitives;
	silencer::clay_bridge::EnsureInitialized(surface->w, surface->h);
	Clay_SetPointerState({ -1.0f, -1.0f }, false);
	BoxBeginFrame();
	BankTextBeginFrame();
	SpriteBeginFrame();
	Clay_BeginLayout();

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
				CLAY({ .id = CLAY_IDI("InGameBuyTechRow", i),
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

	Clay_RenderCommandArray cmds = Clay_EndLayout();
	silencer::clay_bridge::Render(world.resources, renderer, surface, cmds);
}

void DrawChatOverlayClay(Renderer& renderer, World& world, Surface* surface, Player* player) {
	using namespace silencer::clay_bridge;
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

	EnsureInitialized(surface->w, surface->h);
	Clay_SetPointerState({ -1.0f, -1.0f }, false);
	BoxBeginFrame();
	BankTextBeginFrame();

	int panelH = 22 + ((int)lines.size() * 10);
	if(panelH < 42) panelH = 42;

	Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("InGameChatRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)surface->w),
	                       CLAY_SIZING_FIXED((float)surface->h) },
	           .padding = { 0, 9, 0, 160 },
	           .childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_BOTTOM },
	       } }) {
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
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	Render(world.resources, renderer, surface, cmds);
}

}  // namespace

void DrawInGameHud(Renderer& renderer, World& world, Surface* surface, float frametime) {
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
			renderer.DrawHudSystemCameraFrameClay(surface, 95, 2, 92, 381);

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
			renderer.DrawHudSystemCameraFrameClay(surface, 95, 11, 92, 318);

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
			Uint8 currentammo = renderer.DrawHudStatusSpritesClay(surface, player);
			renderer.DrawHudReadoutsClay(surface, player, currentammo);

			int teamCount = renderer.DrawHudTeamsClay(surface);
			Team* team = player->GetTeam(world);

			if(team && team->basedoorid) {
				int secretprogress = team->secretprogress;

				int yoffset = 60;
				if(teamCount >= 3) {
					yoffset += (teamCount * 20) - 65;
				}
				renderer.DrawHudSecretSpritesClay(surface, team, yoffset);
				if(!team->beamingterminalid) {
					renderer.DrawHudSecretProgressClay(surface, player, yoffset, secretprogress);
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
				renderer.DrawHudTraceTimeClay(surface, tracetime);
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
						itemName(item),
						itemPrice(item),
						selected,
						selected ? selectedBrightness : (Uint8)128,
					});
				}

				std::string footer = (player->isbuying || player->InOwnBase(world))
					? "Available Credits: " + std::to_string(player->credits)
					: "Viruses Available: " + std::to_string(player->InventoryItemCount(Player::INV_VIRUS));
				DrawBuyTechOverlayClay(renderer, world, surface, rows, footer);
			}

			if(world.showchat_i || player->chatActive) {
				DrawChatOverlayClay(renderer, world, surface, player);
			}
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
