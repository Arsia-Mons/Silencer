#include "game_create_panel.h"

#include <algorithm>
#include <cstdint>

namespace silencer::client_ui::lobby {

namespace game_create_panel_options_detail {

constexpr int kOptionRowCount = 6;

constexpr uint16_t kPanelPad = 6;
constexpr uint16_t kTitleH = 13;
constexpr uint16_t kInsetBorderWidth = 1;
constexpr uint16_t kFormPadLeft = 4;
constexpr uint16_t kFormPadTop = 2;
constexpr uint16_t kFormPadBottom = 2;
constexpr uint16_t kFormRowH = 14;
constexpr uint16_t kFormRowGap = 3;
constexpr uint16_t kScrollbarWidth = 8;
constexpr uint16_t kScrollbarGap = 2;
constexpr uint16_t kValueColumnMinW = 30;
constexpr uint16_t kValueColumnMaxW = 72;
constexpr uint16_t kScrollbarTrackPad = 1;
constexpr uint16_t kScrollbarThumbMinH = 12;

void ClampOptionsScroll(GameCreatePanelState & state,
                        const GameCreateOptionsLayout & layout) {
	state.optionsVisibleRows = layout.visibleRows;
	state.optionsMaxScroll = layout.scrollMax;
	int nextPosition = static_cast<int>(state.optionsScrollPosition)
	                 + state.optionsScrollDelta;
	if(nextPosition < 0) nextPosition = 0;
	if(nextPosition > static_cast<int>(layout.scrollMax)){
		nextPosition = static_cast<int>(layout.scrollMax);
	}
	state.optionsScrollPosition = static_cast<Uint16>(nextPosition);
	state.optionsScrollDelta = 0;
	if(state.optionsScrollPosition > layout.scrollMax){
		state.optionsScrollPosition = layout.scrollMax;
	}
}

}  // namespace game_create_panel_options_detail

GameCreateOptionsLayout ResolveGameCreateOptionsLayout(Uint16 panelWidth,
                                                       Uint16 panelHeight) {
	GameCreateOptionsLayout out;
	const int contentW = std::max(
		0,
		static_cast<int>(panelWidth)
			- static_cast<int>(game_create_panel_options_detail::kPanelPad) * 2);
	const int contentH = std::max(
		0,
		static_cast<int>(panelHeight)
			- static_cast<int>(game_create_panel_options_detail::kPanelPad) * 2);
	out.titleHeight = game_create_panel_options_detail::kTitleH;

	const int borderH = std::max(
		0,
		contentH - static_cast<int>(out.titleHeight)
			- static_cast<int>(game_create_panel_options_detail::kFormRowGap));
	const int viewportH = std::max(
		0,
		borderH
			- static_cast<int>(game_create_panel_options_detail::kInsetBorderWidth) * 2
			- static_cast<int>(game_create_panel_options_detail::kFormPadTop)
			- static_cast<int>(game_create_panel_options_detail::kFormPadBottom));
	out.viewportHeight = static_cast<Uint16>(viewportH);

	if(viewportH > 0){
		int visibleRows =
			(viewportH + static_cast<int>(game_create_panel_options_detail::kFormRowGap))
			/ (static_cast<int>(game_create_panel_options_detail::kFormRowH)
			   + static_cast<int>(game_create_panel_options_detail::kFormRowGap));
		if(visibleRows < 1) visibleRows = 1;
		if(visibleRows > game_create_panel_options_detail::kOptionRowCount){
			visibleRows = game_create_panel_options_detail::kOptionRowCount;
		}
		out.visibleRows = static_cast<Uint8>(visibleRows);
	}

	if(out.visibleRows > 0 &&
	   out.visibleRows < game_create_panel_options_detail::kOptionRowCount){
		out.scrollMax = static_cast<Uint16>(
			game_create_panel_options_detail::kOptionRowCount - out.visibleRows);
	}
	out.showScrollbar = out.scrollMax > 0;

	int formContentW = std::max(
		0,
		contentW
			- static_cast<int>(game_create_panel_options_detail::kInsetBorderWidth) * 2
			- static_cast<int>(game_create_panel_options_detail::kFormPadLeft) * 2);
	if(out.showScrollbar){
		formContentW -=
			static_cast<int>(game_create_panel_options_detail::kScrollbarWidth)
			+ static_cast<int>(game_create_panel_options_detail::kScrollbarGap);
		if(formContentW < 0) formContentW = 0;
	}
	out.viewportWidth = static_cast<Uint16>(formContentW);
	if(formContentW > 0){
		int valueColumnWidth = formContentW / 3;
		if(valueColumnWidth < static_cast<int>(game_create_panel_options_detail::kValueColumnMinW)){
			valueColumnWidth = game_create_panel_options_detail::kValueColumnMinW;
		}
		if(valueColumnWidth > static_cast<int>(game_create_panel_options_detail::kValueColumnMaxW)){
			valueColumnWidth = game_create_panel_options_detail::kValueColumnMaxW;
		}
		if(valueColumnWidth > formContentW){
			valueColumnWidth = formContentW;
		}
		out.valueColumnWidth = static_cast<Uint16>(valueColumnWidth);
	}
	return out;
}

GameCreateOptionsScrollbarLayout ResolveGameCreateOptionsScrollbarLayout(
	const GameCreateOptionsLayout & layout,
	const GameCreatePanelState & state) {
	GameCreateOptionsScrollbarLayout out;
	if(!layout.showScrollbar ||
	   layout.viewportHeight <= game_create_panel_options_detail::kScrollbarTrackPad * 2){
		return out;
	}
	const int trackHeight = static_cast<int>(layout.viewportHeight)
	                      - static_cast<int>(
							  game_create_panel_options_detail::kScrollbarTrackPad) * 2;
	if(trackHeight <= 0){
		return out;
	}

	int thumbHeight =
		(trackHeight * static_cast<int>(layout.visibleRows))
		/ game_create_panel_options_detail::kOptionRowCount;
	if(thumbHeight < static_cast<int>(game_create_panel_options_detail::kScrollbarThumbMinH)){
		thumbHeight = std::min(
			trackHeight,
			static_cast<int>(game_create_panel_options_detail::kScrollbarThumbMinH));
	}
	if(thumbHeight > trackHeight){
		thumbHeight = trackHeight;
	}

	const int travel = trackHeight - thumbHeight;
	int topSpacer = 0;
	if(layout.scrollMax > 0 && travel > 0){
		topSpacer =
			(travel * static_cast<int>(state.optionsScrollPosition)
			 + static_cast<int>(layout.scrollMax) / 2)
			/ static_cast<int>(layout.scrollMax);
	}
	if(topSpacer < 0) topSpacer = 0;
	if(topSpacer > travel) topSpacer = travel;

	out.topSpacer = static_cast<Uint16>(topSpacer);
	out.thumbHeight = static_cast<Uint16>(thumbHeight);
	out.bottomSpacer = static_cast<Uint16>(travel - topSpacer);
	return out;
}

void GameCreatePanelSyncOptionsLayout(GameCreatePanelState & state,
                                      Uint16 panelWidth,
                                      Uint16 panelHeight) {
	const GameCreateOptionsLayout layout =
		ResolveGameCreateOptionsLayout(panelWidth, panelHeight);
	game_create_panel_options_detail::ClampOptionsScroll(state, layout);
}

}  // namespace silencer::client_ui::lobby
