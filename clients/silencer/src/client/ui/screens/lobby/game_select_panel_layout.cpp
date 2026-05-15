#include "game_select_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/scroll_list.h"

#include <cstdint>
#include <cstring>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonOpts;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;

namespace silencer::client_ui::lobby {

namespace {

// Legacy on-screen coords kept ONLY for inspector hit-rect registration —
// dispatch is label-based; the rect is a fallback for geometric hit-testing.
constexpr int    kListX        = 407;
constexpr int    kListY        = 89;
constexpr Uint16 kListW        = 214;
constexpr Uint16 kListH        = 265;
constexpr Uint8  kListLineH    = 14;
constexpr int    kBtnJoinX     = 436;
constexpr int    kBtnJoinY     = 430;
constexpr int    kBtnSpectateX = 436;
constexpr int    kBtnSpectateY = 408;
constexpr int    kBtnCreateX   = 242;
constexpr int    kBtnCreateY   = 68;

constexpr Uint8  kScrollbarBank = 7;

// LobbyRightUpperBox interior layout knobs.
constexpr uint16_t kUpperBtnPadLeft = 4;
constexpr uint16_t kUpperBtnPadTop  = 4;

// LobbyRightTallBox interior layout knobs.
constexpr uint16_t kTallHeadingPadLeft = 7;
constexpr uint16_t kTallHeadingPadTop  = 6;
constexpr uint16_t kTallListPadLeft    = 9;
constexpr uint16_t kTallListPadTop     = 4;
constexpr uint16_t kTallInfoBlockPadTop  = 4;
constexpr uint16_t kTallInfoPadLeft      = 7;
constexpr uint16_t kTallInfoRowH         = 12;
constexpr uint16_t kTallButtonPadLeft = 38;
constexpr uint16_t kTallButtonRowH    = 21;
constexpr uint16_t kTallSpectatePadTop = 2;
constexpr uint16_t kTallJoinPadTop     = 1;

constexpr int kMaxRows = 256;
Clay_String g_itemSlab[kMaxRows];
constexpr const char * kActionCreate = "lobby.game_select.create";
constexpr const char * kActionJoin = "lobby.game_select.join";
constexpr const char * kActionSpectate = "lobby.game_select.spectate";
constexpr const char * kActionRowPrefix = "lobby.game_select.row";

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

Clay_String StaticId(const char * s) {
	return Clay_String{ true, static_cast<int32_t>(strlen(s)), s };
}

void RegisterButton(silencer::ui::UiInteractionRegistry& interactions,
                    const char * label,
                    const char * actionId,
                    int x,
                    int y) {
	silencer::ui::UiInteractable w;
	w.id = actionId;
	w.labelText = label;
	w.kind  = silencer::ui::UiInteractableKind::Button;
	w.x = x; w.y = y; w.w = 156; w.h = 21;
	interactions.RegisterInteractable(w);
}

void BuildGameSelectInfoBlock(const GameSelectPanelState & state) {
	// Info-block group: 5 fixed-height row slots stacked TOP_TO_BOTTOM.
	// Each slot is height=12 so the layout doesn't reflow when text appears
	// or disappears. The first slot pads down from the list bottom.
	CLAY({ .id = CLAY_ID("GSelInfoBlock"),
	       .layout = {
	           .padding = { kTallInfoPadLeft, 0,
	                        kTallInfoBlockPadTop, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		const struct { const std::string * txt; const char * id; } kInfoRows[5] = {
			{ &state.infoName,     "GSelInfoName" },
			{ &state.infoMap,      "GSelInfoMap" },
			{ &state.infoSecurity, "GSelInfoSec" },
			{ &state.infoCreator,  "GSelInfoCreator" },
			{ &state.infoLimits,   "GSelInfoLimits" },
		};
		for(int i = 0; i < 5; ++i){
			CLAY({ .id = CLAY_SID(StaticId(kInfoRows[i].id)),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(kTallInfoRowH) },
			       } }) {
				if(!kInfoRows[i].txt->empty()){
					BankText(FromStd(*kInfoRows[i].txt),
					         BankTextVariant::Body,
					         {});
				}
			}
		}
	}
}

void BuildGameSelectList(const GameSelectPanelState & state,
                         silencer::ui::UiInteractionRegistry& interactions) {
	const int rowCount = static_cast<int>(state.rows.size());
	const int slotCount = (rowCount < kMaxRows) ? rowCount : kMaxRows;
	for(int i = 0; i < slotCount; ++i){
		g_itemSlab[i] = FromStd(state.rows[i].name);
	}

	ScrollListOpts listOpts;
	listOpts.width          = kListW;
	listOpts.height         = kListH;
	listOpts.lineHeight     = kListLineH;
	listOpts.highlightColor = 180;
	listOpts.textVariant    = BankTextVariant::Body;
	listOpts.scrollbarBank  = kScrollbarBank;

	CLAY({ .id = CLAY_ID("GSelListWrap"),
	       .layout = { .padding = { kTallListPadLeft, 0,
	                                kTallListPadTop,  0 } } }) {
		ScrollList(CLAY_STRING("GSelList"),
		           g_itemSlab,
		           slotCount,
		           state.selectedIndex,
		           state.scrollPos,
		           listOpts,
		           ScrollListHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   kActionRowPrefix,
		                             /*interactions*/ &interactions });
	}

	for(int i = 0; i < slotCount; ++i){
		silencer::ui::UiInteractable w;
		w.id = std::string(kActionRowPrefix) + "." + std::to_string(i);
		w.labelText = state.rows[i].name;
		w.kind  = silencer::ui::UiInteractableKind::ListRow;
		w.x = kListX; w.y = kListY + i * kListLineH;
		w.w = kListW; w.h = kListLineH;
		w.index = i;
		w.selected   = (state.selectedIndex == i);
		interactions.RegisterInteractable(w);
	}
}

void BuildGameSelectActionButtons(const GameSelectPanelState & state,
                                  silencer::ui::UiInteractionRegistry& interactions) {
	// Spectate button — fixed-height slot so the Join slot below sits at a
	// stable y regardless of visibility.
	CLAY({ .id = CLAY_ID("GSelBtnSpectateWrap"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_FIXED(kTallButtonRowH) },
	           .padding = { kTallButtonPadLeft, 0,
	                        kTallSpectatePadTop, 0 },
	       } }) {
		if(state.spectateVisible){
			BankButton(CLAY_STRING("Spectate"),
			           BankButtonVariant::Chrome,
			           BankButtonOpts{},
			           BankButtonHandle{ /*hoveredOut*/ nullptr,
			                             /*actionId*/   kActionSpectate,
			                             /*interactions*/ &interactions });
		}
	}
	if(state.spectateVisible){
		RegisterButton(interactions, "Spectate", kActionSpectate, kBtnSpectateX, kBtnSpectateY);
	}

