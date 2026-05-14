#include "game_create_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/scroll_list.h"
#include "primitives/text_input.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputOpts;

namespace silencer::client_ui::lobby {

namespace {

// Legacy on-screen coords kept ONLY for inspector hit-rect registration.
constexpr int    kMapListX     = 407;
constexpr int    kMapListY     = 89;
constexpr Uint16 kMapListW     = 214;
constexpr Uint16 kMapListH     = 265;
constexpr Uint8  kMapListLineH = 14;
constexpr Uint8  kScrollbarBank = 7;
constexpr int    kNameInputX = 410, kNameInputY = 375;
constexpr Uint16 kNameInputW = 210, kNameInputH = 14;
constexpr int    kPwInputX   = 410, kPwInputY   = 405;
constexpr Uint16 kPwInputW   = 210, kPwInputH   = 14;
constexpr int    kCreateBtnX = 436, kCreateBtnY = 430;

constexpr uint16_t kPanelPad       = 6;
constexpr uint16_t kTallSectionGap = 4;

constexpr int kMaxMapRows = 1024;
Clay_String g_mapSlab[kMaxMapRows];
constexpr const char * kActionCreate    = "lobby.game_create.create";
constexpr const char * kActionMapPrefix = "lobby.game_create.map";
constexpr const char * kActionName      = "lobby.game_create.name";
constexpr const char * kActionPassword  = "lobby.game_create.password";

void RegisterButton(const char * label, const char * actionId,
                    int x, int y, int w, int h) {
	silencer::ui::automation::Widget reg;
	reg.id        = actionId;
	reg.labelText = label;
	reg.kind      = silencer::ui::automation::WidgetKind::Button;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	silencer::ui::automation::Register(reg);
}

void RegisterTextInput(const char * label, const char * actionId,
                       int x, int y, int w, int h,
                       char * buf, int cap, bool isPassword = false) {
	silencer::ui::automation::Widget reg;
	reg.id            = actionId;
	reg.labelText     = label;
	reg.kind          = silencer::ui::automation::WidgetKind::TextInput;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.value         = buf ? buf : "";
	reg.maxLength     = cap > 0 ? cap - 1 : 0;
	reg.isPassword    = isPassword;
	silencer::ui::automation::Register(reg);
}

void BuildMapList(GameCreatePanelState & state) {
	const int slotCount = std::min((int)state.maps.size(), kMaxMapRows);
	for(int i = 0; i < slotCount; ++i){
		const std::string & raw = state.maps[i];
		const char * txt = raw.c_str();
		size_t len = raw.size();
		if(len >= 5 && std::memcmp(txt, "[DL] ", 5) == 0){ txt += 5; len -= 5; }
		g_mapSlab[i] = Clay_String{ false, (int32_t)len, txt };
	}
	ScrollListOpts listOpts;
	listOpts.width          = kMapListW;
	listOpts.height         = kMapListH;
	listOpts.lineHeight     = kMapListLineH;
	listOpts.highlightColor = 180;
	listOpts.textVariant    = BankTextVariant::Body;
	listOpts.scrollbarBank  = kScrollbarBank;
	CLAY({ .id = CLAY_ID("GCrtMapListWrap") }) {
		ScrollList(CLAY_STRING("GCrtMapList"),
		           g_mapSlab, slotCount,
		           state.mapSelectedIndex, state.mapScrollPos,
		           listOpts,
		           ScrollListHandle{ nullptr, kActionMapPrefix });
	}
	for(int i = 0; i < slotCount; ++i){
		silencer::ui::automation::Widget reg;
		reg.id         = std::string(kActionMapPrefix) + "." + std::to_string(i);
		reg.labelText  = g_mapSlab[i].chars ? g_mapSlab[i].chars : "";
		reg.kind       = silencer::ui::automation::WidgetKind::ListRow;
		reg.x = kMapListX; reg.y = kMapListY + i * kMapListLineH;
		reg.w = kMapListW; reg.h = kMapListLineH;
		reg.index      = i;
		reg.selected   = state.mapSelectedIndex == i;
		silencer::ui::automation::Register(reg);
	}
}

void BuildNameAndPassword(GameCreatePanelState & state) {
	CLAY({ .id = CLAY_ID("GCrtNameLabelWrap") }) {
		BankText(CLAY_STRING("Game name:"),
		         BankTextVariant::Heading, {});
	}
	TextInputOpts bodyInput;
	bodyInput.widthPx    = kNameInputW;
	bodyInput.heightPx   = kNameInputH;
	bodyInput.fontBank   = 133;
	bodyInput.fontWidth  = 6;
	bodyInput.brightness = 128;
	bodyInput.showCaret  = false;
	CLAY({ .id = CLAY_ID("GCrtNameInputWrap") }) {
		TextInput(CLAY_STRING("GCrtNameInput"),
		          state.name, bodyInput, {});
	}
	RegisterTextInput("Game name", kActionName,
	                  kNameInputX, kNameInputY, kNameInputW, kNameInputH,
	                  state.name, (int)sizeof(state.name));

	CLAY({ .id = CLAY_ID("GCrtPwLabelWrap") }) {
		BankText(CLAY_STRING("Password (optional):"),
		         BankTextVariant::Heading, {});
	}
	bodyInput.password = true;
	CLAY({ .id = CLAY_ID("GCrtPwInputWrap") }) {
		TextInput(CLAY_STRING("GCrtPwInput"),
		          state.password, bodyInput, {});
	}
	RegisterTextInput("Password", kActionPassword,
	                  kPwInputX, kPwInputY, kPwInputW, kPwInputH,
	                  state.password, (int)sizeof(state.password), true);
}

}  // namespace

void BuildGameCreateTallTree(GameCreatePanelState & state,
                             Resources & resources) {
	(void)resources;

	CLAY({ .id = CLAY_ID("GCrtTallContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { kPanelPad, kPanelPad, kPanelPad, kPanelPad },
	           .childGap = kTallSectionGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtSelectMapTitleWrap") }) {
			BankText(CLAY_STRING("Select Map"),
			         BankTextVariant::Heading, {});
		}

		BuildMapList(state);
		BuildNameAndPassword(state);

		CLAY({ .id = CLAY_ID("GCrtCreateBtnWrap"),
		       .layout = { .childAlignment = { .x = CLAY_ALIGN_X_CENTER } } }) {
			BankButton(CLAY_STRING("Create"),
			           BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, kActionCreate });
		}
		RegisterButton("Create", kActionCreate, kCreateBtnX, kCreateBtnY, 156, 21);
	}
}

}  // namespace silencer::client_ui::lobby
