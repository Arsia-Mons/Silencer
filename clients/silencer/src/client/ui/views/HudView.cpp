#include "client/ui/views/HudView.h"

#include "basedoor.h"
#include "buyableitem.h"
#include "lobby.h"
#include "objecttypes.h"
#include "peer.h"
#include "player.h"
#include "team.h"
#include "terminal.h"
#include "user.h"
#include "world.h"

#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

namespace hudview_detail {

constexpr int kChatX = 400;
constexpr int kChatY = 280;
constexpr int kChatW = 231;
constexpr int kChatChromeH = 70;
constexpr int kChatBackgroundInteriorH = 30;
constexpr int kChatBackgroundBank = 188;
constexpr int kChatTextStartY = 10;
constexpr int kChatLineStepY = 10;
constexpr int kChatMaxHistoryChars = 36;
constexpr int kChatRightCapX = 36;
constexpr Uint8 kPlayerStateHacking = 15;  // matches Player::HACKING

int SpriteWidth(const ::Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spritebank.size() || index >= res.spritebank[bank].size()){
		return 0;
	}
	Surface* sprite = res.spritebank[bank][index].get();
	if(!sprite) return 0;
	if(bank < res.spritewidth.size() && index < res.spritewidth[bank].size()){
		return static_cast<int>(res.spritewidth[bank][index]);
	}
	return sprite->w;
}

int SpriteHeight(const ::Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spritebank.size() || index >= res.spritebank[bank].size()){
		return 0;
	}
	Surface* sprite = res.spritebank[bank][index].get();
	if(!sprite) return 0;
	if(bank < res.spriteheight.size() && index < res.spriteheight[bank].size()){
		return static_cast<int>(res.spriteheight[bank][index]);
	}
	return sprite->h;
}

int SpriteOffsetX(const ::Resources& res, Uint8 bank, Uint16 index) {
	if(bank < res.spriteoffsetx.size() && index < res.spriteoffsetx[bank].size()){
		return res.spriteoffsetx[bank][index];
	}
	return 0;
}

int SpriteOffsetY(const ::Resources& res, Uint8 bank, Uint16 index) {
	if(bank < res.spriteoffsety.size() && index < res.spriteoffsety[bank].size()){
		return res.spriteoffsety[bank][index];
	}
	return 0;
}

int SpriteX(const ::Resources& res, Uint8 bank, Uint16 index, int logicalX = 0) {
	return logicalX + SpriteOffsetX(res, bank, index);
}

int SpriteY(const ::Resources& res, Uint8 bank, Uint16 index, int logicalY = 0) {
	return logicalY + SpriteOffsetY(res, bank, index);
}

std::string PaddedByte(Uint8 value) {
	return std::string(value < 10 ? " " : "") + std::to_string(value);
}

const TeamHudView * FindTeamById(const HudView& view, Uint16 teamId) {
	for(const TeamHudView& team : view.teams){
		if(team.id == teamId) return &team;
	}
	return nullptr;
}

void AddChatBackgroundSprite(ChatOverlayView& chat,
                             const ::Resources& resources,
                             Uint16 spriteIndex,
                             int logicalX,
                             int logicalY,
                             int targetW = 0,
                             int targetH = 0,
                             bool tile = false) {
	const int spriteW = SpriteWidth(resources, kChatBackgroundBank, spriteIndex);
	const int spriteH = SpriteHeight(resources, kChatBackgroundBank, spriteIndex);
	if(spriteW <= 0 || spriteH <= 0) return;
	if(targetW <= 0) targetW = spriteW;
	if(targetH <= 0) targetH = spriteH;
	if(targetW <= 0 || targetH <= 0) return;

	ChatBackgroundSpriteView sprite;
	sprite.x = SpriteX(resources, kChatBackgroundBank, spriteIndex, logicalX);
	sprite.y = SpriteY(resources, kChatBackgroundBank, spriteIndex, logicalY);
	sprite.w = targetW;
	sprite.h = targetH;
	sprite.bank = kChatBackgroundBank;
	sprite.index = spriteIndex;
	sprite.sourceW = spriteW;
	sprite.sourceH = spriteH;
	sprite.tile = tile;
	chat.backgroundSprites.push_back(sprite);
}

