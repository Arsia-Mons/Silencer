#include "game_create_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/box.h"
#include "primitives/text.h"
#include "primitives/button.h"
#include "primitives/text_input.h"

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

constexpr int    kRowHeight = 14;

constexpr uint16_t kPanelPad      = 6;
constexpr uint16_t kFormPadLeft   = 4;
constexpr uint16_t kFormPadTop    = 2;
constexpr uint16_t kFormPadBottom = 2;
constexpr uint16_t kFormRowH      = 14;
constexpr uint16_t kFormRowGap    = 3;
constexpr uint16_t kFormColumnGap = 6;

constexpr const char * kActionSecurity    = "lobby.game_create.security";
constexpr const char * kActionSpectatable = "lobby.game_create.spectatable";
constexpr const char * kActionMinLevel    = "lobby.game_create.min_level";
constexpr const char * kActionMaxLevel    = "lobby.game_create.max_level";
constexpr const char * kActionMaxPlayers  = "lobby.game_create.max_players";
constexpr const char * kActionMaxTeams    = "lobby.game_create.max_teams";

Clay_String FromCStr(const char * s) {
	return Clay_String{ false, static_cast<int32_t>(strlen(s)), s };
}

Clay_String StaticId(const char * s) {
	return Clay_String{ true, static_cast<int32_t>(strlen(s)), s };
}

const char * SecurityLabel(Uint8 idx) {
	switch(idx){
		case 0:  return "Off";
		case 1:  return "Low";
		case 3:  return "High";
		default: return "Medium";
	}
}

void BuildOptionRow(GameCreatePanelState & state, int i,
                    const char * rowId, const char * label,
                    const char * id, const char * actionId,
                    char * buf, int cap,
                    silencer::ui::UiInteractionRegistry& interactions) {
	CLAY({ .id = CLAY_SID(StaticId(rowId)),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_FIXED(kFormRowH) },
	           .childGap = kFormColumnGap,
	           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		CLAY({ .id = CLAY_SIDI(CLAY_STRING("GCrtRowLabel"), (uint32_t)i),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(kFormRowH) },
		           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
		       } }) {
			Text(FromCStr(label), { .size = TextSize::Body });
		}
		if(i == 0){
			Button(CLAY_STRING("GameCreateSecurityButton"), FromCStr(SecurityLabel(state.securityIndex)),
			       ButtonOpts{ .variant = ButtonVariant::Text,
			                   .size = ButtonSize::Auto,
			                   .minWidth = 60,
			                   .paddingY = 2 },
			       ButtonHandle{ nullptr, kActionSecurity, &interactions });
		}else if(i == 5){
			Button(CLAY_STRING("GameCreateSpectatableButton"), FromCStr(state.spectatable ? "Yes" : "No"),
			       ButtonOpts{ .variant = ButtonVariant::Text,
			                   .size = ButtonSize::Auto,
			                   .selected = state.spectatable,
			                   .minWidth = 30,
			                   .paddingY = 2 },
			       ButtonHandle{ nullptr, kActionSpectatable, &interactions });
		}else{
			TextInputOpts opts;
			opts.widthPx     = 20;
			opts.heightPx    = kRowHeight;
			opts.textSize    = TextSize::Body;
			opts.numbersOnly = true;
			opts.showCaret   = false;
			std::string idStr = std::string("Input_") + id;
			TextInput(Clay_String{ false, (int32_t)idStr.size(), idStr.c_str() },
			          buf, opts,
			          TextInputHandle{ nullptr, actionId, label, &interactions,
			                           -1, cap > 0 ? cap - 1 : 0 });
		}
	}
}

}  // namespace game_create_panel_options_detail

void BuildGameCreateUpperTree(GameCreatePanelState & state,
                              Resources & resources,
                              silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;

	CLAY({ .id = CLAY_ID("GCrtOptionsContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { game_create_panel_options_detail::kPanelPad, game_create_panel_options_detail::kPanelPad, game_create_panel_options_detail::kPanelPad, game_create_panel_options_detail::kPanelPad },
	           .childGap = game_create_panel_options_detail::kFormRowGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtOptionsTitleWrap") }) {
			Text(CLAY_STRING("Game Options"),
			     { .size = TextSize::Heading });
		}

		CLAY(Box(BoxVariants::Inset, {
		         .id = CLAY_ID("GCrtOptionsFormBorder"),
		         .layout = {
		             .sizing = { CLAY_SIZING_GROW(0),
		                         CLAY_SIZING_FIT(0) },
		             .padding = { game_create_panel_options_detail::kFormPadLeft,
		                          game_create_panel_options_detail::kFormPadLeft,
		                          game_create_panel_options_detail::kFormPadTop,
		                          game_create_panel_options_detail::kFormPadBottom },
		             .childGap = game_create_panel_options_detail::kFormRowGap,
		             .layoutDirection = CLAY_TOP_TO_BOTTOM,
		         },
		     })) {
			struct LabelRow { const char * label; const char * id; };
			constexpr LabelRow kLabels[6] = {
				{ "Security:",    "GCrtRowSec"   },
				{ "Min Level:",   "GCrtRowMinL"  },
				{ "Max Level:",   "GCrtRowMaxL"  },
				{ "Max Players:", "GCrtRowMaxP"  },
				{ "Max Teams:",   "GCrtRowMaxT"  },
				{ "Spectatable:", "GCrtRowSpect" },
			};

			struct NumInput { const char * id; const char * label; const char * actionId; char * buf; int cap; };
			NumInput kTextInputs[4] = {
				{ "GCrtMinLevel",   "Min Level",   game_create_panel_options_detail::kActionMinLevel,   state.minLevel,   (int)sizeof(state.minLevel)   },
				{ "GCrtMaxLevel",   "Max Level",   game_create_panel_options_detail::kActionMaxLevel,   state.maxLevel,   (int)sizeof(state.maxLevel)   },
				{ "GCrtMaxPlayers", "Max Players", game_create_panel_options_detail::kActionMaxPlayers, state.maxPlayers, (int)sizeof(state.maxPlayers) },
				{ "GCrtMaxTeams",   "Max Teams",   game_create_panel_options_detail::kActionMaxTeams,   state.maxTeams,   (int)sizeof(state.maxTeams)   },
			};

			for(int i = 0; i < 6; ++i){
				const NumInput * ti = (i >= 1 && i <= 4) ? &kTextInputs[i - 1] : nullptr;
				game_create_panel_options_detail::BuildOptionRow(state, i,
				               kLabels[i].id, kLabels[i].label,
				               ti ? ti->id : "",
				               ti ? ti->actionId : "",
				               ti ? ti->buf : nullptr,
				               ti ? ti->cap : 0,
				               interactions);
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
