#include "client/ui/hooks/use_match.h"

#include "audio.h"
#include "basedoor.h"
#include "buyableitem.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "screen_context.h"
#include "team.h"
#include "world.h"

#include <cstring>

namespace silencer {
namespace client_ui {

MatchProviderValue MakeMatchProvider(ScreenContext& ctx) {
	return MakeMatchProvider(ctx.world, ctx.world.peers.localpeerid);
}

MatchProviderValue MakeMatchProvider(World& world, int local_peer_id) {
	MatchProviderValue value;
	value.world = &world;
	value.local_peer_id = local_peer_id;
	return value;
}

namespace match_provider_detail {

Player * PlayerForPeer(World * world, int peer_id) {
	return world ? world->GetPeerPlayer(peer_id) : nullptr;
}

Player * PlayerForPeer(const MatchProviderValue& provider, int peer_id) {
	return PlayerForPeer(provider.world, peer_id);
}

void CopyPlayerChatText(Player& player, const std::string& text) {
	int n = static_cast<int>(text.size());
	if(n > static_cast<int>(sizeof(player.chatText)) - 1){
		n = static_cast<int>(sizeof(player.chatText)) - 1;
	}
	std::memcpy(player.chatText, text.data(), n);
	player.chatText[n] = '\0';
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

}  // namespace match_provider_detail

MatchChatModel::MatchChatModel(
		const MatchProviderValue& provider,
		int local_peer_id)
	: provider_(provider), local_peer_id_(local_peer_id) {}

bool MatchChatModel::active() const {
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	return player && player->chatActive;
}

void MatchChatModel::set_draft(const std::string& text) const {
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	if(!player) return;
	match_provider_detail::CopyPlayerChatText(*player, text);
}

void MatchChatModel::submit(const std::string& text) const {
	World * world = provider_.world;
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	if(!world || !player) return;
	set_draft(text);
	if(std::strlen(player->chatText) > 0){
		world->SendChat(player->chatwithteam, player->chatText);
	}
	player->chatText[0] = '\0';
	player->chatActive = false;
}

void MatchChatModel::cancel() const {
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	if(!player) return;
	player->chatText[0] = '\0';
	player->chatActive = false;
}

void MatchChatModel::toggle_channel() const {
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	if(!player) return;
	player->chatwithteam = !player->chatwithteam;
}

MatchStationModel::MatchStationModel(
		const MatchProviderValue& provider,
		int local_peer_id)
	: provider_(provider), local_peer_id_(local_peer_id) {}

bool MatchStationModel::active() const {
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	return player && (player->isbuying || player->techstationactive);
}

void MatchStationModel::normalize_selection() const {
	World * world = provider_.world;
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	if(!world || !player || !active()) return;
	match_provider_detail::ClampBuyTechSelection(*player, *world);
}

void MatchStationModel::select_row(int index) const {
	World * world = provider_.world;
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	if(!world || !player || !active()) return;
	match_provider_detail::SelectBuyTechRow(*player, *world, index);
}

void MatchStationModel::activate_selected() const {
	World * world = provider_.world;
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	if(!world || !player || !active()) return;
	match_provider_detail::ActivateBuyTechSelection(*player, *world);
}

void MatchStationModel::close() const {
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	if(!player) return;
	player->isbuying = false;
	player->techstationactive = false;
}

MatchHudModel::MatchHudModel(
		const MatchProviderValue& provider,
		int local_peer_id)
	: provider_(provider), local_peer_id_(local_peer_id) {}

bool MatchHudModel::has_input_target() const {
	Player * player = match_provider_detail::PlayerForPeer(provider_, local_peer_id_);
	return player && (player->chatActive || player->isbuying || player->techstationactive);
}

void MatchHudModel::update_overlay_state() const {
	MatchModel match = use_match(provider_);
	match.station.normalize_selection();
}

HudView MatchHudModel::snapshot() const {
	World * world = provider_.world;
	if(!world) return HudView{};
	return BuildHudView(*world);
}

MatchControlSurfaceModel::MatchControlSurfaceModel(
		const MatchProviderValue& provider)
	: provider_(provider) {}

MatchUiControlResult MatchControlSurfaceModel::configure(MatchUiControlMode mode) const {
	MatchUiControlResult result;
	result.mode = mode;

	World * world = provider_.world;
	if(!world){
		result.error = "no match world";
		return result;
	}
	Player * player = world->GetPeerPlayer(world->viewedpeerid);
	if(!player){
		result.error = "no viewed player";
		return result;
	}

	auto populate = [&]() {
		std::vector<BuyableItem *> buyItems;
		std::vector<BuyableItem *> techItems;
		player->CollectBuyMenuItems(*world, false, buyItems);
		player->CollectBuyMenuItems(*world, true, techItems);
		result.available = true;
		result.chatActive = player->chatActive;
		result.chatDraft = player->chatText;
		result.buyActive = player->isbuying;
		result.techActive = player->techstationactive;
		result.showChatTicks = world->messaging.showchat_i;
		result.showPlayerList = world->IsShowingPlayerList();
		result.quitState = world->quitstate;
		result.topMessageProgress = world->messaging.topmessage_i;
		result.messageProgress = world->messaging.message_i;
		result.statusMessageCount =
			static_cast<int>(world->messaging.statusmessages.size());
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
		world->messaging.showchat_i = 0;
		world->SetShowingPlayerList(false);
		world->quitstate = 0;
		world->messaging.topmessage_i = 0;
		world->messaging.topmessage[0] = '\0';
		world->messaging.message_i = 0;
		world->messaging.message[0] = '\0';
		for(char * status : world->messaging.statusmessages){
			delete[] status;
		}
		world->messaging.statusmessages.clear();
	};

	if(mode == MatchUiControlMode::Clear){
		clear();
		populate();
		return result;
	}

	if(mode != MatchUiControlMode::Status) clear();
	if(mode == MatchUiControlMode::Chat || mode == MatchUiControlMode::All){
		player->chatActive = true;
		player->chatwithteam = false;
		std::strncpy(player->chatText, "clay chat smoke", sizeof(player->chatText) - 1);
		player->chatText[sizeof(player->chatText) - 1] = '\0';
		world->messaging.showchat_i = GASLoader::Get().gameengine.chatDisplayTicks;
	}
	if(mode == MatchUiControlMode::Buy || mode == MatchUiControlMode::All){
		player->isbuying = true;
		player->buyifacelastitem = 0;
		player->buyifacelastscrolled = 0;
	}
	if(mode == MatchUiControlMode::Tech || mode == MatchUiControlMode::All){
		Team * team = player->GetTeam(*world);
		const std::vector<Uint16> & teams = world->GetObjectsByType(ObjectTypes::TEAM);
		if(!team && !teams.empty()){
			team = static_cast<Team *>(world->GetObjectFromId(teams[0]));
		}
		if(!team){
			team = static_cast<Team *>(world->CreateObject(ObjectTypes::TEAM));
			if(team){
				team->agency = Team::NOXIS;
				team->number = 0;
				team->color = ((8 << 4) + 13);
			}
		}
		if(team){
			player->SetTeamId(team->id);
			team->AddPeer(world->GetLocalPeerId());
		}
		BaseDoor * door = nullptr;
		if(team){
			team->disabledtech = 0xffffffff;
			door = static_cast<BaseDoor *>(world->GetObjectFromId(team->basedoorid));
			for(Uint16 objectid : world->GetObjectsByType(ObjectTypes::BASEDOOR)){
				auto * candidate = static_cast<BaseDoor *>(world->GetObjectFromId(objectid));
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
			int baseY = ((world->map.height + 10) * 64) + (team->number * 26 * 64) + 64;
			player->y = static_cast<Sint16>(baseY);
		}
		player->techstationactive = true;
		player->techifacelastitem = 0;
		player->techifacelastscrolled = 0;
	}
	if(mode == MatchUiControlMode::PlayerList || mode == MatchUiControlMode::All){
		world->SetShowingPlayerList(true);
	}
	if(mode == MatchUiControlMode::QuitPrompt || mode == MatchUiControlMode::All){
		world->quitstate = 1;
	}
	if(mode == MatchUiControlMode::TopMessage || mode == MatchUiControlMode::All){
		world->ShowTopMessage("        RETAINED TOP MESSAGE");
	}
	if(mode == MatchUiControlMode::Message || mode == MatchUiControlMode::All){
		world->ShowMessage("RETAINED CENTER MESSAGE", 128, 0);
	}
	if(mode == MatchUiControlMode::StatusLine || mode == MatchUiControlMode::All){
		world->ShowStatus("RETAINED STATUS MESSAGE", 153);
	}

	populate();
	return result;
}

MatchModel::MatchModel(const MatchProviderValue& provider)
	: hud(provider, provider.local_peer_id),
	  chat(provider, provider.local_peer_id),
	  station(provider, provider.local_peer_id),
	  control(provider) {}

MatchModel use_match(const MatchProviderValue& provider) {
	return MatchModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