void PopulateChatBackground(ChatOverlayView& chat, const ::Resources& resources) {
	const int topLeftW = SpriteWidth(resources, kChatBackgroundBank, 0);
	const int topMiddleW = std::max(0, kChatW - topLeftW - kChatRightCapX);
	AddChatBackgroundSprite(chat, resources, 0, 0, 0);
	AddChatBackgroundSprite(
		chat, resources, 1, topLeftW, 0, topMiddleW,
		SpriteHeight(resources, kChatBackgroundBank, 1), true);
	AddChatBackgroundSprite(chat, resources, 2, kChatW - kChatRightCapX, 0);

	const int bottomLeftW = SpriteWidth(resources, kChatBackgroundBank, 6);
	const int bottomMiddleW = std::max(0, kChatW - bottomLeftW - kChatRightCapX);
	AddChatBackgroundSprite(
		chat, resources, 6, 0, kChatBackgroundInteriorH);
	AddChatBackgroundSprite(
		chat, resources, 7, bottomLeftW, kChatBackgroundInteriorH,
		bottomMiddleW, SpriteHeight(resources, kChatBackgroundBank, 7), true);
	AddChatBackgroundSprite(
		chat, resources, 8, kChatW - kChatRightCapX, kChatBackgroundInteriorH);
}

void PopulatePlayerFields(PlayerHudView& view, ::Player* player) {
	view.valid = true;
	view.health = player->health;
	view.maxHealth = player->maxhealth;
	view.shield = player->shield;
	view.maxShield = player->maxshield;
	view.fuel = player->fuel;
	view.maxFuel = player->maxfuel;
	view.fuelLow = player->fuellow;
	view.files = player->files;
	view.maxFiles = player->maxfiles;
	view.credits = player->credits;
	view.currentWeapon = player->currentweapon;
	view.laserAmmo = player->laserammo;
	view.rocketAmmo = player->rocketammo;
	view.flamerAmmo = player->flamerammo;
	for(int i = 0; i < 4; ++i){
		view.inventoryItems[i] = player->inventoryitems[i];
		view.inventoryItemsNum[i] = player->inventoryitemsnum[i];
	}
	view.currentInventoryItem = player->currentinventoryitem;
	view.poisonedBy = player->GetPoisonedBy();
	view.tracetime = player->GetTraceTime();
	view.state = player->GetState();
	view.state_i = player->GetStateProgress();
	view.hasSecret = player->hassecret;
	view.chatActive = player->chatActive;
	view.chatWithTeam = player->chatwithteam;
	view.chatText = player->chatText;
	view.chatTextCapacity = static_cast<int>(sizeof(player->chatText));
	view.isBuying = player->isbuying;
	view.techStationActive = player->techstationactive;
	view.buyIfaceLastItem = player->buyifacelastitem;
	view.buyIfaceLastScrolled = player->buyifacelastscrolled;
	view.techIfaceLastItem = player->techifacelastitem;
	view.techIfaceLastScrolled = player->techifacelastscrolled;
	view.teamId = player->GetTeamId();
}

::Player* ResolveViewedPlayer(::World& world) {
	::Peer* peer = world.GetPeer(world.viewedpeerid);
	if(!peer) return nullptr;
	for(std::list<Uint16>::iterator it = peer->controlledlist.begin();
	    it != peer->controlledlist.end(); ++it){
		::Object* object = world.GetObjectFromId(*it);
		if(object && object->type == ObjectTypes::PLAYER){
			return static_cast<::Player*>(object);
		}
	}
	return nullptr;
}

