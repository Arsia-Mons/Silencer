#include "tech_selected_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "primitives/bank_text.h"

#include "game_tech_panel.h"

#include <cstdint>
#include <string>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;

namespace silencer::client_ui::lobby {

namespace {

constexpr int kDescLH = 10;
constexpr uint16_t kTallTechNamePadTop = 16;
constexpr uint16_t kTallDescPadLeft    = 7;
constexpr uint16_t kTallDescPadTop     = 5;
constexpr uint16_t kTallDescRowGap     = 0;

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

}  // namespace

void BuildTechSelectedPanel(const GameTechPanelState & state) {
	// Centered tech-name heading. ALIGN_X_CENTER in a grow wrapper sized to
	// the legacy 232-wide tall pane.
	CLAY({ .id = CLAY_ID("GTechNameWrap"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_FIXED(15) },
	           .padding = { 0, 0, kTallTechNamePadTop, 0 },
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
	       } }) {
		if(!state.techNameStr.empty()){
			BankText(FromStd(state.techNameStr),
			         BankTextVariant::Heading, {});
		}
	}

	// 8 description lines.
	CLAY({ .id = CLAY_ID("GTechDescWrap"),
	       .layout = {
	           .padding = { kTallDescPadLeft, 0,
	                        kTallDescPadTop,  0 },
	           .childGap = kTallDescRowGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		for(int i = 0; i < 8; ++i){
			CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechDescLine"),
			                       static_cast<uint32_t>(i)),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(kDescLH) },
			       } }) {
				if(!state.techDescLines[i].empty()){
					BankText(FromStd(state.techDescLines[i]),
					         BankTextVariant::Body,
					         { .effectColor = 129,
					           .brightness  = static_cast<Uint8>(128 + 16),
					           .colorRamp   = true });
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
