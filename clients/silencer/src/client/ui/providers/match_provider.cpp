#include "client/ui/hooks/use_match.h"

#include "audio.h"
#include "buyableitem.h"
#include "gasloader.h"
#include "player.h"
#include "screen_context.h"
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

MatchModel::MatchModel(const MatchProviderValue& provider)
	: provider_(provider),
	  hud(provider, provider.local_peer_id),
	  chat(provider, provider.local_peer_id),
	  station(provider, provider.local_peer_id) {}

bool MatchModel::active() const {
	return provider_.world && provider_.world->map.loaded;
}

MatchModel use_match(const MatchProviderValue& provider) {
	return MatchModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