void PopulateTeams(HudView& out, ::World& world) {
	const std::vector<Uint16>& teamIds = world.GetObjectsByType(ObjectTypes::TEAM);
	for(Uint16 tid : teamIds){
		::Team* team = static_cast<::Team*>(world.GetObjectFromId(tid));
		if(!team) continue;
		TeamHudView teamView;
		teamView.id = team->id;
		teamView.agency = team->agency;
		teamView.color = team->GetColor();
		teamView.numPeers = team->numpeers;
		for(int i = 0; i < 4; ++i) teamView.peerIds[i] = team->peers[i];
		teamView.secrets = team->secrets;
		teamView.secretProgress = team->secretprogress;
		teamView.baseDoorId = team->basedoorid;
		teamView.beamingTerminalId = team->beamingterminalid;
		if(team->beamingterminalid){
			::Terminal* terminal = static_cast<::Terminal*>(
				world.GetObjectFromId(team->beamingterminalid));
			if(terminal) teamView.beamingTerminalTraceTime = terminal->tracetime;
		}
		teamView.disabledTech = team->disabledtech;

		// Resolve emblem size (from sprite bank 181)
		Surface* emblem = nullptr;
		if(181 < world.resources.spritebank.size() &&
		   team->agency < world.resources.spritebank[181].size()){
			emblem = world.resources.spritebank[181][team->agency].get();
		}
		teamView.emblemW = emblem ? emblem->w * 2 : 32;
		teamView.emblemH = emblem ? emblem->h * 2 : 32;

		for(int i = 0; i < team->numpeers; ++i){
			::Peer* p = world.GetPeer(team->peers[i]);
			TeamHudView::PeerSlot& slot = teamView.peerSlots[i];
			if(!p) continue;
			::Player* peerplayer = world.GetPeerPlayer(p->id);
			if(!peerplayer) continue;
			slot.present = true;
			slot.state = peerplayer->GetState();
			slot.inBase = peerplayer->InBase(world);
			slot.hasSecret = peerplayer->hassecret;

			TeamPeerView pv;
			pv.peerId = p->id;
			pv.isBot = p->isbot;
			::User* user = world.lobby.GetUserInfo(p->accountid);
			if(user){
				char displayname[120];
				if(p->isbot){
					std::snprintf(displayname, sizeof(displayname), "%s [BOT]", user->DisplayName());
				}else{
					std::snprintf(displayname, sizeof(displayname), "%s", user->DisplayName());
				}
				pv.displayName = displayname;
				pv.agencyLevel = user->agency[team->agency].level;
				pv.agencyEndurance = user->agency[team->agency].endurance;
				pv.agencyShield = user->agency[team->agency].shield;
				pv.agencyJetpack = user->agency[team->agency].jetpack;
				pv.agencyHacking = user->agency[team->agency].hacking;
				pv.agencyContacts = user->agency[team->agency].contacts;
				pv.valid = true;
				teamView.playerListPeers.push_back(pv);
			}
		}
		out.teams.push_back(teamView);
	}
}

