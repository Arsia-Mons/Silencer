#include "lobby_chrome.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/text_internal.h"
#include "primitives/button.h"
#include "primitives/box.h"
#include "renderer.h"
#include "resources.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

namespace silencer::client_ui::lobby {

namespace lobby_chrome_detail {

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::text_internal::TextRenderStyle;
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

int GlyphOffsetForBank(Uint16 bank) {
	return bank == 132 ? 34 : 33;
}

struct TextInkMetrics {
	bool hasInk = false;
	int minY = 0;
	int maxY = 0;
	int height = 0;
};

TextInkMetrics MeasureTextInk(const Resources & resources,
                              Clay_String text,
                              TextSize size) {
	TextInkMetrics out;
	if(text.length <= 0 || !text.chars) return out;

	const TextRenderStyle style =
		silencer::ui::primitives::text_internal::ResolveTextRenderStyle(size);
	if(style.bank >= resources.spritebank.size()) return out;

	const auto& glyphs = resources.spritebank[style.bank];
	const int glyphOffset = GlyphOffsetForBank(style.bank);
	for(int32_t i = 0; i < text.length; ++i){
		const unsigned char ch = static_cast<unsigned char>(text.chars[i]);
		if(ch == ' ' || ch == 0xA0) continue;
		const int glyphIndex = static_cast<int>(ch) - glyphOffset;
		if(glyphIndex < 0 || glyphIndex >= static_cast<int>(glyphs.size())) continue;
		Surface * glyph = glyphs[glyphIndex].get();
		if(!glyph || glyph->w <= 0 || glyph->h <= 0) continue;

		int glyphMinY = glyph->h;
		int glyphMaxY = -1;
		for(int y = 0; y < glyph->h; ++y){
			for(int x = 0; x < glyph->w; ++x){
				if(Renderer::GetPixel(glyph, x, y) == 0) continue;
				if(y < glyphMinY) glyphMinY = y;
				if(y > glyphMaxY) glyphMaxY = y;
			}
		}
		if(glyphMaxY >= glyphMinY){
			if(!out.hasInk || glyphMinY < out.minY) out.minY = glyphMinY;
			if(!out.hasInk || glyphMaxY + 1 > out.maxY) out.maxY = glyphMaxY + 1;
			out.hasInk = true;
		}
	}

	if(out.hasInk){
		out.height = out.maxY - out.minY;
	}
	return out;
}

int CenteredTextTop(const TextInkMetrics & ink,
                    Uint16 lineHeight,
                    int boxH) {
	const int maxTop = std::max(0, boxH - static_cast<int>(lineHeight));
	if(ink.hasInk && ink.height > 0){
		return ClampInt((boxH - ink.height) / 2 - ink.minY, 0, maxTop);
	}
	return ClampInt((boxH - static_cast<int>(lineHeight)) / 2, 0, maxTop);
}

int InkBottom(const TextInkMetrics & ink,
              Uint16 lineHeight,
              int textTop) {
	return textTop + (ink.hasInk ? ink.maxY : static_cast<int>(lineHeight));
}

int BottomAlignedTextTop(const TextInkMetrics & ink,
                         Uint16 lineHeight,
                         int boxH,
                         int targetBottom) {
	const int maxTop = std::max(0, boxH - static_cast<int>(lineHeight));
	const int inkBottom = ink.hasInk ? ink.maxY : static_cast<int>(lineHeight);
	return ClampInt(targetBottom - inkBottom, 0, maxTop);
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
                        const Resources & resources,
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

	const auto titleInk = lobby_chrome_detail::MeasureTextInk(
		resources, titleText, lobby_chrome_detail::TextSize::Title);
	const int titleTop = lobby_chrome_detail::CenteredTextTop(
		titleInk, silencer::ui::primitives::TextLineHeight(lobby_chrome_detail::TextSize::Title),
		lobby_chrome_detail::kTitleRowH);

	int titleBottom = lobby_chrome_detail::InkBottom(
		titleInk, silencer::ui::primitives::TextLineHeight(lobby_chrome_detail::TextSize::Title),
		titleTop);
	int mapTop = titleTop;
	if(showMapName){
		const auto mapInk = lobby_chrome_detail::MeasureTextInk(
			resources, mapText, lobby_chrome_detail::TextSize::Title);
		mapTop = lobby_chrome_detail::CenteredTextTop(
			mapInk, silencer::ui::primitives::TextLineHeight(lobby_chrome_detail::TextSize::Title),
			lobby_chrome_detail::kTitleRowH);
		titleBottom = std::max(
			titleBottom,
			lobby_chrome_detail::InkBottom(
				mapInk,
				silencer::ui::primitives::TextLineHeight(lobby_chrome_detail::TextSize::Title),
				mapTop));
	}
	const auto versionInk = lobby_chrome_detail::MeasureTextInk(
		resources, versionText, lobby_chrome_detail::TextSize::Body);
	const int versionTop = lobby_chrome_detail::BottomAlignedTextTop(
		versionInk, silencer::ui::primitives::TextLineHeight(lobby_chrome_detail::TextSize::Body),
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
