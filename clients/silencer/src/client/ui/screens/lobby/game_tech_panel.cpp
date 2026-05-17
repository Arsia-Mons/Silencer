#include "game_tech_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/button.h"

#include "lobby_screen.h"
#include "screen_context.h"
#include "tech_selected_panel.h"
#include "tech_tree_grid.h"
#include "world.h"
#include "lobby.h"
#include "team.h"
#include "peer.h"
#include "user.h"
#include "buyableitem.h"
#include "config.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

namespace silencer::client_ui::lobby {

namespace game_tech_panel_detail {

constexpr const char * kActionBack = "lobby.game_tech.back";
constexpr const char * kActionTogglePrefix = "lobby.game_tech.toggle.";
constexpr const char * kActionDescriptionPrefix = "lobby.game_tech.description.";

// LobbyRightUpperBox interior layout knobs. Box at (238, 64, 160x121).
constexpr uint16_t kUpperBackPadLeft = 4;
constexpr uint16_t kUpperBackPadTop  = 4;
constexpr uint16_t kUpperPeerColPadLeft = 4;
constexpr uint16_t kUpperPeerColPadTop  = 7;
constexpr uint16_t kUpperPeerRowGap     = 5;

// LobbyRightTallBox interior layout knobs. Box at (398, 64, 232x391).
constexpr uint16_t kTallSlotsPadLeft   = 57;
constexpr uint16_t kTallSlotsPadTop    = 36;

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

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
			if(i >= team->numpeers){ peerindex++; continue; }
			Peer * peer = owner.TechPanelPeer(world, team->peers[i]);
			User * user = peer ? world.lobby.GetUserInfo(peer->accountid) : nullptr;
			state.peerNameStrs[peerindex] = user ? std::string(user->name) : std::string();
			peerindex++;
		}
	}

	if(state.descClickedItemIndex >= 0){
		const int idx = state.descClickedItemIndex;
		state.descClickedItemIndex = -1;
		if(idx >= 0 && idx < static_cast<int>(world.buyableitems.size())){
			BuyableItem * item = world.buyableitems[idx];
			state.techNameStr  = "-";
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

void BuildGameTechUpperTree(GameTechPanelState & state,
                            World & world,
                            Resources & resources,
                            LobbyScreen & owner,
                            silencer::ui::UiInteractionRegistry& interactions) {
	(void)world;
	(void)resources;
	(void)owner;

	// Back To Teams button.
	CLAY({ .id = CLAY_ID("GTechBackWrap"),
	       .layout = { .padding = { game_tech_panel_detail::kUpperBackPadLeft, 0,
	                                game_tech_panel_detail::kUpperBackPadTop,  0 } } }) {
		Button(CLAY_STRING("GameTechBackButton"), CLAY_STRING("Back To Teams"),
		           ButtonOpts{ .variant = ButtonVariant::Chrome,
		                       .size = ButtonSize::Compact },
		           ButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_tech_panel_detail::kActionBack,
		                             /*interactions*/ &interactions });
	}

	// Peer name labels — right-aligned column. ALIGN_X_RIGHT inside a
	// grow-width wrapper aligns each name to the wrapper's right edge.
	CLAY({ .id = CLAY_ID("GTechPeerNames"),
	       .layout = {
	           .padding = { game_tech_panel_detail::kUpperPeerColPadLeft, 4,
	                        game_tech_panel_detail::kUpperPeerColPadTop, 0 },
	           .childGap = game_tech_panel_detail::kUpperPeerRowGap,
	           .childAlignment = { .x = CLAY_ALIGN_X_RIGHT },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		for(int i = 0; i < 3; ++i){
			char idBuf[24];
			std::snprintf(idBuf, sizeof(idBuf), "GTechPeerName%d", i);
			Clay_String wid;
			wid.isStaticallyAllocated = false;
			wid.length = (int32_t)std::strlen(idBuf);
			wid.chars  = idBuf;
			CLAY({ .id = CLAY_SID(wid) }) {
				if(!state.peerNameStrs[i].empty()){
					Text(game_tech_panel_detail::FromStd(state.peerNameStrs[i]),
					     { .size = TextSize::Body });
				}
			}
		}
	}
}

void BuildGameTechTallTree(GameTechPanelState & state,
                           World & world,
                           Resources & resources,
                           LobbyScreen & owner,
                           silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;

	// "Tech slots left: N" — bank 133/w6/eff=129/brightness=144/colorRamp.
	CLAY({ .id = CLAY_ID("GTechSlotsWrap"),
	       .layout = { .padding = { game_tech_panel_detail::kTallSlotsPadLeft, 0,
	                                game_tech_panel_detail::kTallSlotsPadTop, 0 } } }) {
		if(!state.slotsLeftStr.empty()){
			Text(game_tech_panel_detail::FromStd(state.slotsLeftStr),
			     { .size = TextSize::Body,
			       .effect = TextEffect::LegacyPalette(
					   129, static_cast<Uint8>(128 + 16), true) });
		}
	}

	BuildTechTreeGrid(world, owner, interactions);
	BuildTechSelectedPanel(state);
}

}  // namespace silencer::client_ui::lobby