void PopulateBuyTech(HudView& out, ::World& world, ::Player* player) {
	if(!player) return;
	if(!(player->isbuying || player->techstationactive)) return;

	std::vector<::BuyableItem*> menuitems;
	player->CollectBuyMenuItems(world, player->techstationactive, menuitems);

	::Team* buyTechTeam =
		static_cast<::Team*>(world.GetObjectFromId(player->GetTeamId()));
	::Team* otherteam = nullptr;
	if(!player->InOwnBase(world)){
		::BaseDoor* basedoor = static_cast<::BaseDoor*>(
			world.GetObjectFromId(player->GetBaseDoorEntering()));
		if(basedoor){
			otherteam = static_cast<::Team*>(world.GetObjectFromId(basedoor->teamid));
		}
	}

	auto itemName = [&](::BuyableItem* item) -> std::string {
		std::string name = item->name;
		int peerIndex = -1;
		if(item->id == ::World::BUY_GIVE0) peerIndex = 0;
		if(item->id == ::World::BUY_GIVE1) peerIndex = 1;
		if(item->id == ::World::BUY_GIVE2) peerIndex = 2;
		if(item->id == ::World::BUY_GIVE3) peerIndex = 3;
		if(peerIndex >= 0 && buyTechTeam){
			::Peer* targetPeer = world.GetPeer(buyTechTeam->peers[peerIndex]);
			if(targetPeer){
				::User* user = world.lobby.GetUserInfo(targetPeer->accountid);
				if(user) name += user->DisplayName();
			}
		}
		return name;
	};

	auto itemPrice = [&](::BuyableItem* item) -> std::string {
		if(player->isbuying){
			if(buyTechTeam && (buyTechTeam->disabledtech & item->techchoice)){
				return "DOWN";
			}
			return std::to_string(item->price);
		}
		if(!player->InOwnBase(world)){
			if(otherteam && (otherteam->disabledtech & item->techchoice)){
				return "DOWN";
			}
			return std::to_string(item->repairprice);
		}
		if(buyTechTeam && (buyTechTeam->disabledtech & item->techchoice)){
			return std::to_string(item->repairprice);
		}
		return "UP";
	};

	int selecteditem = player->isbuying ? player->buyifacelastitem : player->techifacelastitem;
	int scrolled = player->isbuying ? player->buyifacelastscrolled : player->techifacelastscrolled;
	if(selecteditem < 0) selecteditem = 0;
	if(!menuitems.empty() && selecteditem >= (int)menuitems.size()){
		selecteditem = (int)menuitems.size() - 1;
	}
	if(scrolled < 0) scrolled = 0;

	Uint8 uiTick = static_cast<Uint8>(SDL_GetTicks() / 50);
	Uint8 selectedBrightness = 128;
	if(uiTick % 16 >= 8) selectedBrightness += (uiTick % 8);
	else selectedBrightness += 8 - (uiTick % 8);

	BuyTechOverlayView& view = out.buyTech;
	view.visible = true;
	view.isBuying = player->isbuying;
	view.backgroundX = SpriteX(world.resources, 102, 0);
	view.backgroundY = SpriteY(world.resources, 102, 0);
	view.backgroundW = SpriteWidth(world.resources, 102, 0);
	view.backgroundH = SpriteHeight(world.resources, 102, 0);
	view.highlightX = SpriteX(world.resources, 102, 1);
	view.highlightY = SpriteY(world.resources, 102, 1);
	view.highlightW = SpriteWidth(world.resources, 102, 1);
	view.highlightH = SpriteHeight(world.resources, 102, 1);
	for(int i = scrolled; i < (int)menuitems.size() && (int)view.rows.size() < 5; ++i){
		::BuyableItem* item = menuitems[i];
		bool selected = (i == selecteditem);
		const int visibleRow = static_cast<int>(view.rows.size());
		const int yoffset = visibleRow * 25;
		BuyTechRowView row;
		row.index = i;
		row.name = itemName(item);
		row.price = itemPrice(item);
		row.selected = selected;
		row.brightness = selected ? selectedBrightness : (Uint8)128;
		row.spriteBank = item->res_bank;
		row.spriteIndex = item->res_index;
		row.iconX = SpriteX(world.resources, row.spriteBank, row.spriteIndex, 169);
		row.iconY = SpriteY(world.resources, row.spriteBank, row.spriteIndex, 139 + yoffset);
		row.iconW = SpriteWidth(world.resources, row.spriteBank, row.spriteIndex);
		row.iconH = SpriteHeight(world.resources, row.spriteBank, row.spriteIndex);
		view.rows.push_back(row);
	}

	if(player->isbuying || player->InOwnBase(world)){
		view.footer = "Available Credits: " + std::to_string(player->credits);
	}else{
		view.footer = "Viruses Available: " +
		              std::to_string(player->InventoryItemCount(::Player::INV_VIRUS));
	}
}

void PopulateChatOverlay(HudView& out, ::World& world, const PlayerHudView& player) {
	if(!player.valid) return;
	if(!player.chatActive && out.showChatTicks <= 0) return;

	ChatOverlayView& chat = out.chat;
	for(int i = 0; i < static_cast<int>(out.chatLines.size()); ++i){
		if(player.chatActive && i == 0 && out.chatLines.size() == 5){
			continue;
		}
		chat.lines.push_back(out.chatLines[i].substr(0, kChatMaxHistoryChars));
	}
	if(chat.lines.empty() && !player.chatActive) return;

	chat.visible = true;
	chat.inputActive = player.chatActive;
	chat.chatWithTeam = player.chatWithTeam;
	chat.panelX = kChatX;
	chat.panelY = kChatY;
	chat.panelW = kChatW;
	const int contentLines =
		static_cast<int>(chat.lines.size()) + (player.chatActive ? 1 : 0);
	chat.panelH =
		std::max(kChatChromeH,
		         kChatTextStartY + contentLines * kChatLineStepY + 12);
	chat.inputCapacity = player.chatTextCapacity;
	chat.inputPrefix = player.chatWithTeam ? "(TEAM):" : "(ALL):";
	chat.inputText = player.chatText;
	chat.caretVisible = ((world.tickcount / 3) % 32) < 16;
	PopulateChatBackground(chat, world.resources);
}

