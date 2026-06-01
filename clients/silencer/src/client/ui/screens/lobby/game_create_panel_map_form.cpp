#include "game_create_panel.h"

#include "client/ui/hooks/use_app.h"
#include "client/ui/hooks/use_lobby.h"
#include "map.h"
#include "text_wrap.h"
#include "ui/runtime/UiInputState.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

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
constexpr int kPreviewLineH = 11;
constexpr int kPreviewDescLineSlots = 12;

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
}

void HideHoverPreview(GameCreatePanelState & state) {
	state.hoverPreviewVisible = false;
}

void LoadHoverPreview(GameCreatePanelState & state,
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

	state.hoverPreviewMapIndex = hoveredIndex;
	state.hoverPreviewVisible = true;
}

void UpdateHoverPreview(GameCreatePanelState & state,
                        LobbyModel & lobby,
                        int hoveredIndex) {
	if(hoveredIndex < 0){
		HideHoverPreview(state);
		return;
	}
	LoadHoverPreview(state, lobby, hoveredIndex);
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
                                   const silencer::ui::UiInputState & input,
                                   const AppAudioModel & audio,
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

	const int hoveredIndex =
		game_create_panel_map_form_detail::ResolveHoveredMapIndex(
			state, layout, input, panelX, panelY);
	if(hoveredIndex >= 0 && hoveredIndex != state.lastHoveredMapIndex){
		audio.play_ui_click();
	}
	state.lastHoveredMapIndex = hoveredIndex;
	game_create_panel_map_form_detail::UpdateHoverPreview(
		state, lobby, hoveredIndex);
}

GameCreatePreviewOverlayLayout ResolveGameCreatePreviewOverlayLayout(
	const GameCreatePanelState & state,
	const silencer::ui::UiInputState & input) {
	using namespace game_create_panel_map_form_detail;
	GameCreatePreviewOverlayLayout out;
	if(!state.hoverPreviewVisible || state.hoverPreviewPixels.empty()) return out;

	const int descLines = std::min(
		CountPreviewLines(state.hoverPreviewDescription),
		kPreviewDescLineSlots);
	const int previewHeight = kPreviewLineH + kPreviewGap + kPreviewH
		+ (descLines > 0 ? kPreviewGap + (descLines * kPreviewLineH) : 0);
	const int maxX = std::max(0, input.width - static_cast<int>(kPreviewW));
	const int maxY = std::max(0, input.height - previewHeight);
	int previewX = static_cast<int>(input.pointer.x) + kPreviewOffsetX;
	int previewY = static_cast<int>(input.pointer.y) + kPreviewOffsetY;
	if(previewX < 0) previewX = 0;
	if(previewX > maxX) previewX = maxX;
	if(previewY < 0) previewY = 0;
	if(previewY > maxY) previewY = maxY;

	out.visible = true;
	out.x = previewX;
	out.y = previewY;
	out.width = kPreviewW;
	out.height = previewHeight;
	out.lineHeight = kPreviewLineH;
	out.gap = kPreviewGap;
	out.bitmapWidth = kPreviewW;
	out.bitmapHeight = kPreviewH;
	return out;
}

}  // namespace silencer::client_ui::lobby
