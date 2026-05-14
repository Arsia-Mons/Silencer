#include "game_create_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/text_input.h"

#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputOpts;

namespace silencer::client_ui::lobby {

namespace {

// Legacy on-screen coords kept ONLY for inspector hit-rect registration.
constexpr int    kYSpace    = 14;
constexpr int    kRowHeight = 14;
constexpr int    kValueX    = 323;
constexpr int    kFormTop   = 87;

constexpr uint16_t kPanelPad      = 6;
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

void RegisterButton(const char * label, const char * actionId,
                    int x, int y, int w, int h, bool selected = false) {
	silencer::ui::automation::Widget reg;
	reg.id        = actionId;
	reg.labelText = label;
	reg.kind      = silencer::ui::automation::WidgetKind::Button;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.selected  = selected;
	silencer::ui::automation::Register(reg);
}

void RegisterTextInput(const char * label, const char * actionId,
                       int x, int y, int w, int h,
                       char * buf, int cap) {
	silencer::ui::automation::Widget reg;
	reg.id            = actionId;
	reg.labelText     = label;
	reg.kind          = silencer::ui::automation::WidgetKind::TextInput;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.value         = buf ? buf : "";
	reg.maxLength     = cap > 0 ? cap - 1 : 0;
	silencer::ui::automation::Register(reg);
}

void BuildOptionRow(GameCreatePanelState & state, int i,
                    const char * rowId, const char * label,
                    const char * id, const char * actionId,
                    char * buf, int cap) {
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
			BankText(FromCStr(label), BankTextVariant::Body, {});
		}
		if(i == 0){
			BankButton(FromCStr(SecurityLabel(state.securityIndex)),
			           BankButtonVariant::Inline, {},
			           BankButtonHandle{ nullptr, kActionSecurity });
			RegisterButton("Security", kActionSecurity,
			               kValueX, kFormTop + 2, 60, kRowHeight);
		}else if(i == 5){
			BankButton(FromCStr(state.spectatable ? "Yes" : "No"),
			           BankButtonVariant::Inline, {},
			           BankButtonHandle{ nullptr, kActionSpectatable });
			RegisterButton("Spectatable", kActionSpectatable,
			               kValueX, kFormTop + 5*kYSpace + 2, 30, kRowHeight,
			               state.spectatable);
		}else{
			TextInputOpts opts;
			opts.widthPx     = 20;
			opts.heightPx    = kRowHeight;
			opts.fontBank    = 133;
			opts.fontWidth   = 6;
			opts.brightness  = 128;
			opts.numbersOnly = true;
			opts.showCaret   = false;
			std::string idStr = std::string("Input_") + id;
			TextInput(Clay_String{ false, (int32_t)idStr.size(), idStr.c_str() },
			          buf, opts, {});
			const int y = kFormTop + i * kYSpace + 2;
			RegisterTextInput(label, actionId, kValueX, y, 20, kRowHeight, buf, cap);
		}
	}
}

}  // namespace

void BuildGameCreateUpperTree(GameCreatePanelState & state,
                              Resources & resources) {
	(void)resources;

	CLAY({ .id = CLAY_ID("GCrtOptionsContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { kPanelPad, kPanelPad, kPanelPad, kPanelPad },
	           .childGap = kFormRowGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtOptionsTitleWrap") }) {
			BankText(CLAY_STRING("Game Options"),
			         BankTextVariant::Heading, {});
		}

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
			{ "GCrtMinLevel",   "Min Level",   kActionMinLevel,   state.minLevel,   (int)sizeof(state.minLevel)   },
			{ "GCrtMaxLevel",   "Max Level",   kActionMaxLevel,   state.maxLevel,   (int)sizeof(state.maxLevel)   },
			{ "GCrtMaxPlayers", "Max Players", kActionMaxPlayers, state.maxPlayers, (int)sizeof(state.maxPlayers) },
			{ "GCrtMaxTeams",   "Max Teams",   kActionMaxTeams,   state.maxTeams,   (int)sizeof(state.maxTeams)   },
		};

		for(int i = 0; i < 6; ++i){
			const NumInput * ti = (i >= 1 && i <= 4) ? &kTextInputs[i - 1] : nullptr;
			BuildOptionRow(state, i,
			               kLabels[i].id, kLabels[i].label,
			               ti ? ti->id : "",
			               ti ? ti->actionId : "",
			               ti ? ti->buf : nullptr,
			               ti ? ti->cap : 0);
		}
	}
}

}  // namespace silencer::client_ui::lobby