void PopulateReadouts(HudView& out, const PlayerHudView& player) {
	if(!player.valid) return;

	Uint8 currentAmmo = 0;
	switch(player.currentWeapon){
		case 0:
			currentAmmo = 99;
			break;
		case 1:
			currentAmmo = player.laserAmmo;
			break;
		case 2:
			currentAmmo = player.rocketAmmo;
			break;
		case 3:
			currentAmmo = player.flamerAmmo;
			break;
	}

	HudReadoutsView& readouts = out.readouts;
	readouts.visible = true;
	readouts.currentAmmo = PaddedByte(currentAmmo);
	readouts.blasterAmmo = "99";
	readouts.laserAmmo = player.laserAmmo > 0 ? PaddedByte(player.laserAmmo) : "";
	readouts.rocketAmmo = player.rocketAmmo > 0 ? PaddedByte(player.rocketAmmo) : "";
	readouts.flamerAmmo = player.flamerAmmo > 0 ? PaddedByte(player.flamerAmmo) : "";
	readouts.credits = std::to_string(player.credits);
	readouts.health = std::to_string(player.health);
	readouts.shield = std::to_string(player.shield);
	for(int i = 0; i < 4; ++i){
		if(player.inventoryItemsNum[i] > 1){
			readouts.inventoryCounts[i] = std::to_string(player.inventoryItemsNum[i]);
		}
	}

	const TeamHudView * team = FindTeamById(out, player.teamId);
	if(team && team->beamingTerminalId && team->beamingTerminalTraceTime > 0){
		readouts.traceTime = team->beamingTerminalTraceTime;
	}
	if(player.tracetime > 0) readouts.traceTime = player.tracetime;
}

void AddHudSprite(HudSpriteView * sprites,
                  int& count,
                  int capacity,
                  const ::Resources& resources,
                  int x,
                  int y,
                  Uint8 spriteBank,
                  Uint16 spriteIndex,
                  Uint8 brightness = 128,
                  bool animatedHighlight = false) {
	if(!sprites || count < 0 || count >= capacity) return;
	const int w = SpriteWidth(resources, spriteBank, spriteIndex);
	const int h = SpriteHeight(resources, spriteBank, spriteIndex);
	if(w <= 0 || h <= 0) return;

	HudSpriteView& sprite = sprites[count++];
	sprite.visible = true;
	sprite.x = x;
	sprite.y = y;
	sprite.w = w;
	sprite.h = h;
	sprite.spriteBank = spriteBank;
	sprite.spriteIndex = spriteIndex;
	sprite.brightness = brightness;
	sprite.animatedHighlight = animatedHighlight;
}

void AddStatusSprite(HudStatusView& status,
                     const ::Resources& resources,
                     int x,
                     int y,
                     Uint8 spriteBank,
                     Uint16 spriteIndex,
                     int sourceX = 0,
                     int sourceY = 0,
                     int sourceW = 0,
                     int sourceH = 0,
                     Uint8 brightness = 128,
                     bool animatedShield = false,
                     bool blinkWarning = false) {
	const int w = sourceW > 0 ? sourceW : SpriteWidth(resources, spriteBank, spriteIndex);
	const int h = sourceH > 0 ? sourceH : SpriteHeight(resources, spriteBank, spriteIndex);
	if(w <= 0 || h <= 0) return;

	HudSpriteView sprite;
	sprite.visible = true;
	sprite.x = x;
	sprite.y = y;
	sprite.w = w;
	sprite.h = h;
	sprite.spriteBank = spriteBank;
	sprite.spriteIndex = spriteIndex;
	sprite.sourceX = sourceX;
	sprite.sourceY = sourceY;
	sprite.sourceW = sourceW;
	sprite.sourceH = sourceH;
	sprite.brightness = brightness;
	sprite.animatedShield = animatedShield;
	sprite.blinkWarning = blinkWarning;
	status.sprites.push_back(sprite);
}