	// Join button.
	CLAY({ .id = CLAY_ID("GSelBtnJoinWrap"),
	       .layout = {
	           .padding = { kTallButtonPadLeft, 0,
	                        kTallJoinPadTop, 0 },
	       } }) {
		if(state.joinVisible){
			BankButton(CLAY_STRING("Join Game"),
			           BankButtonVariant::Chrome,
			           BankButtonOpts{},
			           BankButtonHandle{ /*hoveredOut*/ nullptr,
			                             /*actionId*/   kActionJoin,
			                             /*interactions*/ &interactions });
		}
	}
	if(state.joinVisible){
		RegisterButton(interactions, "Join Game", kActionJoin, kBtnJoinX, kBtnJoinY);
	}
}

}  // namespace

void BuildGameSelectUpperTree(GameSelectPanelState & state,
                              Resources & resources,
                              silencer::ui::UiInteractionRegistry& interactions) {
	(void)state;
	(void)resources;

	// Create Game button — single flex child of the Upper box.
	CLAY({ .id = CLAY_ID("GSelBtnCreateWrap"),
	       .layout = { .padding = { kUpperBtnPadLeft, 0,
	                                kUpperBtnPadTop,  0 } } }) {
		BankButton(CLAY_STRING("Create Game"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   kActionCreate,
		                             /*interactions*/ &interactions });
	}
	RegisterButton(interactions, "Create Game", kActionCreate, kBtnCreateX, kBtnCreateY);
}

void BuildGameSelectTallTree(GameSelectPanelState & state,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;

	// "Active Games" heading at top of Tall box.
	CLAY({ .id = CLAY_ID("GSelHeaderWrap"),
	       .layout = { .padding = { kTallHeadingPadLeft, 0,
	                                kTallHeadingPadTop,  0 } } }) {
		BankText(CLAY_STRING("Active Games"),
		         BankTextVariant::Heading,
		         {});
	}

	BuildGameSelectList(state, interactions);
	BuildGameSelectInfoBlock(state);
	BuildGameSelectActionButtons(state, interactions);
}

}  // namespace silencer::client_ui::lobby
