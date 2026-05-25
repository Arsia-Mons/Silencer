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

#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

namespace hudview_detail {

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
	for(int i = scrolled; i < (int)menuitems.size() && (int)view.rows.size() < 5; ++i){
		::BuyableItem* item = menuitems[i];
		bool selected = (i == selecteditem);
		BuyTechRowView row;
		row.index = i;
		row.name = itemName(item);
		row.price = itemPrice(item);
		row.selected = selected;
		row.brightness = selected ? selectedBrightness : (Uint8)128;
		row.spriteBank = item->res_bank;
		row.spriteIndex = item->res_index;
		view.rows.push_back(row);
	}

	if(player->isbuying || player->InOwnBase(world)){
		view.footer = "Available Credits: " + std::to_string(player->credits);
	}else{
		view.footer = "Viruses Available: " +
		              std::to_string(player->InventoryItemCount(::Player::INV_VIRUS));
	}
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

	// Buy/Tech overlay derived from viewed player.
	hudview_detail::PopulateBuyTech(view, world, viewedplayer);

	return view;
}

}  // namespace client_ui
}  // namespace silencer