Uint8 InventoryResIndex(Uint8 id) {
	switch(id){
		default:
		case ::Player::INV_NONE:
			return 0xFF;
		case ::Player::INV_HEALTHPACK:
			return 1;
		case ::Player::INV_LAZARUSTRACT:
			return 2;
		case ::Player::INV_SECURITYPASS:
			return 3;
		case ::Player::INV_VIRUS:
			return 14;
		case ::Player::INV_POISON:
			return 5;
		case ::Player::INV_EMPBOMB:
			return 6;
		case ::Player::INV_SHAPEDBOMB:
			return 7;
		case ::Player::INV_PLASMABOMB:
			return 8;
		case ::Player::INV_NEUTRONBOMB:
			return 9;
		case ::Player::INV_PLASMADET:
			return 12;
		case ::Player::INV_FIXEDCANNON:
			return 11;
		case ::Player::INV_FLARE:
			return 10;
		case ::Player::INV_POISONFLARE:
			return 13;
		case ::Player::INV_CAMERA:
			return 4;
		case ::Player::INV_BASEDOOR:
			return 0;
	}
}

std::string InventoryLetter(Uint8 id) {
	switch(id){
		case ::Player::INV_PLASMABOMB:
			return "P";
		case ::Player::INV_SHAPEDBOMB:
			return "S";
		case ::Player::INV_NEUTRONBOMB:
			return "N";
		case ::Player::INV_EMPBOMB:
			return "E";
		case ::Player::INV_PLASMADET:
			return "D";
		case ::Player::INV_FLARE:
			return "F";
		default:
			return "";
	}
}

void PopulateStatus(HudView& out,
                    const ::Resources& resources,
                    const PlayerHudView& player) {
	if(!player.valid) return;

	HudStatusView& status = out.status;
	status.visible = true;
	AddStatusSprite(status, resources, SpriteX(resources, 94, 0), SpriteY(resources, 94, 0), 94, 0);
	if(player.fuelLow){
		AddStatusSprite(status, resources, SpriteX(resources, 95, 8), SpriteY(resources, 95, 8), 95, 8);
	}

	const int fuelW = player.maxFuel > 0
		? static_cast<int>((static_cast<float>(player.fuel) / player.maxFuel) * SpriteWidth(resources, 95, 6))
		: 0;
	if(fuelW > 0){
		AddStatusSprite(status, resources, SpriteX(resources, 95, 6), SpriteY(resources, 95, 6), 95, 6,
		                0, 0, fuelW, SpriteHeight(resources, 95, 6));
	}
	AddStatusSprite(status, resources, SpriteX(resources, 95, 5), SpriteY(resources, 95, 5), 95, 5);

	const int healthH = SpriteHeight(resources, 95, 0);
	const int healthY = player.maxHealth > 0
		? healthH - static_cast<int>((static_cast<float>(player.health) / player.maxHealth) * healthH)
		: healthH;
	if(healthH - healthY > 0){
		AddStatusSprite(status, resources, SpriteX(resources, 95, 0), SpriteY(resources, 95, 0) + healthY, 95, 0,
		                0, healthY, SpriteWidth(resources, 95, 0), healthH - healthY);
	}

	const int shieldH = SpriteHeight(resources, 95, 1);
	int shieldY = player.maxShield > 0
		? shieldH - static_cast<int>((static_cast<float>(player.shield) / player.maxShield) * shieldH)
		: shieldH;
	if(shieldY < 0) shieldY = 0;
	if(shieldH - shieldY > 0){
		AddStatusSprite(status, resources, SpriteX(resources, 95, 1), SpriteY(resources, 95, 1) + shieldY, 95, 1,
		                0, shieldY, SpriteWidth(resources, 95, 1), shieldH - shieldY,
		                128, player.shield > player.maxShield);
	}

	if(player.poisonedBy){
		AddStatusSprite(status, resources, 183, 453, 97, 5);
	}
	const int filesW = player.maxFiles > 0
		? static_cast<int>((static_cast<float>(player.files) / player.maxFiles) * SpriteWidth(resources, 95, 7))
		: 0;
	if(filesW > 0){
		AddStatusSprite(status, resources, SpriteX(resources, 95, 7), SpriteY(resources, 95, 7), 95, 7,
		                0, 0, filesW, SpriteHeight(resources, 95, 7));
	}

	Uint16 weaponFace = 1;
	Uint16 weaponGlow = 5;
	switch(player.currentWeapon){
		case 1: weaponFace = 2; weaponGlow = 6; break;
		case 2: weaponFace = 3; weaponGlow = 7; break;
		case 3: weaponFace = 4; weaponGlow = 8; break;
		default: break;
	}
	AddStatusSprite(status, resources, SpriteX(resources, 96, weaponFace), SpriteY(resources, 96, weaponFace), 96, weaponFace);
	AddStatusSprite(status, resources, SpriteX(resources, 96, weaponGlow), SpriteY(resources, 96, weaponGlow), 96, weaponGlow);
	AddStatusSprite(status, resources, SpriteX(resources, 96, 0), SpriteY(resources, 96, 0) + (player.currentWeapon * 14), 96, 0);
	if(player.health && player.maxHealth > 0 &&
	   static_cast<float>(player.health) / player.maxHealth <= 0.5f){
		AddStatusSprite(status, resources, SpriteX(resources, 95, 3), SpriteY(resources, 95, 3), 95, 3,
		                0, 0, 0, 0, 128, false, true);
	}
	if(player.shield && player.maxShield > 0 &&
	   static_cast<float>(player.shield) / player.maxShield <= 0.5f){
		AddStatusSprite(status, resources, SpriteX(resources, 95, 4), SpriteY(resources, 95, 4), 95, 4,
		                0, 0, 0, 0, 128, false, true);
	}
	AddStatusSprite(status, resources, SpriteX(resources, 94, 2), SpriteY(resources, 94, 2), 94, 2);

	const int xoffsets[] = {612, 584, 556, 528};
	const int yoffsets[] = {13, 13, 11, 7};
	for(int i = 0; i < 4; ++i){
		const Uint8 inventoryIndex = InventoryResIndex(player.inventoryItems[i]);
		const Uint8 brightness = player.currentInventoryItem == i ? 128 : 32;
		HudInventorySlotView& slot = status.inventory[i];
		slot.letter = InventoryLetter(player.inventoryItems[i]);
		slot.letterBrightness = brightness;
		slot.letterX = xoffsets[i] - 2;
		slot.letterY = yoffsets[i];
		const int w = SpriteWidth(resources, 97, inventoryIndex);
		const int h = SpriteHeight(resources, 97, inventoryIndex);
		if(w > 0 && h > 0){
			slot.icon.visible = true;
			slot.icon.x = SpriteX(resources, 97, inventoryIndex, xoffsets[i]);
			slot.icon.y = SpriteY(resources, 97, inventoryIndex, yoffsets[i]);
			slot.icon.w = w;
			slot.icon.h = h;
			slot.icon.spriteBank = 97;
			slot.icon.spriteIndex = inventoryIndex;
			slot.icon.brightness = brightness;
		}
	}
}

