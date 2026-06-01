#include "game_create_panel.h"

#include "clay/clay.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/box.h"
#include "primitives/text.h"
#include "primitives/button.h"
#include "primitives/text_input.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Box;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

namespace silencer::client_ui::lobby {

namespace game_create_panel_options_detail {

constexpr int kOptionRowCount = 6;

constexpr uint16_t kPanelPad         = 6;
constexpr uint16_t kFormPadLeft      = 4;
constexpr uint16_t kFormPadTop       = 2;
constexpr uint16_t kFormPadBottom    = 2;
constexpr uint16_t kFormRowH         = 14;
constexpr uint16_t kFormRowGap       = 3;
constexpr uint16_t kFormColumnGap    = 6;
constexpr uint16_t kScrollbarWidth   = 8;
constexpr uint16_t kScrollbarGap     = 2;
constexpr uint16_t kValueColumnMinW  = 30;
constexpr uint16_t kValueColumnMaxW  = 72;
constexpr uint16_t kScrollbarTrackPad = 1;
constexpr uint16_t kScrollbarThumbMinH = 12;

constexpr const char * kActionSecurity    = "lobby.game_create.security";
constexpr const char * kActionSpectatable = "lobby.game_create.spectatable";
constexpr const char * kActionMinLevel    = "lobby.game_create.min_level";
constexpr const char * kActionMaxLevel    = "lobby.game_create.max_level";
constexpr const char * kActionMaxPlayers  = "lobby.game_create.max_players";
constexpr const char * kActionMaxTeams    = "lobby.game_create.max_teams";

struct UpperLayout {
	Uint16 titleHeight = 0;
	Uint16 viewportHeight = 0;
	Uint16 valueColumnWidth = 0;
	Uint16 scrollMax = 0;
	Uint8 visibleRows = 0;
	bool showScrollbar = false;
};

struct ScrollbarLayout {
	Uint16 topSpacer = 0;
	Uint16 thumbHeight = 0;
	Uint16 bottomSpacer = 0;
};

Clay_String FromCStr(const char * s) {
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

Clay_String StaticId(const char * s) {
	return Clay_String{ true, static_cast<int32_t>(std::strlen(s)), s };
}

const char * SecurityLabel(Uint8 idx) {
	switch(idx){
		case 0:  return "Off";
		case 1:  return "Low";
		case 3:  return "High";
		default: return "Medium";
	}
}

void RegisterOptionsScrollArea(Clay_ElementId clayId,
                               silencer::ui::UiInteractionRegistry& interactions) {
	silencer::ui::UiElementSnapshot element;
	element.id = kGameCreateOptionsScrollId;
	element.kind = silencer::ui::UiElementKind::Container;
	element.label = kGameCreateOptionsScrollLabel;
	element.value = "scroll";
	element.clayId = clayId;
	element.hasClayId = true;
	interactions.Register(element);
}

UpperLayout ResolveUpperLayout(Uint16 panelWidth,
                               Uint16 panelHeight) {
	UpperLayout out;
	const int contentW = std::max(
		0,
		static_cast<int>(panelWidth) - static_cast<int>(kPanelPad) * 2);
	const int contentH = std::max(
		0,
		static_cast<int>(panelHeight) - static_cast<int>(kPanelPad) * 2);
	out.titleHeight = silencer::ui::primitives::TextLineHeight(TextSize::Heading);

	const int borderH = std::max(
		0,
		contentH - static_cast<int>(out.titleHeight) - static_cast<int>(kFormRowGap));
	const int viewportH = std::max(
		0,
		borderH - static_cast<int>(kFormPadTop) - static_cast<int>(kFormPadBottom));
	out.viewportHeight = static_cast<Uint16>(viewportH);

	if(viewportH > 0){
		int visibleRows = (viewportH + static_cast<int>(kFormRowGap))
		                / (static_cast<int>(kFormRowH) + static_cast<int>(kFormRowGap));
		if(visibleRows < 1) visibleRows = 1;
		if(visibleRows > kOptionRowCount) visibleRows = kOptionRowCount;
		out.visibleRows = static_cast<Uint8>(visibleRows);
	}

	if(out.visibleRows > 0 && out.visibleRows < kOptionRowCount){
		out.scrollMax = static_cast<Uint16>(kOptionRowCount - out.visibleRows);
	}
	out.showScrollbar = out.scrollMax > 0;

	int valueColumnWidth = 0;
	int formContentW = contentW;
	if(out.showScrollbar){
		formContentW -= static_cast<int>(kScrollbarWidth) + static_cast<int>(kScrollbarGap);
		if(formContentW < 0) formContentW = 0;
	}
	if(formContentW > 0){
		valueColumnWidth = formContentW / 3;
		if(valueColumnWidth < static_cast<int>(kValueColumnMinW)){
			valueColumnWidth = kValueColumnMinW;
		}
		if(valueColumnWidth > static_cast<int>(kValueColumnMaxW)){
			valueColumnWidth = kValueColumnMaxW;
		}
		if(valueColumnWidth > formContentW){
			valueColumnWidth = formContentW;
		}
	}
	out.valueColumnWidth = static_cast<Uint16>(valueColumnWidth);
	return out;
}

void ClampOptionsScroll(GameCreatePanelState & state,
                        const UpperLayout & layout) {
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

ScrollbarLayout ResolveScrollbarLayout(const UpperLayout & layout,
                                       const GameCreatePanelState & state) {
	ScrollbarLayout out;
	if(!layout.showScrollbar || layout.viewportHeight <= kScrollbarTrackPad * 2){
		return out;
	}
	const int trackHeight = static_cast<int>(layout.viewportHeight)
	                      - static_cast<int>(kScrollbarTrackPad) * 2;
	if(trackHeight <= 0){
		return out;
	}

	int thumbHeight = (trackHeight * static_cast<int>(layout.visibleRows))
	                / kOptionRowCount;
	if(thumbHeight < static_cast<int>(kScrollbarThumbMinH)){
		thumbHeight = std::min(trackHeight, static_cast<int>(kScrollbarThumbMinH));
	}
	if(thumbHeight > trackHeight){
		thumbHeight = trackHeight;
	}

	const int travel = trackHeight - thumbHeight;
	int topSpacer = 0;
	if(layout.scrollMax > 0 && travel > 0){
		topSpacer = (travel * static_cast<int>(state.optionsScrollPosition)
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

void BuildOptionRow(GameCreatePanelState & state,
                    int i,
                    const char * rowId,
                    const char * label,
                    const char * id,
                    const char * actionId,
                    char * buf,
                    int cap,
                    Uint16 valueColumnWidth,
                    silencer::ui::UiInteractionRegistry& interactions) {
	CLAY({ .id = CLAY_SID(StaticId(rowId)),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_FIXED(kFormRowH) },
	           .childGap = kFormColumnGap,
	           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		CLAY({ .id = CLAY_SIDI(CLAY_STRING("GCrtRowLabel"), static_cast<uint32_t>(i)),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(kFormRowH) },
		           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
		       } }) {
			Text(FromCStr(label), { .size = TextSize::Body });
		}

		CLAY({ .id = CLAY_SIDI(CLAY_STRING("GCrtRowValue"), static_cast<uint32_t>(i)),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(valueColumnWidth)),
		                       CLAY_SIZING_FIXED(kFormRowH) },
		           .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
		                               .y = CLAY_ALIGN_Y_CENTER },
		       } }) {
			if(i == 0){
				Button(CLAY_STRING("GameCreateSecurityButton"),
				       FromCStr(SecurityLabel(state.securityIndex)),
				       ButtonOpts{ .variant = ButtonVariant::Text,
				                   .size = ButtonSize::Auto,
				                   .alignLeft = true,
				                   .minWidth = valueColumnWidth,
				                   .paddingY = 2 },
				       ButtonHandle{ nullptr, kActionSecurity, &interactions });
			}else if(i == 5){
				Button(CLAY_STRING("GameCreateSpectatableButton"),
				       FromCStr(state.spectatable ? "Yes" : "No"),
				       ButtonOpts{ .variant = ButtonVariant::Text,
				                   .size = ButtonSize::Auto,
				                   .selected = state.spectatable,
				                   .alignLeft = true,
				                   .minWidth = valueColumnWidth,
				                   .paddingY = 2 },
				       ButtonHandle{ nullptr, kActionSpectatable, &interactions });
			}else{
				TextInputOpts opts;
				opts.widthPx = static_cast<Uint16>(std::min<int>(
					std::max<int>(20, valueColumnWidth), 28));
				opts.heightPx = kFormRowH;
				opts.textSize = TextSize::Body;
				opts.numbersOnly = true;
				opts.showCaret = false;
				std::string idStr = std::string("Input_") + id;
				TextInput(Clay_String{ false, static_cast<int32_t>(idStr.size()), idStr.c_str() },
				          buf, opts,
				          TextInputHandle{ nullptr, actionId, label, &interactions,
				                           -1, cap > 0 ? cap - 1 : 0 });
			}
		}
	}
}

}  // namespace game_create_panel_options_detail

