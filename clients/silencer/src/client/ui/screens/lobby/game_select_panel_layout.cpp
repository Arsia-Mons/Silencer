#include "game_select_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/box.h"
#include "primitives/text.h"
#include "primitives/button.h"
#include "primitives/scroll_list.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Box;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

namespace silencer::client_ui::lobby {

namespace game_select_panel_layout_detail {

constexpr Uint8  kListLineH    = 14;

constexpr Uint8  kScrollbarBank = 7;

// Upper stepped-pane slot interior layout knobs.
constexpr uint16_t kUpperBtnPadLeft = 4;
constexpr uint16_t kUpperBtnPadRight = 4;
constexpr uint16_t kUpperBtnPadTop  = 4;

// Tall stepped-pane slot interior layout knobs.
constexpr uint16_t kTallHeadingPadLeft = 7;
constexpr uint16_t kTallHeadingPadTop  = 6;
constexpr uint16_t kTallListPadLeft    = 9;
constexpr uint16_t kTallListPadTop     = 4;
constexpr uint16_t kTallListBorderPad  = 1;
constexpr uint16_t kTallFooterPadTop   = 4;
constexpr uint16_t kTallFooterGap      = 4;
constexpr uint16_t kTallInfoPadLeft    = 7;
constexpr uint16_t kTallInfoRowH       = 12;
constexpr uint16_t kTallButtonRowH     = 21;
constexpr uint16_t kTallButtonGap      = 2;

constexpr int kMaxRows = 256;
Clay_String g_itemSlab[kMaxRows];
constexpr const char * kActionCreate = "lobby.game_select.create";
constexpr const char * kActionJoin = "lobby.game_select.join";
constexpr const char * kActionSpectate = "lobby.game_select.spectate";
constexpr const char * kActionRowPrefix = "lobby.game_select.row";

struct GameSelectTallLayout {
	Uint16 listBoxWidth  = 0;
	Uint16 listBoxHeight = 0;
	Uint16 listWidth  = 0;
	Uint16 listHeight = 0;
};

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

ButtonOpts FullWidthUpperButtonOpts(Uint16 panelWidth) {
	const int buttonWidth = std::max(
		1,
		static_cast<int>(panelWidth)
			- static_cast<int>(kUpperBtnPadLeft)
			- static_cast<int>(kUpperBtnPadRight));
	return ButtonOpts{
		.variant = ButtonVariant::Chrome,
		.size = ButtonSize::Auto,
		.minWidth = buttonWidth,
		.maxWidth = buttonWidth,
	};
}

GameSelectTallLayout ResolveTallLayout(Uint16 panelWidth,
                                       Uint16 panelHeight) {
	GameSelectTallLayout out;
	const int headingH = silencer::ui::primitives::TextLineHeight(TextSize::Heading);
	const int titleWrapH = static_cast<int>(kTallHeadingPadTop) + headingH;
	const int infoH = 5 * static_cast<int>(kTallInfoRowH);
	const int footerH =
		static_cast<int>(kTallFooterPadTop)
		+ infoH
		+ static_cast<int>(kTallFooterGap)
		+ static_cast<int>(kTallButtonRowH)
		+ static_cast<int>(kTallButtonGap)
		+ static_cast<int>(kTallButtonRowH);
	out.listBoxWidth = static_cast<Uint16>(std::max(
		0,
		static_cast<int>(panelWidth) - static_cast<int>(kTallListPadLeft) * 2));
	out.listBoxHeight = static_cast<Uint16>(std::max(
		0,
		static_cast<int>(panelHeight) - titleWrapH
		       - static_cast<int>(kTallListPadTop) - footerH));
	out.listWidth = static_cast<Uint16>(std::max(
		0,
		static_cast<int>(out.listBoxWidth) - static_cast<int>(kTallListBorderPad) * 2));
	out.listHeight = static_cast<Uint16>(std::max(
		0,
		static_cast<int>(out.listBoxHeight) - static_cast<int>(kTallListBorderPad) * 2));
	return out;
}

void BuildGameSelectInfoBlock(const GameSelectPanelState & state) {
	// Info-block group: 5 fixed-height row slots stacked TOP_TO_BOTTOM.
	// Each slot is height=12 so the layout doesn't reflow when text appears
	// or disappears.
	CLAY({ .id = CLAY_ID("GSelInfoBlock"),
	       .layout = {
	           .padding = { kTallInfoPadLeft, 0, 0, 0 },
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
					Text(FromStd(*kInfoRows[i].txt),
					     { .size = TextSize::Body });
				}
			}
		}
	}
}

void BuildGameSelectList(const GameSelectPanelState & state,
                         const GameSelectTallLayout & layout,
                         silencer::ui::UiInteractionRegistry& interactions) {
	const int rowCount = static_cast<int>(state.rows.size());
	const int slotCount = (rowCount < kMaxRows) ? rowCount : kMaxRows;
	for(int i = 0; i < slotCount; ++i){
		g_itemSlab[i] = FromStd(state.rows[i].name);
	}

	ScrollListOpts listOpts;
	listOpts.width          = layout.listWidth;
	listOpts.height         = layout.listHeight;
	listOpts.lineHeight     = kListLineH;
	listOpts.highlightColor = 180;
	listOpts.text.size      = TextSize::Body;
	listOpts.scrollbarBank  = kScrollbarBank;

	CLAY({ .id = CLAY_ID("GSelListWrap"),
	       .layout = { .padding = { kTallListPadLeft, 0,
	                                kTallListPadTop,  0 } } }) {
		CLAY(Box(BoxVariants::Inset, {
		         .id = CLAY_ID("GSelListBorder"),
		         .layout = {
		             .sizing = { CLAY_SIZING_FIXED(static_cast<float>(layout.listBoxWidth)),
		                         CLAY_SIZING_FIXED(static_cast<float>(layout.listBoxHeight)) },
		             .padding = { kTallListBorderPad, kTallListBorderPad,
		                          kTallListBorderPad, kTallListBorderPad },
		         },
		     })) {
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
	}
}

void BuildGameSelectActionButtons(const GameSelectPanelState & state,
                                  silencer::ui::UiInteractionRegistry& interactions) {
	CLAY({ .id = CLAY_ID("GSelButtons"),
	       .layout = {
	           .childGap = kTallButtonGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		// Fixed-height slots keep the footer stable regardless of which
		// actions are available for the current selection.
		CLAY({ .id = CLAY_ID("GSelBtnSpectateWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(kTallButtonRowH) },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
		       } }) {
			if(state.spectateVisible){
				Button(CLAY_STRING("GameSelectSpectateButton"), CLAY_STRING("Spectate"),
				           ButtonOpts{ .variant = ButtonVariant::Chrome,
				                       .size = ButtonSize::Compact },
				           ButtonHandle{ /*hoveredOut*/ nullptr,
				                             /*actionId*/   kActionSpectate,
				                             /*interactions*/ &interactions });
			}
		}

		CLAY({ .id = CLAY_ID("GSelBtnJoinWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(kTallButtonRowH) },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
		       } }) {
			if(state.joinVisible){
				Button(CLAY_STRING("GameSelectJoinButton"), CLAY_STRING("Join Game"),
				           ButtonOpts{ .variant = ButtonVariant::Chrome,
				                       .size = ButtonSize::Compact },
				           ButtonHandle{ /*hoveredOut*/ nullptr,
				                             /*actionId*/   kActionJoin,
				                             /*interactions*/ &interactions });
			}
		}
	}
}

}  // namespace game_select_panel_layout_detail

void BuildGameSelectUpperTree(GameSelectPanelState & state,
                              Uint16 panelWidth,
                              Resources & resources,
                              silencer::ui::UiInteractionRegistry& interactions) {
	(void)state;
	(void)resources;

	// Create Game button — single flex child of the Upper box.
	CLAY({ .id = CLAY_ID("GSelBtnCreateWrap"),
	       .layout = { .padding = { game_select_panel_layout_detail::kUpperBtnPadLeft, 0,
	                                game_select_panel_layout_detail::kUpperBtnPadTop,  0 } } }) {
		Button(CLAY_STRING("GameSelectCreateButton"), CLAY_STRING("Create Game"),
		           game_select_panel_layout_detail::FullWidthUpperButtonOpts(panelWidth),
		           ButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_select_panel_layout_detail::kActionCreate,
		                             /*interactions*/ &interactions });
	}
}

void BuildGameSelectTallTree(GameSelectPanelState & state,
                             Uint16 panelWidth,
                             Uint16 panelHeight,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;
	const game_select_panel_layout_detail::GameSelectTallLayout layout =
		game_select_panel_layout_detail::ResolveTallLayout(panelWidth, panelHeight);

	// "Active Games" heading at top of Tall box.
	CLAY({ .id = CLAY_ID("GSelHeaderWrap"),
	       .layout = { .padding = { game_select_panel_layout_detail::kTallHeadingPadLeft, 0,
	                                game_select_panel_layout_detail::kTallHeadingPadTop,  0 } } }) {
		Text(CLAY_STRING("Active Games"),
		     { .size = TextSize::Heading });
	}

	game_select_panel_layout_detail::BuildGameSelectList(state, layout, interactions);
	CLAY({ .id = CLAY_ID("GSelFooter"),
	       .layout = {
	           .padding = { 0, 0, game_select_panel_layout_detail::kTallFooterPadTop, 0 },
	           .childGap = game_select_panel_layout_detail::kTallFooterGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		game_select_panel_layout_detail::BuildGameSelectInfoBlock(state);
		game_select_panel_layout_detail::BuildGameSelectActionButtons(state, interactions);
	}
}

}  // namespace silencer::client_ui::lobby