void PopulateSecretOverlay(HudView& out,
                           const ::Resources& resources,
                           const PlayerHudView& player) {
	if(!player.valid) return;
	const TeamHudView * team = FindTeamById(out, player.teamId);
	if(!team || !team->baseDoorId) return;

	int yoffset = 60;
	const int teamCount = static_cast<int>(out.teams.size());
	if(teamCount >= 3) yoffset += (teamCount * 20) - 65;

	HudSecretOverlayView& overlay = out.secretOverlay;
	overlay.visible = true;
	overlay.yOffset = yoffset;
	overlay.showProgress = !team->beamingTerminalId;
	overlay.secretProgress = team->secretProgress;
	overlay.hackingTickEligible =
		player.state == kPlayerStateHacking && player.state_i == 16;

	const Uint16 backgroundIndex = team->beamingTerminalId ? 1 : 0;
	AddHudSprite(overlay.sprites,
	             overlay.spriteCount,
	             3,
	             resources,
	             SpriteX(resources, 187, backgroundIndex),
	             SpriteY(resources, 187, backgroundIndex, yoffset),
	             187,
	             backgroundIndex);
	if(out.highlightSecrets){
		AddHudSprite(overlay.sprites,
		             overlay.spriteCount,
		             3,
		             resources,
		             SpriteX(resources, 86, 2),
		             SpriteY(resources, 86, 2, yoffset),
		             86,
		             2,
		             128,
		             true);
	}
	if(out.highlightMinimap){
		AddHudSprite(overlay.sprites,
		             overlay.spriteCount,
		             3,
		             resources,
		             SpriteX(resources, 86, 1),
		             SpriteY(resources, 86, 1),
		             86,
		             1,
		             128,
		             true);
	}
}

