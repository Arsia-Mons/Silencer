#include "options_controls_screen.h"

#include "controls_keybind_list.h"
#include "controls_rebind_capture.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "config.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace options_controls_screen_detail {

constexpr int REBIND_TIMEOUT_TICKS = 72;
constexpr uint16_t kPanelW = 540;
constexpr uint16_t kPanelPadX = 20;
constexpr uint16_t kPanelPadY = 24;
constexpr const char * kActionPreset = "options_controls.preset";
constexpr const char * kActionSave = "options_controls.save";
constexpr const char * kActionCancel = "options_controls.cancel";
constexpr const char * kActionScrollUp = "options_controls.scroll_up";
constexpr const char * kActionScrollDown = "options_controls.scroll_down";
constexpr const char * kActionPrimaryPrefix = "options_controls.primary.";
constexpr const char * kActionSecondaryPrefix = "options_controls.secondary.";
constexpr const char * kActionOperatorPrefix = "options_controls.operator.";

bool IsBuiltinKeybindProfile(const std::string & name) {
	return name == "default" || name == "wasd" || name == "gamepad";
}

bool StartsWith(const std::string & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

int SuffixInt(const std::string & value, const char * prefix) {
	if(!StartsWith(value, prefix)) return -1;
	return std::atoi(value.c_str() + std::strlen(prefix));
}

}  // namespace options_controls_screen_detail

int OptionsControlsScreen::MaxScroll() const {
	int max = (int)Action::Count - silencer::client_ui::options::kKeybindListVisibleRows;
	return max < 0 ? 0 : max;
}

void OptionsControlsScreen::Build(ScreenContext & ctx) {
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	scrollPosition = 0;
	rebindRow = -1;
	rebindSlot = -1;
	presetClicked = false;
	saveClicked = false;
	cancelClicked = false;
	scrollDelta = 0;
	operatorClickedRow = -1;
}

void OptionsControlsScreen::BeginRebindFromVisibleRow(int row, int slot) {
	int absolute = scrollPosition + row;
	if(absolute < 0 || absolute >= (int)Action::Count) return;
	rebindRow = absolute;
	rebindSlot = slot;
}

void OptionsControlsScreen::ToggleOperatorFromVisibleRow(int row) {
	int absolute = scrollPosition + row;
	if(absolute < 0 || absolute >= (int)Action::Count) return;
	operatorClickedRow = absolute;
}

void OptionsControlsScreen::Tick(ScreenContext & ctx) {
	using namespace silencer::client_ui::options;

	if(scrollDelta != 0){
		scrollPosition = std::max(0, std::min(MaxScroll(), scrollPosition + scrollDelta));
		scrollDelta = 0;
	}
	if(presetClicked){
		presetClicked = false;
		CycleKeybindPreset(ctx.keymap);
	}
	if(operatorClickedRow >= 0 && operatorClickedRow < (int)Action::Count){
		Action a = ACTION_TABLE[operatorClickedRow].action;
		LegacyBindingView v = ViewLegacy(ctx.keymap, a);
		v.and_ = !v.and_;
		ForkActiveProfileIfBuiltin(ctx.keymap);
		WriteLegacy(ctx.keymap, a, v.key1, v.key2, v.and_);
		operatorClickedRow = -1;
	}
	if(rebindRow >= 0){
		if(optionscontrolstick == 0){
			optionscontrolstick = ctx.world.tickcount;
		}
		if(rebindRow >= 0 &&
		   ctx.world.tickcount - optionscontrolstick > options_controls_screen_detail::REBIND_TIMEOUT_TICKS){
			FinishKeyboardRebind(ctx, rebindRow, rebindSlot, SDL_SCANCODE_UNKNOWN);
		}
	}else{
		optionscontrolstick = 0;
	}
	if(saveClicked){
		saveClicked = false;
		const std::string active = Config::GetInstance().active_keybind_profile;
		if(!options_controls_screen_detail::IsBuiltinKeybindProfile(active)){
			ctx.keymap.SaveFile(WritableProfilePath(active));
		}
		Config::GetInstance().Save();
		ctx.GoToState(GameState::OPTIONS);
		return;
	}
	if(cancelClicked){
		cancelClicked = false;
		LoadActiveKeymap(ctx.keymap);
		Config::GetInstance().Load();
		ctx.GoToState(GameState::OPTIONS);
	}
}

bool OptionsControlsScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) {
	using namespace silencer::client_ui::options;

	if(action.kind == silencer::ui::UiActionKind::CaptureBinding){
		if(rebindRow < 0) return false;
		FinishBindingRebind(ctx, rebindRow, rebindSlot, action.binding);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		cancelClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_controls_screen_detail::kActionPreset){
		presetClicked = true;
		return true;
	}
	if(action.id == options_controls_screen_detail::kActionSave){
		saveClicked = true;
		return true;
	}
	if(action.id == options_controls_screen_detail::kActionCancel){
		cancelClicked = true;
		return true;
	}
	if(action.id == options_controls_screen_detail::kActionScrollUp){
		scrollDelta--;
		return true;
	}
	if(action.id == options_controls_screen_detail::kActionScrollDown){
		scrollDelta++;
		return true;
	}
	int row = options_controls_screen_detail::SuffixInt(action.id, options_controls_screen_detail::kActionPrimaryPrefix);
	if(row >= 0){
		BeginRebindFromVisibleRow(row, 0);
		return true;
	}
	row = options_controls_screen_detail::SuffixInt(action.id, options_controls_screen_detail::kActionSecondaryPrefix);
	if(row >= 0){
		BeginRebindFromVisibleRow(row, 1);
		return true;
	}
	row = options_controls_screen_detail::SuffixInt(action.id, options_controls_screen_detail::kActionOperatorPrefix);
	if(row >= 0){
		ToggleOperatorFromVisibleRow(row);
		return true;
	}
	return false;
}

void OptionsControlsScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) {
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;
	using namespace silencer::client_ui::options;

	KeybindListView view;
	view.presetText = !ctx.keymap.label.empty() ? ctx.keymap.label
	                : !ctx.keymap.name.empty() ? ctx.keymap.name
	                : std::string(Config::GetInstance().active_keybind_profile);
	for(int i = 0; i < kKeybindListVisibleRows; i++){
		int row = scrollPosition + i;
		if(row >= (int)Action::Count) break;
		Action action = ACTION_TABLE[row].action;
		LegacyBindingView v = ViewLegacy(ctx.keymap, action);
		KeybindRowView & out = view.rows[i];
		out.actionLabel = std::string(GetActionInfo(action).label) + ":";
		out.primaryLabel = GetBindingLabel(ctx, action, 0);
		out.secondaryLabel = GetBindingLabel(ctx, action, 1);
		out.operatorLabel = v.and_ ? "AND" : "OR";
		out.rebindingPrimary = (rebindRow == row && rebindSlot == 0);
		out.rebindingSecondary = (rebindRow == row && rebindSlot == 1);
		view.visibleRowCount = i + 1;
	}

	CLAY({ .id = CLAY_ID("OptionsControlsRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { 0, 0, 34, 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsControlsPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(options_controls_screen_detail::kPanelW),
		                       CLAY_SIZING_FIT(0) },
		           .padding = { options_controls_screen_detail::kPanelPadX, options_controls_screen_detail::kPanelPadX,
		                        options_controls_screen_detail::kPanelPadY, options_controls_screen_detail::kPanelPadY },
		           .childGap = 12,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(7, 7) } }) {
			BuildKeybindListBody(view, interactions);
		}
	}
}

void OptionsControlsScreen::Destroy(ScreenContext & ctx) {
	(void)ctx;
}
