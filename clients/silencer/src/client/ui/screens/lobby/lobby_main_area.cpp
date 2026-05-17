#include "lobby_main_area.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "primitives/box.h"

#include "character_panel.h"
#include "chat_panel.h"
#include "game_select_panel.h"
#include "game_create_panel.h"
#include "game_join_panel.h"
#include "game_tech_panel.h"
#include "lobby_screen.h"

#include "screen_context.h"
#include "world.h"

#include <algorithm>
#include <cstdint>

namespace silencer::client_ui::lobby {

namespace lobby_main_area_detail {

using silencer::ui::primitives::Box;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

constexpr uint16_t kRegionGap = 10;
constexpr uint16_t kCharacterW = 218;
constexpr uint16_t kUpperH = 121;
constexpr uint16_t kRightTallW = 232;
constexpr uint16_t kRightTallH = 391;
constexpr uint16_t kNarrowChatMinH = 88;
// Matches the old lobby BG's baked panel fill through the palette alpha LUT.
constexpr uint8_t kPanelFillColor = 74;
constexpr uint8_t kPanelFillOpacity = 128;

void BuildRightUpperContents(LobbyMainAreaPanels & panels,
                             ScreenContext & ctx,
                             LobbyScreen & owner,
                             silencer::ui::UiInteractionRegistry& interactions) {
	World & world = ctx.world;
	Resources & resources = world.resources;
	if(panels.gameCreateActive){
		BuildGameCreateUpperTree(panels.gameCreate, resources, interactions);
	}else if(panels.gameJoinActive){
		BuildGameJoinUpperTree(panels.gameJoin, resources, interactions);
	}else if(panels.gameTechActive){
		BuildGameTechUpperTree(panels.gameTech, world, resources, owner, interactions);
	}else{
		BuildGameSelectUpperTree(panels.gameSelect, resources, interactions);
	}
}

void BuildRightTallContents(LobbyMainAreaPanels & panels,
                            ScreenContext & ctx,
                            LobbyScreen & owner,
                            silencer::ui::UiInteractionRegistry& interactions) {
	World & world = ctx.world;
	Resources & resources = world.resources;
	if(panels.gameCreateActive){
		BuildGameCreateTallTree(panels.gameCreate, ctx, resources, interactions);
	}else if(panels.gameJoinActive){
		BuildGameJoinTallTree(panels.gameJoin, resources, interactions);
	}else if(panels.gameTechActive){
		BuildGameTechTallTree(panels.gameTech, world, resources, owner, interactions);
	}else{
		BuildGameSelectTallTree(panels.gameSelect, resources, interactions);
	}
}

void BuildNarrowBody(LobbyMainAreaPanels & panels,
                     ScreenContext & ctx,
                     LobbyScreen & owner,
                     int narrowTallH,
                     silencer::ui::UiInteractionRegistry& interactions) {
	World & world = ctx.world;
	Resources & resources = world.resources;
	CLAY(Box(BoxVariants::Chrome, {
	         .id = CLAY_ID("LobbyCharacterBox"),
	         .layout = {
	             .sizing = { CLAY_SIZING_GROW(0),
	                         CLAY_SIZING_FIXED(kUpperH) },
	             .layoutDirection = CLAY_TOP_TO_BOTTOM,
	         },
	         .backgroundColor = { kPanelFillColor, 0, 0, kPanelFillOpacity },
	         .clip = { .horizontal = true, .vertical = true },
	     })) {
		BuildCharacterPanelTree(panels.character, world, resources, interactions);
	}

	CLAY(Box(BoxVariants::Chrome, {
	         .id = CLAY_ID("LobbyRightUpperBox"),
	         .layout = {
	             .sizing = { CLAY_SIZING_GROW(0),
	                         CLAY_SIZING_FIXED(kUpperH) },
	             .layoutDirection = CLAY_TOP_TO_BOTTOM,
	         },
		         .backgroundColor = { kPanelFillColor, 0, 0, kPanelFillOpacity },
		         .clip = { .horizontal = true, .vertical = true },
	     })) {
		BuildRightUpperContents(panels, ctx, owner, interactions);
	}

	CLAY(Box(BoxVariants::Chrome, {
	         .id = CLAY_ID("LobbyRightTallBox"),
	         .layout = {
	             .sizing = { CLAY_SIZING_GROW(0),
	                         CLAY_SIZING_FIXED((float)narrowTallH) },
	             .layoutDirection = CLAY_TOP_TO_BOTTOM,
	         },
		         .backgroundColor = { kPanelFillColor, 0, 0, kPanelFillOpacity },
		         .clip = { .horizontal = true, .vertical = true },
	     })) {
		BuildRightTallContents(panels, ctx, owner, interactions);
	}

	CLAY(Box(BoxVariants::Chrome, {
	         .id = CLAY_ID("LobbyChatBox"),
	         .layout = {
	             .sizing = { CLAY_SIZING_GROW(0),
	                         CLAY_SIZING_GROW(0) },
	             .layoutDirection = CLAY_TOP_TO_BOTTOM,
	         },
	         .backgroundColor = { kPanelFillColor, 0, 0, kPanelFillOpacity },
	         .clip = { .horizontal = true, .vertical = true },
	     })) {
		BuildChatPanelTree(panels.chat, world, resources, interactions);
	}
}

void BuildWideBody(LobbyMainAreaPanels & panels,
                   ScreenContext & ctx,
                   LobbyScreen & owner,
                   silencer::ui::UiInteractionRegistry& interactions) {
	World & world = ctx.world;
	Resources & resources = world.resources;
	CLAY({ .id = CLAY_ID("LobbyLeftMiddleStack"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childGap = kRegionGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	     }) {
		CLAY({ .id = CLAY_ID("LobbyUpperRow"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(kUpperH) },
		           .childGap = kRegionGap,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       },
		     }) {
			CLAY(Box(BoxVariants::Chrome, {
			         .id = CLAY_ID("LobbyCharacterBox"),
			         .layout = {
			             .sizing = { CLAY_SIZING_FIXED(kCharacterW),
			                         CLAY_SIZING_GROW(0) },
			             .layoutDirection = CLAY_TOP_TO_BOTTOM,
			         },
			         .backgroundColor = { kPanelFillColor, 0, 0, kPanelFillOpacity },
			         .clip = { .horizontal = true, .vertical = true },
			     })) {
				BuildCharacterPanelTree(panels.character, world, resources, interactions);
			}

			CLAY(Box(BoxVariants::Chrome, {
			         .id = CLAY_ID("LobbyRightUpperBox"),
			         .layout = {
			             .sizing = { CLAY_SIZING_GROW(0),
			                         CLAY_SIZING_GROW(0) },
			             .layoutDirection = CLAY_TOP_TO_BOTTOM,
			         },
			         .backgroundColor = { kPanelFillColor, 0, 0, kPanelFillOpacity },
			         .clip = { .horizontal = true, .vertical = true },
			     })) {
				BuildRightUpperContents(panels, ctx, owner, interactions);
			}
		}

		CLAY(Box(BoxVariants::Chrome, {
		         .id = CLAY_ID("LobbyChatBox"),
		         .layout = {
		             .sizing = { CLAY_SIZING_GROW(0),
		                         CLAY_SIZING_GROW(0) },
		             .layoutDirection = CLAY_TOP_TO_BOTTOM,
		         },
		         .backgroundColor = { kPanelFillColor, 0, 0, kPanelFillOpacity },
		         .clip = { .horizontal = true, .vertical = true },
		     })) {
			BuildChatPanelTree(panels.chat, world, resources, interactions);
		}
	}

	CLAY(Box(BoxVariants::Chrome, {
	         .id = CLAY_ID("LobbyRightTallBox"),
	         .layout = {
	             .sizing = { CLAY_SIZING_FIXED(kRightTallW),
	                         CLAY_SIZING_GROW(0) },
	             .layoutDirection = CLAY_TOP_TO_BOTTOM,
	         },
		         .backgroundColor = { kPanelFillColor, 0, 0, kPanelFillOpacity },
		         .clip = { .horizontal = true, .vertical = true },
	     })) {
		BuildRightTallContents(panels, ctx, owner, interactions);
	}
}

}  // namespace lobby_main_area_detail

void BuildLobbyMainArea(LobbyMainAreaPanels & panels,
                        ScreenContext & ctx,
                        LobbyScreen & owner,
                        bool narrow,
                        int bodyH,
                        silencer::ui::UiInteractionRegistry& interactions) {
	const int narrowTallFit = bodyH - ((int)lobby_main_area_detail::kUpperH * 2)
	                        - ((int)lobby_main_area_detail::kRegionGap * 3) - (int)lobby_main_area_detail::kNarrowChatMinH;
	const int narrowTallH = std::max(0, std::min((int)lobby_main_area_detail::kRightTallH, narrowTallFit));

	CLAY({ .id = CLAY_ID("LobbyBody"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childGap = lobby_main_area_detail::kRegionGap,
	           .layoutDirection = narrow ? CLAY_TOP_TO_BOTTOM
	                                     : CLAY_LEFT_TO_RIGHT,
	       },
	    }) {
		if(narrow){
			lobby_main_area_detail::BuildNarrowBody(panels, ctx, owner, narrowTallH, interactions);
		}else{
			lobby_main_area_detail::BuildWideBody(panels, ctx, owner, interactions);
		}
	}
}

}  // namespace silencer::client_ui::lobby