void PopulateSystemCameraFrame(HudView& out,
                               const ::Resources& resources,
                               int slot,
                               Uint8 spriteBank,
                               Uint16 spriteIndex,
                               Uint8 offsetBank,
                               int logicalY) {
	if(slot < 0 || slot >= 2) return;
	if(!out.systemCamera[slot].active) return;

	const int w = SpriteWidth(resources, spriteBank, spriteIndex);
	const int h = SpriteHeight(resources, spriteBank, spriteIndex);
	if(w <= 0 || h <= 0) return;

	SystemCameraFrameView& frame = out.systemCameraFrames[slot];
	frame.visible = true;
	frame.x = -SpriteOffsetX(resources, spriteBank, spriteIndex);
	frame.y = -SpriteOffsetY(resources, offsetBank, spriteIndex) + logicalY;
	frame.w = w;
	frame.h = h;
	frame.spriteBank = spriteBank;
	frame.spriteIndex = spriteIndex;
}

}  // namespace hudview_detail

HudView BuildHudView(::World& world) {
	HudView view;

	view.mapLoaded = world.map.loaded;
	if(!view.mapLoaded){
		return view;
	}

	view.tickCount = world.tickcount;
	view.highlightSecrets = world.ShouldHighlightSecrets();
	view.highlightMinimap = world.ShouldHighlightMinimap();
	view.showPlayerList = world.IsShowingPlayerList();
	view.quitState = world.quitstate;
	view.showChatTicks = world.messaging.showchat_i;

	// System camera state
	for(int slot = 0; slot < 2; ++slot){
		view.systemCamera[slot].active = world.IsSystemCameraActive(slot);
		view.systemCamera[slot].followObjectId = world.GetSystemCameraFollowId(slot);
		view.systemCamera[slot].offsetX = world.GetSystemCameraX(slot);
		view.systemCamera[slot].offsetY = world.GetSystemCameraY(slot);
	}
	hudview_detail::PopulateSystemCameraFrame(view, world.resources, 0, 95, 2, 92, 381);
	hudview_detail::PopulateSystemCameraFrame(view, world.resources, 1, 95, 11, 92, 318);

	// Messages
	view.message.message_i = world.GetMessageProgress();
	view.message.messagetype = world.GetMessageType();
	view.message.messagetime = world.GetMessageTime();
	if(view.message.message_i){
		view.message.message = world.GetMessageText();
	}
	view.topMessage.topmessage_i = world.GetTopMessageProgress();
	if(view.topMessage.topmessage_i){
		view.topMessage.text = world.GetTopMessageText();
	}

	// Status messages: each entry encodes text, then time, then color in adjacent bytes.
	for(std::deque<char*>::const_iterator it = world.messaging.statusmessages.begin();
	    it != world.messaging.statusmessages.end(); ++it){
		char* raw = *it;
		InGameStatusLineView line;
		line.text = raw;
		line.time = static_cast<Uint8>(raw[std::strlen(raw) + 1]);
		line.color = static_cast<Uint8>(raw[std::strlen(raw) + 2]);
		view.statusMessages.push_back(line);
	}

	// Chat history (overlay reads the stored deque verbatim).
	for(const std::string& line : world.messaging.chatlines){
		view.chatLines.push_back(line);
	}

	// Players
	::Player* localplayer = world.GetPeerPlayer(world.GetLocalPeerId());
	if(localplayer) hudview_detail::PopulatePlayerFields(view.localPlayer, localplayer);

	::Player* viewedplayer = hudview_detail::ResolveViewedPlayer(world);
	if(viewedplayer){
		hudview_detail::PopulatePlayerFields(view.viewedPlayer, viewedplayer);
		view.viewedPlayer.inOwnBase = viewedplayer->InOwnBase(world);
		view.viewedPlayer.virusInventoryCount =
			viewedplayer->InventoryItemCount(::Player::INV_VIRUS);
	}

	// Teams strip + player-list rows
	hudview_detail::PopulateTeams(view, world);
	hudview_detail::PopulateStatus(view, world.resources, view.viewedPlayer);
	hudview_detail::PopulateReadouts(view, view.viewedPlayer);
	hudview_detail::PopulateSecretOverlay(view, world.resources, view.viewedPlayer);

	// Buy/Tech overlay derived from viewed player.
	hudview_detail::PopulateBuyTech(view, world, viewedplayer);
	hudview_detail::PopulateChatOverlay(view, world, view.viewedPlayer);

	return view;
}

}  // namespace client_ui
}  // namespace silencer
