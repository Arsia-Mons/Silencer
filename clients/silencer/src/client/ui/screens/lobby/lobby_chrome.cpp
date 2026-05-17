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

constexpr uint16_t kRootPadX = 10;
constexpr uint16_t kRootPadTop = 25;
constexpr const char * kActionGoBack = "lobby.go_back";

}  // namespace lobby_chrome_detail

bool LobbyUseNarrowLayout(int surfaceW) {
	return surfaceW < kLobbyNarrowBreakpointW;
}

uint16_t LobbyTitleBarHeight(bool narrow, const std::string & mapName) {
	return (narrow && !mapName.empty()) ? kLobbyTitleBarMapH : kLobbyTitleBarH;
}

void BuildLobbyTitleBar(const std::string & version,
                        const std::string & mapName,
                        bool narrow,
                        int surfaceW,
                        silencer::ui::UiInteractionRegistry& interactions) {
	(void)surfaceW;
	const uint16_t titleH = LobbyTitleBarHeight(narrow, mapName);
	CLAY(lobby_chrome_detail::Box(lobby_chrome_detail::BoxVariants::Chrome, {
	         .id = CLAY_ID("LobbyTitleBar"),
		         .layout = {
		             .sizing = { CLAY_SIZING_GROW(0),
		                         CLAY_SIZING_FIXED((float)titleH) },
	             .padding = { 5, 5, 4, 4 },
	             .childGap = narrow ? static_cast<uint16_t>(1)
	                                : static_cast<uint16_t>(6),
	             .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
	             .layoutDirection = narrow ? CLAY_TOP_TO_BOTTOM
	                                       : CLAY_LEFT_TO_RIGHT,
	         },
	     })) {
		auto buildTitleRow = [&]() {
			CLAY({ .id = CLAY_ID("LobbyTitleRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(21) },
			           .childGap = 6,
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

				if(!narrow && !mapName.empty()){
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
		};
		buildTitleRow();

		if(narrow && !mapName.empty()){
			Clay_String mstr;
			mstr.isStaticallyAllocated = false;
			mstr.length = (int32_t)mapName.size();
			mstr.chars  = mapName.c_str();
			CLAY({ .id = CLAY_ID("LobbyMapNameNarrow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(15) },
			       } }) {
				lobby_chrome_detail::Text(mstr,
				     { .size = lobby_chrome_detail::TextSize::Title,
				       .effect = lobby_chrome_detail::TextEffect::LegacyPalette(
						   129, 160, true) });
			}
		}
	}

}

}  // namespace silencer::client_ui::lobby
