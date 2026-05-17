#include "game_create_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/button.h"
#include "primitives/scroll_list.h"
#include "primitives/text_input.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;

namespace silencer::client_ui::lobby {

namespace game_create_panel_map_form_detail {

// Legacy on-screen coords kept ONLY for inspector hit-rect registration.
constexpr int    kMapListX     = 407;
constexpr int    kMapListY     = 89;
constexpr Uint16 kMapListW     = 214;
constexpr Uint16 kMapListH     = 265;
constexpr Uint8  kMapListLineH = 14;
constexpr Uint8  kScrollbarBank = 7;
constexpr Uint16 kNameInputW = 210, kNameInputH = 14;
constexpr Uint16 kPwInputW   = 210, kPwInputH   = 14;

constexpr uint16_t kPanelPad       = 6;
constexpr uint16_t kTallSectionGap = 4;

constexpr int kMaxMapRows = 1024;
Clay_String g_mapSlab[kMaxMapRows];
constexpr const char * kActionCreate    = "lobby.game_create.create";
constexpr const char * kActionMapPrefix = "lobby.game_create.map";
constexpr const char * kActionName      = "lobby.game_create.name";
constexpr const char * kActionPassword  = "lobby.game_create.password";

void BuildMapList(GameCreatePanelState & state,
                  silencer::ui::UiInteractionRegistry& interactions) {
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
	listOpts.text.size      = TextSize::Body;
	listOpts.scrollbarBank  = kScrollbarBank;
	CLAY({ .id = CLAY_ID("GCrtMapListWrap") }) {
		ScrollList(CLAY_STRING("GCrtMapList"),
		           g_mapSlab, slotCount,
		           state.mapSelectedIndex, state.mapScrollPos,
		           listOpts,
		           ScrollListHandle{ nullptr, kActionMapPrefix, &interactions });
	}
	for(int i = 0; i < slotCount; ++i){
		silencer::ui::UiInteractable reg;
		reg.id         = std::string(kActionMapPrefix) + "." + std::to_string(i);
		reg.labelText  = g_mapSlab[i].chars ? g_mapSlab[i].chars : "";
		reg.kind       = silencer::ui::UiInteractableKind::ListRow;
		reg.x = kMapListX; reg.y = kMapListY + i * kMapListLineH;
		reg.w = kMapListW; reg.h = kMapListLineH;
		reg.index      = i;
		reg.selected   = state.mapSelectedIndex == i;
		interactions.RegisterInteractable(reg);
	}
}

void BuildNameAndPassword(GameCreatePanelState & state,
                          silencer::ui::UiInteractionRegistry& interactions) {
	CLAY({ .id = CLAY_ID("GCrtNameLabelWrap") }) {
		Text(CLAY_STRING("Game name:"),
		     { .size = TextSize::Heading });
	}
	TextInputOpts bodyInput;
	bodyInput.widthPx    = kNameInputW;
	bodyInput.heightPx   = kNameInputH;
	bodyInput.textSize   = TextSize::Body;
	bodyInput.showCaret  = false;
	CLAY({ .id = CLAY_ID("GCrtNameInputWrap") }) {
		TextInput(CLAY_STRING("GCrtNameInput"),
		          state.name, bodyInput,
		          TextInputHandle{ nullptr, kActionName, "Game name",
		                           &interactions, -1,
		                           static_cast<int>(sizeof(state.name)) - 1 });
	}

	CLAY({ .id = CLAY_ID("GCrtPwLabelWrap") }) {
		Text(CLAY_STRING("Password (optional):"),
		     { .size = TextSize::Heading });
	}
	bodyInput.password = true;
	CLAY({ .id = CLAY_ID("GCrtPwInputWrap") }) {
		TextInput(CLAY_STRING("GCrtPwInput"),
		          state.password, bodyInput,
		          TextInputHandle{ nullptr, kActionPassword, "Password",
		                           &interactions, -1,
		                           static_cast<int>(sizeof(state.password)) - 1 });
	}
}

}  // namespace game_create_panel_map_form_detail

void BuildGameCreateTallTree(GameCreatePanelState & state,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;

	CLAY({ .id = CLAY_ID("GCrtTallContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { game_create_panel_map_form_detail::kPanelPad, game_create_panel_map_form_detail::kPanelPad, game_create_panel_map_form_detail::kPanelPad, game_create_panel_map_form_detail::kPanelPad },
	           .childGap = game_create_panel_map_form_detail::kTallSectionGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtSelectMapTitleWrap") }) {
			Text(CLAY_STRING("Select Map"),
			     { .size = TextSize::Heading });
		}

		game_create_panel_map_form_detail::BuildMapList(state, interactions);
		game_create_panel_map_form_detail::BuildNameAndPassword(state, interactions);

		CLAY({ .id = CLAY_ID("GCrtCreateBtnWrap"),
		       .layout = { .childAlignment = { .x = CLAY_ALIGN_X_CENTER } } }) {
			Button(CLAY_STRING("GameCreateCreateButton"), CLAY_STRING("Create"),
			       ButtonOpts{ .variant = ButtonVariant::Chrome,
			                   .size = ButtonSize::Compact },
			       ButtonHandle{ nullptr, game_create_panel_map_form_detail::kActionCreate, &interactions });
		}
	}
}

}  // namespace silencer::client_ui::lobby
