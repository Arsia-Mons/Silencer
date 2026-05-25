#include "lobby_chrome.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/text_internal.h"
#include "primitives/button.h"
#include "primitives/box.h"

#include <algorithm>
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
constexpr int kTitleRowH = 21;

int ClampInt(int value, int lo, int hi) {
	if(value < lo) return lo;
	if(value > hi) return hi;
	return value;
}

Clay_String ToClayString(const std::string & text) {
	Clay_String out;
	out.isStaticallyAllocated = false;
	out.length = static_cast<int32_t>(text.size());
	out.chars = text.c_str();
	return out;
}

int CenteredTextTop(TextSize size, int boxH) {
	const auto style =
		silencer::ui::primitives::text_internal::ResolveTextRenderStyle(size);
	return silencer::ui::primitives::text_internal::CenteredTextTop(style, boxH);
}

int TextInkBottom(TextSize size, int textTop) {
	const auto style =
		silencer::ui::primitives::text_internal::ResolveTextRenderStyle(size);
	return silencer::ui::primitives::text_internal::TextInkBottom(style, textTop);
}

int BottomAlignedTextTop(TextSize size, int boxH, int targetBottom) {
	const auto style =
		silencer::ui::primitives::text_internal::ResolveTextRenderStyle(size);
	return silencer::ui::primitives::text_internal::BottomAlignedTextTop(
		style, boxH, targetBottom);
}

void BuildAlignedTextSlot(Clay_String clayId,
                          Clay_String text,
                          TextSize size,
                          TextEffect effect,
                          int topPad) {
	CLAY({ .id = CLAY_SID_LOCAL(clayId),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0),
	                       CLAY_SIZING_FIXED(static_cast<float>(kTitleRowH)) },
	           .padding = { 0,
	                        0,
	                        static_cast<uint16_t>(topPad),
	                        0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		Text(text, { .size = size, .effect = effect });
	}
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
	const Clay_String titleText = CLAY_STRING("Silencer");
	const Clay_String versionText = lobby_chrome_detail::ToClayString(version);
	const Clay_String mapText = lobby_chrome_detail::ToClayString(mapName);

	const int titleTop = lobby_chrome_detail::CenteredTextTop(
		lobby_chrome_detail::TextSize::Title,
		lobby_chrome_detail::kTitleRowH);

	int titleBottom = lobby_chrome_detail::TextInkBottom(
		lobby_chrome_detail::TextSize::Title,
		titleTop);
	int mapTop = titleTop;
	if(showMapName){
		mapTop = lobby_chrome_detail::CenteredTextTop(
			lobby_chrome_detail::TextSize::Title,
			lobby_chrome_detail::kTitleRowH);
		titleBottom = std::max(
			titleBottom,
			lobby_chrome_detail::TextInkBottom(
				lobby_chrome_detail::TextSize::Title,
				mapTop));
	}
	const int versionTop = lobby_chrome_detail::BottomAlignedTextTop(
		lobby_chrome_detail::TextSize::Body,
		lobby_chrome_detail::kTitleRowH, titleBottom);
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
		     })) {
		CLAY({ .id = CLAY_ID("LobbyTitleRow"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(static_cast<float>(lobby_chrome_detail::kTitleRowH)) },
		           .childGap = rowGap,
		           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			lobby_chrome_detail::BuildAlignedTextSlot(
				CLAY_STRING("LobbyTitle"),
				titleText,
				lobby_chrome_detail::TextSize::Title,
				lobby_chrome_detail::TextEffect::LegacyPalette(152),
				titleTop);

			lobby_chrome_detail::BuildAlignedTextSlot(
				CLAY_STRING("LobbyVer"),
				versionText,
				lobby_chrome_detail::TextSize::Body,
				lobby_chrome_detail::TextEffect::LegacyPalette(189),
				versionTop);

			if(showMapName){
				lobby_chrome_detail::BuildAlignedTextSlot(
					CLAY_STRING("LobbyMapName"),
					mapText,
					lobby_chrome_detail::TextSize::Title,
					lobby_chrome_detail::TextEffect::LegacyPalette(129, 160, true),
					mapTop);
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
