#include "game_create_panel.h"

#include "client/ui/hooks/use_app.h"
#include "client/ui/hooks/use_lobby.h"
#include "game.h"
#include "screen_context.h"
#include "map.h"
#include "text_wrap.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "primitives/text.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;

namespace silencer::client_ui::lobby {

namespace game_create_panel_map_form_detail {

constexpr Uint8 kMapListLineH = 14;
constexpr Uint16 kInputH = 14;
constexpr Uint16 kButtonH = 21;
constexpr Uint16 kPreviewW = 172;
constexpr Uint16 kPreviewH = 62;

constexpr uint16_t kPanelPad = 6;
constexpr uint16_t kTallSectionGap = 4;
constexpr uint16_t kFooterGap = 4;
constexpr uint16_t kListBorderPad = 1;
constexpr uint16_t kScrollbarWidth = 8;
constexpr uint16_t kPreviewGap = 5;
constexpr int kPreviewOffsetX = -185;
constexpr int kPreviewOffsetY = -30;
constexpr size_t kPreviewNameChars = 29;
constexpr int kHeadingH = 13;

Clay_String FromStd(const std::string & s) {
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

std::string Basename(const std::string & path) {
	const size_t slash = path.find_last_of("/\\");
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

void ResetHoverPreview(GameCreatePanelState & state) {
	state.hoverPreviewVisible = false;
	state.hoverPreviewMapIndex = -1;
	state.hoverPreviewName.clear();
	state.hoverPreviewDescription.clear();
	state.hoverPreviewPixels.clear();
	state.hoverPreviewSurface = {};
	state.hoverPreviewCustomData = {};
}

void HideHoverPreview(GameCreatePanelState & state) {
	state.hoverPreviewVisible = false;
}

void LoadHoverPreview(GameCreatePanelState & state,
                      ScreenContext & ctx,
                      LobbyModel & lobby,
                      int hoveredIndex) {
	if(hoveredIndex < 0 || hoveredIndex >= static_cast<int>(state.maps.size())){
		ResetHoverPreview(state);
		return;
	}

	const std::string & mapLabel = state.maps[hoveredIndex];
	if(state.hoverPreviewMapIndex == hoveredIndex && !state.hoverPreviewPixels.empty()){
		state.hoverPreviewVisible = true;
		return;
	}

	ResetHoverPreview(state);

	const std::string filename = lobby.create.preview_map_path(mapLabel);
	if(filename.empty()) return;

	SDL_IOStream * file = SDL_IOFromFile(filename.c_str(), "rb");
	if(!file) return;

	Map::Header header;
	const bool loaded = Map::LoadHeader(file, header);
	SDL_CloseIO(file);
	if(!loaded) return;

	state.hoverPreviewPixels.resize(static_cast<size_t>(kPreviewW) * kPreviewH);
	if(!Map::UncompressMinimap(
		reinterpret_cast<Uint8 (*)[kPreviewW * kPreviewH]>(state.hoverPreviewPixels.data()),
		header.minimapcompressed,
		header.minimapcompressedsize)) {
		ResetHoverPreview(state);
		return;
	}

	state.hoverPreviewName = Basename(filename);
	if(state.hoverPreviewName.size() > kPreviewNameChars){
		state.hoverPreviewName.resize(kPreviewNameChars);
	}

	char * wrapped = silencer::ui::WordWrapText(header.description, kPreviewNameChars);
	if(wrapped){
		state.hoverPreviewDescription = wrapped;
		delete[] wrapped;
	}

	state.hoverPreviewSurface.pixels = state.hoverPreviewPixels.data();
	state.hoverPreviewSurface.width = kPreviewW;
	state.hoverPreviewSurface.height = kPreviewH;
	state.hoverPreviewCustomData.kind = silencer::clay_bridge::CustomKind::Surface;
	state.hoverPreviewCustomData.payload = &state.hoverPreviewSurface;
	state.hoverPreviewMapIndex = hoveredIndex;
	state.hoverPreviewVisible = true;
}

void UpdateHoverPreview(GameCreatePanelState & state,
                        ScreenContext & ctx,
                        LobbyModel & lobby,
                        int hoveredIndex) {
	if(hoveredIndex < 0){
		HideHoverPreview(state);
		return;
	}
	LoadHoverPreview(state, ctx, lobby, hoveredIndex);
}

int CountPreviewLines(const std::string & text) {
	if(text.empty()) return 0;
	int lines = 1;
	for(char c : text){
		if(c == '\n') ++lines;
	}
	return lines;
}

int ResolveHoveredMapIndex(const GameCreatePanelState & state,
                           const GameCreateTallLayout & layout,
                           const silencer::ui::UiInputState & input,
                           int panelX,
                           int panelY) {
	if(layout.visibleMapRows == 0 || layout.listWidth == 0 || layout.listHeight == 0){
		return -1;
	}
	const int mapCount = static_cast<int>(state.maps.size());
	const int visibleRows = static_cast<int>(layout.visibleMapRows);
	const bool showScrollbar = mapCount > visibleRows;
	const int rowsWidth = showScrollbar
		? std::max(0, static_cast<int>(layout.listWidth) - static_cast<int>(kScrollbarWidth))
		: static_cast<int>(layout.listWidth);
	const int listX = panelX + static_cast<int>(kPanelPad) + static_cast<int>(kListBorderPad);
	const int listY = panelY + static_cast<int>(kPanelPad) + kHeadingH
	                + static_cast<int>(kTallSectionGap)
	                + static_cast<int>(kListBorderPad);
	const int pointerX = static_cast<int>(input.pointer.x);
	const int pointerY = static_cast<int>(input.pointer.y);
	if(pointerX < listX || pointerY < listY ||
	   pointerX >= listX + rowsWidth ||
	   pointerY >= listY + static_cast<int>(layout.listHeight)){
		return -1;
	}
	const int slot = (pointerY - listY) / static_cast<int>(kMapListLineH);
	const int index = static_cast<int>(state.mapScrollPos) + slot;
	return index >= 0 && index < mapCount ? index : -1;
}

}  // namespace game_create_panel_map_form_detail

GameCreateTallLayout ResolveGameCreateTallLayout(Uint16 panelWidth,
                                                 Uint16 panelHeight) {
	GameCreateTallLayout out;
	const int contentW = std::max(
		0,
		static_cast<int>(panelWidth)
			- static_cast<int>(game_create_panel_map_form_detail::kPanelPad) * 2);
	const int contentH = std::max(
		0,
		static_cast<int>(panelHeight)
			- static_cast<int>(game_create_panel_map_form_detail::kPanelPad) * 2);
	const int headingH = game_create_panel_map_form_detail::kHeadingH;
	const int footerH =
		headingH + game_create_panel_map_form_detail::kFooterGap
		+ game_create_panel_map_form_detail::kInputH
		+ game_create_panel_map_form_detail::kFooterGap + headingH
		+ game_create_panel_map_form_detail::kFooterGap
		+ game_create_panel_map_form_detail::kInputH
		+ game_create_panel_map_form_detail::kFooterGap
		+ game_create_panel_map_form_detail::kButtonH;
	const int listH = std::max(
		0,
		contentH - headingH
			- static_cast<int>(game_create_panel_map_form_detail::kTallSectionGap)
			- footerH
			- static_cast<int>(game_create_panel_map_form_detail::kTallSectionGap));
	out.listBoxWidth = static_cast<Uint16>(contentW);
	out.listBoxHeight = static_cast<Uint16>(listH);
	out.listWidth = static_cast<Uint16>(std::max(
		0,
		contentW - static_cast<int>(game_create_panel_map_form_detail::kListBorderPad) * 2));
	out.listHeight = static_cast<Uint16>(std::max(
		0,
		listH - static_cast<int>(game_create_panel_map_form_detail::kListBorderPad) * 2));
	out.inputWidth = static_cast<Uint16>(contentW);
	out.visibleMapRows = static_cast<Uint8>(
		std::min<int>(32, out.listHeight / game_create_panel_map_form_detail::kMapListLineH));
	return out;
}

void GameCreatePanelSyncTallLayout(GameCreatePanelState & state,
                                   ScreenContext & ctx,
                                   LobbyModel & lobby,
                                   Uint16 panelWidth,
                                   Uint16 panelHeight,
                                   int panelX,
                                   int panelY) {
	const GameCreateTallLayout layout =
		ResolveGameCreateTallLayout(panelWidth, panelHeight);
	const int visibleRows = static_cast<int>(layout.visibleMapRows);
	int maxScroll = static_cast<int>(state.maps.size()) - visibleRows;
	if(maxScroll < 0) maxScroll = 0;
	if(state.mapScrollPos > maxScroll){
		state.mapScrollPos = static_cast<Uint16>(maxScroll);
	}

	const silencer::ui::UiInputState & input = ctx.game.CurrentUiInput();
	const int hoveredIndex =
		game_create_panel_map_form_detail::ResolveHoveredMapIndex(
			state, layout, input, panelX, panelY);
	if(hoveredIndex >= 0 && hoveredIndex != state.lastHoveredMapIndex){
		silencer::client_ui::use_app(
			silencer::client_ui::MakeAppProvider(ctx))
			.audio.play_ui_click();
	}
	state.lastHoveredMapIndex = hoveredIndex;
	game_create_panel_map_form_detail::UpdateHoverPreview(
		state, ctx, lobby, hoveredIndex);
}

void BuildGameCreatePreviewOverlay(GameCreatePanelState & state,
                                   ScreenContext & ctx) {
	using namespace game_create_panel_map_form_detail;
	if(!state.hoverPreviewVisible || state.hoverPreviewPixels.empty()) return;

	const silencer::ui::UiInputState & input = ctx.game.CurrentUiInput();
	const int lineHeight = silencer::ui::primitives::TextLineHeight(TextSize::BodySm);
	const int descLines = CountPreviewLines(state.hoverPreviewDescription);
	const int previewHeight = lineHeight + kPreviewGap + kPreviewH
		+ (descLines > 0 ? kPreviewGap + (descLines * lineHeight) : 0);
	const int maxX = std::max(0, input.width - static_cast<int>(kPreviewW));
	const int maxY = std::max(0, input.height - previewHeight);
	int previewX = static_cast<int>(input.pointer.x) + kPreviewOffsetX;
	int previewY = static_cast<int>(input.pointer.y) + kPreviewOffsetY;
	if(previewX < 0) previewX = 0;
	if(previewX > maxX) previewX = maxX;
	if(previewY < 0) previewY = 0;
	if(previewY > maxY) previewY = maxY;

	const silencer::ui::primitives::TextEffect previewEffect =
		silencer::ui::primitives::TextEffect::LegacyPalette(129, 128 + 32, true);

	CLAY({ .id = CLAY_ID("GCrtMapPreview"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(kPreviewW)),
	                       CLAY_SIZING_FIT(0) },
	           .childGap = kPreviewGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .floating = {
	           .offset = { static_cast<float>(previewX), static_cast<float>(previewY) },
	           .zIndex = 2,
	           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
	           .attachTo = CLAY_ATTACH_TO_ROOT,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtMapPreviewName"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(kPreviewW)),
		                       CLAY_SIZING_FIT(0) },
		       } }) {
			Text(FromStd(state.hoverPreviewName),
			     { .size = TextSize::BodySm,
			       .wrap = silencer::ui::primitives::TextWrap::None,
			       .effect = previewEffect });
		}

		CLAY({ .id = CLAY_ID("GCrtMapPreviewMinimap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(kPreviewW)),
		                       CLAY_SIZING_FIXED(static_cast<float>(kPreviewH)) },
		       },
		       .custom = { .customData = &state.hoverPreviewCustomData } }) {}

		if(!state.hoverPreviewDescription.empty()){
			CLAY({ .id = CLAY_ID("GCrtMapPreviewDesc"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(kPreviewW)),
			                       CLAY_SIZING_FIT(0) },
			       } }) {
				Text(FromStd(state.hoverPreviewDescription),
				     { .size = TextSize::BodySm,
				       .wrap = silencer::ui::primitives::TextWrap::Newlines,
				       .effect = previewEffect });
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
