#include "game_tech_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/button.h"

#include "screen_context.h"
#include "tech_selected_panel.h"
#include "tech_tree_grid.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

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

template <typename Text>
bool StartsWith(const Text & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

template <typename Text>
int SuffixInt(const Text & value, const char * prefix) {
	if(!StartsWith(value, prefix)) return -1;
	return std::atoi(value.c_str() + std::strlen(prefix));
}

void CopySnapshot(GameTechPanelState & state,
                  const ScreenContext::LobbyTechSnapshot & snapshot) {
	state.slotsLeftStr = snapshot.slotsLeft;
	state.peerNameStrs = snapshot.peerNames;
	state.rows.clear();
	state.rows.reserve(snapshot.rows.size());
	for(const ScreenContext::LobbyTechGridRow & source : snapshot.rows){
		GameTechGridRow row;
		row.itemIndex = source.itemIndex;
		row.label = source.label;
		row.labelBrightness = source.labelBrightness;
		for(size_t i = 0; i < row.cells.size(); ++i){
			row.cells[i].draw = source.cells[i].draw;
			row.cells[i].selected = source.cells[i].selected;
			row.cells[i].brightness = source.cells[i].brightness;
		}
		state.rows.push_back(std::move(row));
	}
}

}  // namespace game_tech_panel_detail

void GameTechPanelInit(GameTechPanelState & state) {
	state = GameTechPanelState{};
}

void GameTechPanelTick(GameTechPanelState & state,
                       ScreenContext & ctx) {
	if(state.toggleClickedItemIndex >= 0){
		const int idx = state.toggleClickedItemIndex;
		state.toggleClickedItemIndex = -1;
		ctx.ToggleLobbyTechChoice(idx);
	}

	game_tech_panel_detail::CopySnapshot(state, ctx.CurrentLobbyTechSnapshot());

	if(state.descClickedItemIndex >= 0){
		const int idx = state.descClickedItemIndex;
		state.descClickedItemIndex = -1;
		const ScreenContext::LobbyTechItemDetails details =
			ctx.LobbyTechItemDetailsForIndex(idx);
		if(details.found){
			state.techNameStr = details.title;
			state.techDescLines = details.descriptionLines;
		}else{
			state.techNameStr.clear();
			for(std::string & line : state.techDescLines) line.clear();
		}
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

	// Participant name labels — right-aligned column. ALIGN_X_RIGHT inside a
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

	BuildTechTreeGrid(state, interactions);
	BuildTechSelectedPanel(state);
}

}  // namespace silencer::client_ui::lobby
