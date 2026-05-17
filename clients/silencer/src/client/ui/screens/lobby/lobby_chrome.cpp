#include "lobby_chrome.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/button.h"
#include "primitives/box.h"

#include <cstdint>
#include <string>

namespace silencer::client_ui::lobby {

namespace lobby_chrome_detail {

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Box;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

constexpr const char * kActionGoBack = "lobby.go_back";
constexpr uint8_t kPanelFillColor = 74;
constexpr uint8_t kPanelFillOpacity = 128;

int ClampInt(int value, int lo, int hi) {
	if(value < lo) return lo;
	if(value > hi) return hi;
	return value;
}

}  // namespace lobby_chrome_detail

uint16_t LobbyTitleBarHeight() {
	return kLobbyTitleBarH;
}

void BuildLobbyTitleBar(const std::string & version,
                        const std::string & mapName,
                        int surfaceW,
                        silencer::ui::UiInteractionRegistry& interactions) {
	const uint16_t titleH = LobbyTitleBarHeight();
	const uint16_t padX = static_cast<uint16_t>(
		lobby_chrome_detail::ClampInt((surfaceW * 5) / 640, 5, 10));
	const uint16_t rowGap = static_cast<uint16_t>(
		lobby_chrome_detail::ClampInt((surfaceW * 6) / 640, 4, 10));
	const bool showMapName = !mapName.empty() && surfaceW >= 700;
	CLAY(lobby_chrome_detail::Box(lobby_chrome_detail::BoxVariants::Chrome, {
		         .id = CLAY_ID("LobbyTitleBar"),
			         .layout = {
			             .sizing = { CLAY_SIZING_GROW(0),
			                         CLAY_SIZING_FIXED((float)titleH) },
		             .padding = { padX, padX, 4, 4 },
		             .childGap = rowGap,
		             .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
		             .layoutDirection = CLAY_LEFT_TO_RIGHT,
		         },
		         .backgroundColor = { lobby_chrome_detail::kPanelFillColor, 0, 0,
		                              lobby_chrome_detail::kPanelFillOpacity },
		     })) {
		CLAY({ .id = CLAY_ID("LobbyTitleRow"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(21) },
		           .childGap = rowGap,
		           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			CLAY({ .id = CLAY_ID("LobbyTitle") }) {
				lobby_chrome_detail::Text(CLAY_STRING("Silencer"),
				     { .size = lobby_chrome_detail::TextSize::Title,
				       .effect = lobby_chrome_detail::TextEffect::LegacyPalette(152) });
			}

			Clay_String verstr;
			verstr.isStaticallyAllocated = false;
			verstr.length = (int32_t)version.size();
			verstr.chars  = version.c_str();
			CLAY({ .id = CLAY_ID("LobbyVer") }) {
				lobby_chrome_detail::Text(verstr,
				     { .size = lobby_chrome_detail::TextSize::Body,
				       .effect = lobby_chrome_detail::TextEffect::LegacyPalette(189) });
			}

			if(showMapName){
				Clay_String mstr;
				mstr.isStaticallyAllocated = false;
				mstr.length = (int32_t)mapName.size();
				mstr.chars  = mapName.c_str();
				CLAY({ .id = CLAY_ID("LobbyMapName") }) {
					lobby_chrome_detail::Text(mstr,
					     { .size = lobby_chrome_detail::TextSize::Title,
					       .effect = lobby_chrome_detail::TextEffect::LegacyPalette(
							   129, 160, true) });
				}
			}

			CLAY({ .id = CLAY_ID("LobbyTitleBarSpacer"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(0) },
			       } }) {}

			CLAY({ .id = CLAY_ID("LobbyGoBackWrap") }) {
				lobby_chrome_detail::Button(CLAY_STRING("LobbyGoBackButton"), CLAY_STRING("Go Back"),
				           lobby_chrome_detail::ButtonOpts{ .variant = lobby_chrome_detail::ButtonVariant::Chrome,
				                                           .size = lobby_chrome_detail::ButtonSize::Compact },
				           lobby_chrome_detail::ButtonHandle{ nullptr, lobby_chrome_detail::kActionGoBack, &interactions });
			}
		}
	}

}

}  // namespace silencer::client_ui::lobby
