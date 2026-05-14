#include "lobby_chrome.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/box.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace silencer::client_ui::lobby {

namespace {

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::Box;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

constexpr uint16_t kRootPadX = 10;
constexpr uint16_t kRootPadTop = 25;
constexpr const char * kActionGoBack = "lobby.go_back";

}  // namespace

bool LobbyUseNarrowLayout(int surfaceW) {
	return surfaceW < kLobbyNarrowBreakpointW;
}

uint16_t LobbyTitleBarHeight(bool narrow, const std::string & mapName) {
	return (narrow && !mapName.empty()) ? kLobbyTitleBarMapH : kLobbyTitleBarH;
}

void BuildLobbyTitleBar(const std::string & version,
                        const std::string & mapName,
                        bool narrow,
                        int surfaceW) {
	const uint16_t titleH = LobbyTitleBarHeight(narrow, mapName);
	CLAY(Box(BoxVariants::Chrome, {
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
					BankText(CLAY_STRING("Silencer"),
					         BankTextVariant::Title,
					         { .effectColor = 152 });
				}

				Clay_String verstr;
				verstr.isStaticallyAllocated = false;
				verstr.length = (int32_t)version.size();
				verstr.chars  = version.c_str();
				CLAY({ .id = CLAY_ID("LobbyVer") }) {
					BankText(verstr,
					         BankTextVariant::Body,
					         { .effectColor = 189 });
				}

				if(!narrow && !mapName.empty()){
					Clay_String mstr;
					mstr.isStaticallyAllocated = false;
					mstr.length = (int32_t)mapName.size();
					mstr.chars  = mapName.c_str();
					CLAY({ .id = CLAY_ID("LobbyMapName") }) {
						BankText(mstr,
						         BankTextVariant::Title,
						         { .effectColor = 129,
						           .brightness  = 160,
						           .colorRamp   = true });
					}
				}

				CLAY({ .id = CLAY_ID("LobbyTitleBarSpacer"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(0) },
				       } }) {}

				CLAY({ .id = CLAY_ID("LobbyGoBackWrap") }) {
					BankButton(CLAY_STRING("Go Back"),
					           BankButtonVariant::Chrome,
					           {},
					           { .hoveredOut = nullptr,
					             .actionId = kActionGoBack });
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
				BankText(mstr,
				         BankTextVariant::Title,
				         { .effectColor = 129,
				           .brightness  = 160,
				           .colorRamp   = true });
			}
		}
	}

	silencer::ui::automation::Widget gb;
	gb.id = kActionGoBack;
	gb.labelText = "Go Back";
	gb.kind = silencer::ui::automation::WidgetKind::Button;
	gb.x = std::max(0, surfaceW - (int)kRootPadX - 5 - 156);
	gb.y = kRootPadTop + 4;
	gb.w = 156; gb.h = 21;
	silencer::ui::automation::Register(gb);
}

}  // namespace silencer::client_ui::lobby