void BuildGameCreateUpperTree(GameCreatePanelState & state,
                              Uint16 panelWidth,
                              Uint16 panelHeight,
                              silencer::ui::UiInteractionRegistry& interactions) {
	const auto layout = game_create_panel_options_detail::ResolveUpperLayout(
		panelWidth, panelHeight);
	game_create_panel_options_detail::ClampOptionsScroll(state, layout);

	CLAY({ .id = CLAY_ID("GCrtOptionsContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = {
	               game_create_panel_options_detail::kPanelPad,
	               game_create_panel_options_detail::kPanelPad,
	               game_create_panel_options_detail::kPanelPad,
	               game_create_panel_options_detail::kPanelPad,
	           },
	           .childGap = game_create_panel_options_detail::kFormRowGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtOptionsTitleWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(static_cast<float>(layout.titleHeight)) },
		       } }) {
			Text(CLAY_STRING("Game Options"),
			     { .size = TextSize::Heading });
		}

		CLAY(Box(BoxVariants::Inset, {
		         .id = CLAY_ID("GCrtOptionsFormBorder"),
		         .layout = {
		             .sizing = { CLAY_SIZING_GROW(0),
		                         CLAY_SIZING_GROW(0) },
		             .padding = {
		                 game_create_panel_options_detail::kFormPadLeft,
		                 game_create_panel_options_detail::kFormPadLeft,
		                 game_create_panel_options_detail::kFormPadTop,
		                 game_create_panel_options_detail::kFormPadBottom,
		             },
		             .childGap = game_create_panel_options_detail::kScrollbarGap,
		             .layoutDirection = CLAY_LEFT_TO_RIGHT,
		         },
		     })) {
			const Clay_ElementId viewportId = CLAY_ID("GCrtOptionsViewport");
			CLAY({ .id = viewportId,
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(static_cast<float>(layout.viewportHeight)) },
			           .childGap = game_create_panel_options_detail::kFormRowGap,
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       },
			       .clip = { .vertical = true } }) {
				game_create_panel_options_detail::RegisterOptionsScrollArea(
					viewportId, interactions);

				struct LabelRow {
					const char * label;
					const char * id;
				};
				constexpr LabelRow kLabels[game_create_panel_options_detail::kOptionRowCount] = {
					{ "Security:",    "GCrtRowSec"   },
					{ "Min Level:",   "GCrtRowMinL"  },
					{ "Max Level:",   "GCrtRowMaxL"  },
					{ "Max Players:", "GCrtRowMaxP"  },
					{ "Max Teams:",   "GCrtRowMaxT"  },
					{ "Spectatable:", "GCrtRowSpect" },
				};

				struct NumInput {
					const char * id;
					const char * label;
					const char * actionId;
					char * buf;
					int cap;
				};
				NumInput kTextInputs[4] = {
					{ "GCrtMinLevel",   "Min Level",   game_create_panel_options_detail::kActionMinLevel,   state.minLevel,   static_cast<int>(sizeof(state.minLevel))   },
					{ "GCrtMaxLevel",   "Max Level",   game_create_panel_options_detail::kActionMaxLevel,   state.maxLevel,   static_cast<int>(sizeof(state.maxLevel))   },
					{ "GCrtMaxPlayers", "Max Players", game_create_panel_options_detail::kActionMaxPlayers, state.maxPlayers, static_cast<int>(sizeof(state.maxPlayers)) },
					{ "GCrtMaxTeams",   "Max Teams",   game_create_panel_options_detail::kActionMaxTeams,   state.maxTeams,   static_cast<int>(sizeof(state.maxTeams))   },
				};

				const int firstRow = static_cast<int>(state.optionsScrollPosition);
				const int lastRow = std::min(
					game_create_panel_options_detail::kOptionRowCount,
					firstRow + static_cast<int>(state.optionsVisibleRows));
				for(int i = firstRow; i < lastRow; ++i){
					const NumInput * ti = (i >= 1 && i <= 4) ? &kTextInputs[i - 1] : nullptr;
					game_create_panel_options_detail::BuildOptionRow(
						state,
						i,
						kLabels[i].id,
						kLabels[i].label,
						ti ? ti->id : "",
						ti ? ti->actionId : "",
						ti ? ti->buf : nullptr,
						ti ? ti->cap : 0,
						layout.valueColumnWidth,
						interactions);
				}
			}

			if(layout.showScrollbar){
				const auto scrollbarLayout =
					game_create_panel_options_detail::ResolveScrollbarLayout(layout, state);
				CLAY(Box(BoxVariants::Inset, {
				         .id = CLAY_ID("GCrtOptionsScrollbarTrack"),
				         .layout = {
				             .sizing = {
				                 CLAY_SIZING_FIXED(
				                     static_cast<float>(
				                         game_create_panel_options_detail::kScrollbarWidth)),
				                 CLAY_SIZING_FIXED(static_cast<float>(layout.viewportHeight)),
				             },
				             .padding = {
				                 game_create_panel_options_detail::kScrollbarTrackPad,
				                 game_create_panel_options_detail::kScrollbarTrackPad,
				                 game_create_panel_options_detail::kScrollbarTrackPad,
				                 game_create_panel_options_detail::kScrollbarTrackPad,
				             },
				             .layoutDirection = CLAY_TOP_TO_BOTTOM,
				         },
				     })) {
					if(scrollbarLayout.topSpacer > 0){
						CLAY({ .id = CLAY_ID("GCrtOptionsScrollbarTopSpacer"),
						       .layout = {
						           .sizing = {
						               CLAY_SIZING_GROW(0),
						               CLAY_SIZING_FIXED(
						                   static_cast<float>(scrollbarLayout.topSpacer)),
						           },
						       } }) {}
					}

					CLAY(Box(BoxVariants::Plain, {
					         .id = CLAY_ID("GCrtOptionsScrollbarThumb"),
					         .layout = {
					             .sizing = {
					                 CLAY_SIZING_GROW(0),
					                 CLAY_SIZING_FIXED(
					                     static_cast<float>(scrollbarLayout.thumbHeight)),
					             },
					         },
					         .backgroundColor = { 216, 0, 0, 255 },
					     })) {}

					if(scrollbarLayout.bottomSpacer > 0){
						CLAY({ .id = CLAY_ID("GCrtOptionsScrollbarBottomSpacer"),
						       .layout = {
						           .sizing = {
						               CLAY_SIZING_GROW(0),
						               CLAY_SIZING_FIXED(
						                   static_cast<float>(scrollbarLayout.bottomSpacer)),
						           },
						       } }) {}
					}
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
