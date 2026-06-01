#include "lobby_main_area.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "primitives/box.h"

#include "character_panel.h"
#include "chat_panel.h"
#include "game_create_panel.h"
#include "game_join_panel.h"

#include "client/ui/hooks/use_lobby.h"
#include "screen_context.h"
#include "renderdevice.h"

#include <algorithm>
#include <cstdint>

namespace silencer::client_ui::lobby {

namespace lobby_main_area_detail {

using silencer::ui::primitives::Box;
using silencer::ui::primitives::BoxStrokeStyle;
namespace BoxSides = silencer::ui::primitives::BoxSides;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

constexpr int kLegacyBodyW = 620;
constexpr int kLegacyBodyH = 391;
constexpr int kLegacyTopRowContentW = 378;
constexpr int kLegacyCharacterW = 218;
constexpr int kLegacyTallW = 232;

constexpr int kMinTallW = 170;
constexpr int kMaxTallW = 320;
constexpr int kMinChatW = 220;
constexpr int kMinUpperW = 120;
constexpr int kMinCharacterW = 140;
constexpr int kMaxCharacterW = 300;
constexpr int kMinLowerLeftH = 88;
int ClampInt(int value, int lo, int hi) {
	if(value < lo) return lo;
	if(value > hi) return hi;
	return value;
}

int RoundRatio(int actual,
               int numerator,
               int denominator) {
	if(denominator <= 0) return 0;
	return static_cast<int>(
		(static_cast<long long>(actual) * numerator + denominator / 2) / denominator);
}

BoxStrokeStyle OpenRightChrome() {
	BoxStrokeStyle style = BoxVariants::Chrome;
	style.sides = static_cast<Uint8>(BoxSides::Top | BoxSides::Bottom | BoxSides::Left);
	return style;
}

BoxStrokeStyle OpenLeftChrome() {
	BoxStrokeStyle style = BoxVariants::Chrome;
	style.sides = static_cast<Uint8>(BoxSides::Top | BoxSides::Bottom | BoxSides::Right);
	return style;
}

BoxStrokeStyle RightEdgeChrome() {
	BoxStrokeStyle style = BoxVariants::Chrome;
	style.sides = BoxSides::Right;
	return style;
}

void AddBorderBlurRect(RenderDevice * renderdevice, SDL_Rect rect) {
	if(!renderdevice || rect.w <= 0 || rect.h <= 0) return;
	renderdevice->AddLobbyPanelBorderBlurRect(rect);
}

void AddPanelBorderBlur(RenderDevice * renderdevice,
                        int x,
                        int y,
                        int w,
                        int h,
                        Uint8 sides) {
	if(!renderdevice || w <= 0 || h <= 0) return;
	if(sides & BoxSides::Top){
		AddBorderBlurRect(renderdevice, SDL_Rect{ x, y, w, 1 });
	}
	if(sides & BoxSides::Bottom){
		AddBorderBlurRect(renderdevice, SDL_Rect{ x, y + h - 1, w, 1 });
	}
	if(sides & BoxSides::Left){
		AddBorderBlurRect(renderdevice, SDL_Rect{ x, y, 1, h });
	}
	if(sides & BoxSides::Right){
		AddBorderBlurRect(renderdevice, SDL_Rect{ x + w - 1, y, 1, h });
	}
}

void QueueLobbyPanelBorderBlurRects(ScreenContext & ctx,
                                    int bodyX,
                                    int bodyY,
                                    const LobbyMainAreaLayout & layout) {
	RenderDevice * renderdevice = ctx.renderdevice;
	if(!renderdevice) return;
	const int topY = bodyY;
	const int lowerY = bodyY + layout.upperH + layout.regionGap;
	const int rightX = bodyX + layout.topRowW;
	const int characterX = bodyX;
	const int rightUpperX = bodyX + layout.characterW + layout.regionGap;
	const int seamX = bodyX + layout.topRowW - layout.regionGap;

	AddPanelBorderBlur(renderdevice,
	                   characterX, topY,
	                   layout.characterW, layout.upperH,
	                   BoxSides::All);
	AddPanelBorderBlur(renderdevice,
	                   rightUpperX, topY,
	                   layout.rightUpperW, layout.upperH,
	                   static_cast<Uint8>(BoxSides::Top | BoxSides::Bottom | BoxSides::Left));
	AddPanelBorderBlur(renderdevice,
	                   seamX, bodyY + layout.upperH,
	                   layout.regionGap, layout.regionGap,
	                   BoxSides::Right);
	AddPanelBorderBlur(renderdevice,
	                   bodyX, lowerY,
	                   layout.chatW, layout.chatH,
	                   BoxSides::All);
	AddPanelBorderBlur(renderdevice,
	                   seamX, lowerY,
	                   layout.regionGap, layout.chatH,
	                   BoxSides::Right);
	AddPanelBorderBlur(renderdevice,
	                   rightX, bodyY,
	                   layout.rightTallW, layout.rightTallH,
	                   static_cast<Uint8>(BoxSides::Top | BoxSides::Bottom | BoxSides::Right));
}

LobbyMainAreaLayout ResolveSteppedPaneLayout(int bodyW,
                                             int bodyH,
                                             int regionGap) {
	LobbyMainAreaLayout out;
	out.regionGap = regionGap;
	out.upperH = ClampInt(RoundRatio(bodyH, 121, kLegacyBodyH), 84, 156);
	int maxUpperH = std::max(0, bodyH - regionGap - kMinLowerLeftH);
	if(maxUpperH > 0 && out.upperH > maxUpperH){
		out.upperH = maxUpperH;
	}

	int desiredRightTallW =
		ClampInt(RoundRatio(bodyW, kLegacyTallW, kLegacyBodyW), kMinTallW, kMaxTallW);
	const int maxRightTallW = std::max(0, bodyW - kMinChatW);
	if(desiredRightTallW > maxRightTallW){
		desiredRightTallW = maxRightTallW;
	}
	if(maxRightTallW >= kMinTallW && desiredRightTallW < kMinTallW){
		desiredRightTallW = kMinTallW;
	}
	out.rightTallW = desiredRightTallW;
	out.rightTallH = std::max(0, bodyH);
	out.topRowW = std::max(0, bodyW - out.rightTallW);
	out.chatW = std::max(0, out.topRowW - regionGap);
	out.chatH = std::max(0, bodyH - out.upperH - regionGap);

	const int availableTopRowW = std::max(0, out.topRowW - regionGap);
	const int desiredCharacterW = ClampInt(
		RoundRatio(availableTopRowW, kLegacyCharacterW, kLegacyTopRowContentW),
		kMinCharacterW,
		kMaxCharacterW);
	const int maxCharacterW = std::max(0, availableTopRowW - kMinUpperW);
	if(maxCharacterW >= kMinCharacterW){
		out.characterW = ClampInt(desiredCharacterW, kMinCharacterW, maxCharacterW);
	}else{
		out.characterW = maxCharacterW;
	}
	out.rightUpperW = std::max(0, availableTopRowW - out.characterW);
	return out;
}

void BuildRightUpperContents(LobbyMainAreaPanels & panels,
                             ScreenContext & ctx,
                             LobbyModel & lobby,
                             const LobbyMainAreaLayout & layout,
                             silencer::ui::UiInteractionRegistry& interactions) {
	(void)ctx;
	(void)lobby;
	if(panels.gameCreateActive){
		BuildGameCreateUpperTree(
			panels.gameCreate,
			static_cast<Uint16>(std::max(0, layout.rightUpperW)),
			static_cast<Uint16>(std::max(0, layout.upperH)),
			interactions);
	}
}

void BuildRightTallContents(LobbyMainAreaPanels & panels,
                            ScreenContext & ctx,
                            LobbyModel & lobby,
                            const LobbyMainAreaLayout & layout,
                            silencer::ui::UiInteractionRegistry& interactions) {
	if(panels.gameCreateActive){
		BuildGameCreateTallTree(
			panels.gameCreate,
			ctx,
			lobby,
			static_cast<Uint16>(std::max(0, layout.rightTallW)),
			static_cast<Uint16>(std::max(0, layout.rightTallH)),
			interactions);
	}
}

void BuildLobbySteppedPane(LobbyMainAreaPanels & panels,
                           ScreenContext & ctx,
                           LobbyModel & lobby,
                           const LobbyMainAreaLayout & layout,
                           silencer::ui::UiInteractionRegistry& interactions) {
	CLAY({ .id = CLAY_ID("LobbyBody"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       },
	    }) {
		CLAY({ .id = CLAY_ID("LobbyLeftStack"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED((float)layout.topRowW),
		                       CLAY_SIZING_GROW(0) },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		    }) {
			CLAY({ .id = CLAY_ID("LobbyUpperRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED((float)layout.upperH) },
			           .childGap = static_cast<uint16_t>(layout.regionGap),
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       },
			    }) {
				CLAY(Box(BoxVariants::Chrome, {
				         .id = CLAY_ID("LobbyCharacterBox"),
				         .layout = {
				             .sizing = { CLAY_SIZING_FIXED((float)layout.characterW),
				                         CLAY_SIZING_GROW(0) },
				             .layoutDirection = CLAY_TOP_TO_BOTTOM,
				         },
				         .clip = { .horizontal = true, .vertical = true },
				     })) {
					BuildCharacterPanelTree(
						panels.character,
						static_cast<Uint16>(std::max(0, layout.characterW)),
						lobby.character,
						interactions);
				}

				CLAY(Box(OpenRightChrome(), {
				         .id = CLAY_ID("LobbyRightUpperBox"),
				         .layout = {
				             .sizing = { CLAY_SIZING_FIXED((float)layout.rightUpperW),
				                         CLAY_SIZING_GROW(0) },
				             .layoutDirection = CLAY_TOP_TO_BOTTOM,
				         },
				         .clip = { .horizontal = true, .vertical = true },
				     })) {
					BuildRightUpperContents(panels, ctx, lobby, layout, interactions);
				}
			}

				CLAY({ .id = CLAY_ID("LobbyElbowGapRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED((float)layout.regionGap) },
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       },
				    }) {
					CLAY({ .id = CLAY_ID("LobbyElbowGapFill"),
					       .layout = {
					           .sizing = { CLAY_SIZING_GROW(0),
					                       CLAY_SIZING_GROW(0) },
					       },
					    }) {
					}
					CLAY(Box(RightEdgeChrome(), {
					         .id = CLAY_ID("LobbyElbowGapSeam"),
					         .layout = {
					             .sizing = { CLAY_SIZING_FIXED((float)layout.regionGap),
					                         CLAY_SIZING_GROW(0) },
					         },
					     })) {}
				}

			CLAY({ .id = CLAY_ID("LobbyLowerRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_GROW(0) },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       },
			    }) {
				CLAY(Box(BoxVariants::Chrome, {
				         .id = CLAY_ID("LobbyChatBox"),
				         .layout = {
				             .sizing = { CLAY_SIZING_FIXED((float)layout.chatW),
				                         CLAY_SIZING_GROW(0) },
				             .layoutDirection = CLAY_TOP_TO_BOTTOM,
				         },
				         .clip = { .horizontal = true, .vertical = true },
				     })) {
					BuildChatPanelTree(panels.chat,
					                   static_cast<Uint16>(std::max(0, layout.chatW)),
					                   static_cast<Uint16>(std::max(0, layout.chatH)),
					                   interactions);
				}
				CLAY(Box(RightEdgeChrome(), {
				         .id = CLAY_ID("LobbyChatTallSeam"),
				         .layout = {
				             .sizing = { CLAY_SIZING_FIXED((float)layout.regionGap),
				                         CLAY_SIZING_GROW(0) },
				         },
				     })) {}
				}
			}

			CLAY(Box(OpenLeftChrome(), {
			         .id = CLAY_ID("LobbyRightTallBox"),
			         .layout = {
			             .sizing = { CLAY_SIZING_FIXED((float)layout.rightTallW),
			                         CLAY_SIZING_GROW(0) },
			             .layoutDirection = CLAY_TOP_TO_BOTTOM,
			         },
			         .clip = { .horizontal = true, .vertical = true },
			     })) {
			BuildRightTallContents(panels, ctx, lobby, layout, interactions);
		}
	}
}

}  // namespace lobby_main_area_detail

LobbyMainAreaLayout ResolveLobbyMainAreaLayout(int bodyW,
                                               int bodyH,
                                               int regionGap) {
	return lobby_main_area_detail::ResolveSteppedPaneLayout(
		bodyW,
		bodyH,
		regionGap);
}

void BuildLobbyMainArea(LobbyMainAreaPanels & panels,
                        ScreenContext & ctx,
                        LobbyModel & lobby,
                        int bodyX,
                        int bodyY,
                        int bodyW,
                        int bodyH,
                        int regionGap,
                        silencer::ui::UiInteractionRegistry& interactions) {
	const LobbyMainAreaLayout layout =
		ResolveLobbyMainAreaLayout(bodyW, bodyH, regionGap);
	lobby_main_area_detail::QueueLobbyPanelBorderBlurRects(ctx, bodyX, bodyY, layout);
	lobby_main_area_detail::BuildLobbySteppedPane(
		panels,
		ctx,
		lobby,
		layout,
		interactions);
}

}  // namespace silencer::client_ui::lobby
