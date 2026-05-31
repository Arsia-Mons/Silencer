#include "game_tech_panel.h"

#include "buyableitem.h"
#include "config.h"
#include "lobby.h"
#include "lobby_screen.h"
#include "peer.h"
#include "screen_context.h"
#include "team.h"
#include "user.h"
#include "world.h"

#include <cstdlib>
#include <cstring>

namespace silencer::client_ui::lobby {

namespace game_tech_panel_detail {

constexpr const char * kActionBack = "lobby.game_tech.back";
constexpr const char * kActionTogglePrefix = "lobby.game_tech.toggle.";
constexpr const char * kActionDescriptionPrefix = "lobby.game_tech.description.";

bool StartsWith(const std::string & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

int SuffixInt(const std::string & value, const char * prefix) {
	if(!StartsWith(value, prefix)) return -1;
	return std::atoi(value.c_str() + std::strlen(prefix));
}

}  // namespace game_tech_panel_detail

void GameTechPanelInit(GameTechPanelState & state) {
	state = GameTechPanelState{};
}

void GameTechPanelTick(GameTechPanelState & state,
                       World & world,
                       ScreenContext & ctx,
                       LobbyScreen & owner) {
	const Uint8 localid = owner.TechPanelLocalPeerId(world);
	Peer * localpeer = owner.TechPanelPeer(world, localid);
	Team * team = world.GetPeerTeam(localid);

	int techslotsleft = 0;
	if(localpeer && team){
		User * user = world.lobby.GetUserInfo(localpeer->accountid);
		if(user){
			techslotsleft =
				user->agency[team->agency].techslots - world.TechSlotsUsed(*localpeer);
			state.slotsLeftStr = "Tech slots left: " + std::to_string(techslotsleft);
		}else{
			state.slotsLeftStr.clear();
		}
	}else{
		state.slotsLeftStr.clear();
		if(!localpeer && world.tickcount % 12 == 0){
			owner.TechPanelRequestPeerList(world);
		}
	}

	for(int i = 0; i < 3; i++) state.peerNameStrs[i].clear();
	if(team){
		int peerindex = 0;
		for(int i = 0; i < 4 && peerindex < 3; i++){
			if(team->peers[i] == localid) continue;
			if(i >= team->numpeers){
				peerindex++;
				continue;
			}
			Peer * peer = owner.TechPanelPeer(world, team->peers[i]);
			User * user = peer ? world.lobby.GetUserInfo(peer->accountid) : nullptr;
			state.peerNameStrs[peerindex] = user ? std::string(user->DisplayName()) : std::string();
			peerindex++;
		}
	}

	state.techRows.resize(world.buyableitems.size());
	for(size_t i = 0; i < world.buyableitems.size(); ++i){
		GameTechRow & row = state.techRows[i];
		BuyableItem * item = world.buyableitems[i];
		row.visible = item != nullptr;
		row.selected = item && localpeer && ((localpeer->techchoices & item->techchoice) != 0);
		row.label.clear();
		if(item){
			row.label = item->name;
			row.label += " (";
			row.label += std::to_string(item->techslots);
			row.label += ")";
		}
	}

	if(state.descClickedItemIndex >= 0){
		const int idx = state.descClickedItemIndex;
		state.descClickedItemIndex = -1;
		if(idx >= 0 && idx < static_cast<int>(world.buyableitems.size())){
			BuyableItem * item = world.buyableitems[idx];
			state.techNameStr = "-";
			state.techNameStr += item->name;
			state.techNameStr += "-";
			char desc[1024];
			std::strncpy(desc, item->description, sizeof(desc));
			desc[sizeof(desc) - 1] = '\0';
			int lineNo = 0;
			char * line = std::strtok(desc, "\n");
			while(line && lineNo < 8){
				state.techDescLines[lineNo++] = line;
				line = std::strtok(nullptr, "\n");
			}
			for(int j = lineNo; j < 8; j++) state.techDescLines[j].clear();
		}
	}

	if(state.toggleClickedItemIndex >= 0){
		const int idx = state.toggleClickedItemIndex;
		state.toggleClickedItemIndex = -1;
		if(localpeer && team && idx >= 0
		   && idx < static_cast<int>(world.buyableitems.size())){
			BuyableItem * item = world.buyableitems[idx];
			const bool interactable = (item->techslots <= techslotsleft)
			                       || ((localpeer->techchoices & item->techchoice) != 0);
			if(interactable){
				const Uint32 newChoices = localpeer->techchoices ^ item->techchoice;
				owner.TechPanelSetTech(world, newChoices);
				Config::GetInstance().defaulttechchoices[team->agency] = newChoices;
				Config::GetInstance().Save();
			}
		}
	}

	if(state.backClicked){
		state.backClicked = false;
		owner.ShowGameJoin(ctx);
		return;
	}
}

bool GameTechPanelHandleUiIntent(GameTechPanelState & state,
                                 const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == game_tech_panel_detail::kActionBack){
		state.backClicked = true;
		return true;
	}
	int index = game_tech_panel_detail::SuffixInt(action.id, game_tech_panel_detail::kActionTogglePrefix);
	if(index >= 0){
		state.toggleClickedItemIndex = index;
		return true;
	}
	index = game_tech_panel_detail::SuffixInt(action.id, game_tech_panel_detail::kActionDescriptionPrefix);
	if(index >= 0){
		state.descClickedItemIndex = index;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
