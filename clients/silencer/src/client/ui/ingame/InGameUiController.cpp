#include "client/ui/ingame/InGameUiController.h"

#include "audio.h"
#include "basedoor.h"
#include "buyableitem.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "team.h"
#include "runtime/UiInteractionRegistry.h"
#include "world.h"

#include <cstring>

namespace silencer {
namespace client_ui {

namespace ingameuicontroller_detail {

bool StartsWith(const std::string& value, const char * prefix) {
	return value.compare(0, std::strlen(prefix), prefix) == 0;
}

void ClampBuyTechSelection(Player& player, World& world) {
	std::vector<BuyableItem *> items;
	bool tech = player.techstationactive;
	player.CollectBuyMenuItems(world, tech, items);
	int & selected = player.isbuying ? player.buyifacelastitem : player.techifacelastitem;
	int & scrolled = player.isbuying ? player.buyifacelastscrolled : player.techifacelastscrolled;
	if(items.empty()){
		selected = 0;
		scrolled = 0;
		return;
	}
	if(selected < 0) selected = 0;
	if(selected >= static_cast<int>(items.size())) selected = static_cast<int>(items.size()) - 1;
	if(selected >= scrolled + 5) scrolled = selected - 4;
	if(selected < scrolled) scrolled = selected;
	if(scrolled < 0) scrolled = 0;
}

void SelectBuyTechRow(Player& player, World& world, int index) {
	std::vector<BuyableItem *> items;
	bool tech = player.techstationactive;
	player.CollectBuyMenuItems(world, tech, items);
	if(items.empty()) return;
	if(index < 0) index = 0;
	if(index >= static_cast<int>(items.size())) index = static_cast<int>(items.size()) - 1;
	int & selected = player.isbuying ? player.buyifacelastitem : player.techifacelastitem;
	if(selected != index){
		Audio::GetInstance().Play(
			world.resources.soundbank[GASLoader::Get().player.soundRoundCountdown],
			64);
	}
	selected = index;
	ClampBuyTechSelection(player, world);
}

void ActivateBuyTechSelection(Player& player, World& world) {
	std::vector<BuyableItem *> items;
	bool tech = player.techstationactive;
	player.CollectBuyMenuItems(world, tech, items);
	if(items.empty()) return;
	int & selected = player.isbuying ? player.buyifacelastitem : player.techifacelastitem;
	if(selected < 0) selected = 0;
	if(selected >= static_cast<int>(items.size())) selected = static_cast<int>(items.size()) - 1;
	BuyableItem * buyableitem = items[selected];
	if(player.isbuying){
		player.BuyItem(world, buyableitem->id);
	}else if(player.InOwnBase(world)){
		player.RepairItem(world, buyableitem->id);
	}else{
		player.VirusItem(world, buyableitem->id);
	}
}

}  // namespace ingameuicontroller_detail

InGameUiController::InGameUiController(World& world) : world_(world) {}

bool InGameUiController::HasInputTarget(int localPeerId) {
	Player * player = world_.GetPeerPlayer(localPeerId);
	return player && (player->chatActive || player->isbuying || player->techstationactive);
}

void InGameUiController::UpdateOverlayState(int localPeerId) {
	Player * localplayer = world_.GetPeerPlayer(localPeerId);
	if(!localplayer) return;
	if(localplayer->isbuying || localplayer->techstationactive){
		std::vector<BuyableItem *> items;
		localplayer->CollectBuyMenuItems(world_, localplayer->techstationactive, items);
		int & selected = localplayer->isbuying
			? localplayer->buyifacelastitem
			: localplayer->techifacelastitem;
		int & scrolled = localplayer->isbuying
			? localplayer->buyifacelastscrolled
			: localplayer->techifacelastscrolled;
		if(items.empty()){
			selected = 0;
			scrolled = 0;
		}else{
			if(selected < 0) selected = 0;
			if(selected >= static_cast<int>(items.size())) selected = static_cast<int>(items.size()) - 1;
			if(selected >= scrolled + 5) scrolled = selected - 4;
			if(selected < scrolled) scrolled = selected;
			if(scrolled < 0) scrolled = 0;
		}
	}
}

bool InGameUiController::ApplyActions(
	int localPeerId,
	const std::vector<silencer::ui::UiAction>& actions,
	silencer::ui::UiInteractionRegistry& interactions) {
	Player * localplayer = world_.GetPeerPlayer(localPeerId);
	if(!localplayer) return false;

	bool handled = false;
	for(const silencer::ui::UiAction& action : actions){
		if(localplayer->chatActive &&
		   (action.id == "ingame.chat" || action.id == "ingame.chat.channel")){
			handled = true;
			if(action.kind == silencer::ui::UiActionKind::SetText &&
			   action.id == "ingame.chat"){
				int n = static_cast<int>(action.value.size());
				if(n > static_cast<int>(sizeof(localplayer->chatText)) - 1){
					n = static_cast<int>(sizeof(localplayer->chatText)) - 1;
				}
				std::memcpy(localplayer->chatText, action.value.data(), n);
				localplayer->chatText[n] = '\0';
			}else if(action.kind == silencer::ui::UiActionKind::SubmitText){
				int n = static_cast<int>(action.value.size());
				if(n > static_cast<int>(sizeof(localplayer->chatText)) - 1){
					n = static_cast<int>(sizeof(localplayer->chatText)) - 1;
				}
				std::memcpy(localplayer->chatText, action.value.data(), n);
				localplayer->chatText[n] = '\0';
				if(std::strlen(localplayer->chatText) > 0){
					world_.SendChat(localplayer->chatwithteam, localplayer->chatText);
				}
				localplayer->chatText[0] = '\0';
				localplayer->chatActive = false;
			}else if(action.kind == silencer::ui::UiActionKind::Cancel){
				localplayer->chatText[0] = '\0';
				localplayer->chatActive = false;
			}else if(action.kind == silencer::ui::UiActionKind::Navigate ||
			         action.kind == silencer::ui::UiActionKind::Activate){
				if(action.id == "ingame.chat.channel"){
					localplayer->chatwithteam = !localplayer->chatwithteam;
					interactions.FocusInteractableById("ingame.chat");
				}
			}
			continue;
		}

		if((localplayer->isbuying || localplayer->techstationactive) &&
		   ingameuicontroller_detail::StartsWith(action.id, "ingame.buytech.row.")){
			handled = true;
			if(action.index >= 0){
				ingameuicontroller_detail::SelectBuyTechRow(*localplayer, world_, action.index);
			}
			if(action.kind == silencer::ui::UiActionKind::Select &&
			   action.value != "focus_next" && action.value != "focus_previous"){
				ingameuicontroller_detail::ActivateBuyTechSelection(*localplayer, world_);
			}
			continue;
		}

		if((localplayer->isbuying || localplayer->techstationactive) &&
		   action.kind == silencer::ui::UiActionKind::Cancel){
			localplayer->isbuying = false;
			localplayer->techstationactive = false;
			handled = true;
		}
	}
	return handled;
}

InGameUiControlResult InGameUiController::ConfigureForControl(InGameUiControlMode mode) {
	InGameUiControlResult result;
	result.mode = mode;

	Player * player = world_.GetPeerPlayer(world_.viewedpeerid);  // viewedpeerid is public
	if(!player){
		result.error = "no viewed player";
		return result;
	}

	auto populate = [&]() {
		std::vector<BuyableItem *> buyItems;
		std::vector<BuyableItem *> techItems;
		player->CollectBuyMenuItems(world_, false, buyItems);
		player->CollectBuyMenuItems(world_, true, techItems);
		result.available = true;
		result.chatActive = player->chatActive;
		result.buyActive = player->isbuying;
		result.techActive = player->techstationactive;
		result.showChatTicks = world_.messaging.showchat_i;
		result.showPlayerList = world_.IsShowingPlayerList();
		result.buyItemCount = static_cast<int>(buyItems.size());
		result.techItemCount = static_cast<int>(techItems.size());
		result.buySelectedIndex = player->buyifacelastitem;
		result.techSelectedIndex = player->techifacelastitem;
	};

	auto clear = [&]() {
		player->chatActive = false;
		player->chatText[0] = '\0';
		player->isbuying = false;
		player->techstationactive = false;
		world_.messaging.chatlines.clear();
		world_.messaging.showchat_i = 0;
		world_.SetShowingPlayerList(false);
	};

	if(mode == InGameUiControlMode::Clear){
		clear();
		populate();
		return result;
	}

	if(mode != InGameUiControlMode::Status) clear();
	if(mode == InGameUiControlMode::Chat || mode == InGameUiControlMode::All){
		player->chatActive = true;
		player->chatwithteam = false;
		std::strncpy(player->chatText, "clay chat smoke", sizeof(player->chatText) - 1);
		player->chatText[sizeof(player->chatText) - 1] = '\0';
		world_.messaging.chatlines.push_back("- test");
		world_.messaging.showchat_i = GASLoader::Get().gameengine.chatDisplayTicks;
	}
	if(mode == InGameUiControlMode::Buy || mode == InGameUiControlMode::All){
		player->isbuying = true;
		player->buyifacelastitem = 0;
		player->buyifacelastscrolled = 0;
	}
	if(mode == InGameUiControlMode::Tech || mode == InGameUiControlMode::All){
		Team * team = player->GetTeam(world_);
		const std::vector<Uint16> & teams = world_.GetObjectsByType(ObjectTypes::TEAM);
		if(!team && !teams.empty()){
			team = static_cast<Team *>(world_.GetObjectFromId(teams[0]));
		}
		if(!team){
			team = static_cast<Team *>(world_.CreateObject(ObjectTypes::TEAM));
			if(team){
				team->agency = Team::NOXIS;
				team->number = 0;
				team->color = ((8 << 4) + 13);
			}
		}
		if(team){
			player->SetTeamId(team->id);
			team->AddPeer(world_.GetLocalPeerId());
		}
		BaseDoor * door = nullptr;
		if(team){
			team->disabledtech = 0xffffffff;
			door = static_cast<BaseDoor *>(world_.GetObjectFromId(team->basedoorid));
			for(Uint16 objectid : world_.GetObjectsByType(ObjectTypes::BASEDOOR)){
				auto * candidate = static_cast<BaseDoor *>(world_.GetObjectFromId(objectid));
				if(candidate && candidate->teamid == team->id){
					door = candidate;
					break;
				}
			}
		}
		if(door){
			player->SetBaseDoorEntering(door->id);
		}
		if(team){
			int baseY = ((world_.map.height + 10) * 64) + (team->number * 26 * 64) + 64;
			player->y = static_cast<Sint16>(baseY);
		}
		player->techstationactive = true;
		player->techifacelastitem = 0;
		player->techifacelastscrolled = 0;
	}
	if(mode == InGameUiControlMode::PlayerList || mode == InGameUiControlMode::All){
		world_.SetShowingPlayerList(true);
	}

	populate();
	return result;
}

}  // namespace client_ui
}  // namespace silencer
