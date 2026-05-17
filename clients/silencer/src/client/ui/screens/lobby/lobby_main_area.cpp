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

constexpr uint16_t kCharacterW = 218;
constexpr uint16_t kUpperH = 121;
constexpr uint16_t kRightTallW = 232;
constexpr int kLegacyBodyW = 620;
constexpr int kLegacyBodyH = 391;
constexpr int kLegacyLeftColumnW = 378;
constexpr int kMinChatW = 220;
constexpr int kMinUpperPartnerW = 120;
// Matches the old lobby BG's baked panel fill through the palette alpha LUT.
constexpr uint8_t kPanelFillColor = 74;
constexpr uint8_t kPanelFillOpacity = 128;

int ClampInt(int value, int lo, int hi) {
	if(value < lo) return lo;
	if(value > hi) return hi;
	return value;
}

int ScaleLegacyPx(int base,
                  int actual,
                  int legacy,
                  int minValue,
                  int maxValue) {
	if(legacy <= 0) return ClampInt(base, minValue, maxValue);
	const int scaled = static_cast<int>((static_cast<long long>(base) * actual + legacy / 2) / legacy);
	return ClampInt(scaled, minValue, maxValue);
}

struct LobbyBodyMetrics {
	int regionGap = 10;
	int characterW = kCharacterW;
	int upperH = kUpperH;
	int rightTallW = kRightTallW;
	int leftColumnW = kLegacyLeftColumnW;
	int chatH = kLegacyBodyH - kUpperH - 10;
};

LobbyBodyMetrics ResolveWideMetrics(int bodyW,
                                    int bodyH,
                                    int regionGap) {
	LobbyBodyMetrics out;
	out.regionGap = regionGap;
	out.upperH = ScaleLegacyPx(kUpperH, bodyH, kLegacyBodyH, 84, 156);
	int maxUpperH = std::max(0, bodyH - regionGap - 88);
	if(maxUpperH > 0 && out.upperH > maxUpperH){
		out.upperH = maxUpperH;
	}

	int desiredRightTallW = ScaleLegacyPx(kRightTallW, bodyW, kLegacyBodyW, 170, 320);
	int maxRightTallW = std::max(0, bodyW - regionGap - kMinChatW);
	if(desiredRightTallW > maxRightTallW){
		desiredRightTallW = maxRightTallW;
	}
	if(maxRightTallW >= 170 && desiredRightTallW < 170){
		desiredRightTallW = 170;
	}
	out.rightTallW = desiredRightTallW;
	out.leftColumnW = std::max(0, bodyW - out.rightTallW - regionGap);
	out.chatH = std::max(0, bodyH - out.upperH - regionGap);

	const int desiredCharacterW =
		ScaleLegacyPx(kCharacterW, out.leftColumnW, kLegacyLeftColumnW, 128, 300);
	const int maxCharacterW = std::max(0, out.leftColumnW - regionGap - kMinUpperPartnerW);
	if(maxCharacterW >= 140){
		out.characterW = std::max(140, std::min(desiredCharacterW, maxCharacterW));
	}else{
		out.characterW = maxCharacterW;
	}
	return out;
}

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

void BuildWideBody(LobbyMainAreaPanels & panels,
                   ScreenContext & ctx,
                   LobbyScreen & owner,
                   const LobbyBodyMetrics & metrics,
                   silencer::ui::UiInteractionRegistry& interactions) {
	World & world = ctx.world;
	Resources & resources = world.resources;
	CLAY({ .id = CLAY_ID("LobbyLeftMiddleStack"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childGap = static_cast<uint16_t>(metrics.regionGap),
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	     }) {
		CLAY({ .id = CLAY_ID("LobbyUpperRow"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED((float)metrics.upperH) },
		           .childGap = static_cast<uint16_t>(metrics.regionGap),
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       },
		     }) {
			CLAY(Box(BoxVariants::Chrome, {
			         .id = CLAY_ID("LobbyCharacterBox"),
			         .layout = {
			             .sizing = { CLAY_SIZING_FIXED((float)metrics.characterW),
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
			BuildChatPanelTree(panels.chat,
			                   world,
			                   resources,
			                   static_cast<Uint16>(std::max(0, metrics.leftColumnW)),
			                   static_cast<Uint16>(std::max(0, metrics.chatH)),
			                   interactions);
		}
	}

	CLAY(Box(BoxVariants::Chrome, {
	         .id = CLAY_ID("LobbyRightTallBox"),
	         .layout = {
	             .sizing = { CLAY_SIZING_FIXED((float)metrics.rightTallW),
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
                        int bodyW,
                        int bodyH,
                        int regionGap,
                        silencer::ui::UiInteractionRegistry& interactions) {
	const lobby_main_area_detail::LobbyBodyMetrics metrics =
		lobby_main_area_detail::ResolveWideMetrics(bodyW, bodyH, regionGap);

	CLAY({ .id = CLAY_ID("LobbyBody"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childGap = static_cast<uint16_t>(metrics.regionGap),
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       },
	    }) {
		lobby_main_area_detail::BuildWideBody(
			panels,
			ctx,
			owner,
			metrics,
			interactions);
	}
}

}  // namespace silencer::client_ui::lobby
