#include "game_tech_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/button.h"

#include "client/ui/hooks/use_lobby.h"
#include "tech_selected_panel.h"
#include "tech_tree_grid.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
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

// Upper stepped-pane slot interior layout knobs.
constexpr uint16_t kUpperBackPadLeft = 4;
constexpr uint16_t kUpperBackPadRight = 4;
constexpr uint16_t kUpperBackPadTop  = 4;
constexpr uint16_t kUpperPeerColPadLeft = 4;
constexpr uint16_t kUpperPeerColPadTop  = 7;
constexpr uint16_t kUpperPeerRowGap     = 5;

// Tall stepped-pane slot interior layout knobs.
constexpr uint16_t kTallSlotsPadLeft   = 57;
constexpr uint16_t kTallSlotsPadTop    = 36;

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

ButtonOpts FullWidthUpperButtonOpts(Uint16 panelWidth) {
	const int buttonWidth = std::max(
		1,
		static_cast<int>(panelWidth)
			- static_cast<int>(kUpperBackPadLeft)
			- static_cast<int>(kUpperBackPadRight));
	return ButtonOpts{
		.variant = ButtonVariant::Chrome,
		.size = ButtonSize::Auto,
		.minWidth = buttonWidth,
		.maxWidth = buttonWidth,
	};
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

GameTechPanelTickResult GameTechPanelTick(GameTechPanelState & state,
                                          LobbyModel & lobby) {
	GameTechPanelTickResult result;
	const LobbyPregameTechModel::Status status = lobby.pregame.tech.status();
	state.slotsLeftStr = status.slots_left;
	state.peerNameStrs = status.peer_names;

	if(state.descClickedItemIndex >= 0){
		const int idx = state.descClickedItemIndex;
		state.descClickedItemIndex = -1;
		const LobbyPregameTechModel::Description desc =
			lobby.pregame.tech.description(idx);
		if(!desc.name.empty()){
			state.techNameStr = desc.name;
			state.techDescLines = desc.lines;
		}
	}

	if(state.toggleClickedItemIndex >= 0){
		const int idx = state.toggleClickedItemIndex;
		state.toggleClickedItemIndex = -1;
		lobby.pregame.tech.toggle(idx);
	}

	if(state.backClicked){
		state.backClicked = false;
		result.show_roster = true;
		return result;
	}
	return result;
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
                            Uint16 panelWidth,
                            silencer::ui::UiInteractionRegistry& interactions) {
	// Back To Teams button.
	CLAY({ .id = CLAY_ID("GTechBackWrap"),
	       .layout = { .padding = { game_tech_panel_detail::kUpperBackPadLeft, 0,
	                                game_tech_panel_detail::kUpperBackPadTop,  0 } } }) {
		Button(CLAY_STRING("GameTechBackButton"), CLAY_STRING("Back To Teams"),
		           game_tech_panel_detail::FullWidthUpperButtonOpts(panelWidth),
		           ButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_tech_panel_detail::kActionBack,
		                             /*interactions*/ &interactions });
	}

	// Teammate name labels: right-aligned column. ALIGN_X_RIGHT inside a
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
                           LobbyModel & lobby,
                           silencer::ui::UiInteractionRegistry& interactions) {
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

	BuildTechTreeGrid(lobby, interactions);
	BuildTechSelectedPanel(state);
}

}  // namespace silencer::client_ui::lobby
