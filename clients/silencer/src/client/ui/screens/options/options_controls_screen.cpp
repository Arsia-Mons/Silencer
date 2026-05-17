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
constexpr int kLegacyViewportW = 640;
constexpr int kLegacyViewportH = 480;
constexpr int kFrameMarginLeft = 5;
constexpr int kFrameMarginRight = 7;
constexpr int kFrameMarginTop = 6;
constexpr int kFrameMarginBottom = 20;
constexpr int kTitleTextY = 14;
constexpr int kActionRowH = 33;
constexpr int kActionTopY = 405;
constexpr uint16_t kPanelMinW = 560;
constexpr uint16_t kPanelMinH = 420;
constexpr uint16_t kPanelPadX = 48;
constexpr uint16_t kPanelPadTop = 70;
constexpr const char * kActionPreset = "options_controls.preset";
constexpr const char * kActionSave = "options_controls.save";
constexpr const char * kActionCancel = "options_controls.cancel";
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

int ScaleLegacyPx(int value, int current, int legacy) {
	return std::max(0, (value * current + legacy / 2) / legacy);
}

int ScaleLegacyButtonTop(int top, int height, int current, int legacy) {
	const int centerTwice = top * 2 + height;
	return std::max(0, (centerTwice * current - height * legacy + legacy) / (legacy * 2));
}

}  // namespace options_controls_screen_detail

int OptionsControlsScreen::MaxScroll() const {
	int max = (int)Action::Count - visibleRowCapacity_;
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
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		if(action.id.empty() || action.id == kKeybindListScrollId){
			int amount = action.amount;
			if(action.value == "wheel") amount = -amount;
			scrollDelta += amount;
			return true;
		}
		return false;
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
	using namespace silencer::clay_bridge;
	using namespace silencer::client_ui::options;

	const int uiScale = std::max(1, UiScale());
	const int layoutWidth = dst.w / uiScale;
	const int layoutHeight = dst.h / uiScale;
	const int framePadLeft = options_controls_screen_detail::ScaleLegacyPx(
		options_controls_screen_detail::kFrameMarginLeft,
		layoutWidth,
		options_controls_screen_detail::kLegacyViewportW);
	const int framePadRight = options_controls_screen_detail::ScaleLegacyPx(
		options_controls_screen_detail::kFrameMarginRight,
		layoutWidth,
		options_controls_screen_detail::kLegacyViewportW);
	const int framePadTop = options_controls_screen_detail::ScaleLegacyPx(
		options_controls_screen_detail::kFrameMarginTop,
		layoutHeight,
		options_controls_screen_detail::kLegacyViewportH);
	const int framePadBottom = options_controls_screen_detail::ScaleLegacyPx(
		options_controls_screen_detail::kFrameMarginBottom,
		layoutHeight,
		options_controls_screen_detail::kLegacyViewportH);
	const int panelHeight = std::max<int>(
		options_controls_screen_detail::kPanelMinH,
		layoutHeight - framePadTop - framePadBottom);
	const int panelBottom = framePadTop + panelHeight;
	const int actionTop = options_controls_screen_detail::ScaleLegacyButtonTop(
		options_controls_screen_detail::kActionTopY,
		options_controls_screen_detail::kActionRowH,
		layoutHeight,
		options_controls_screen_detail::kLegacyViewportH);
	const int panelPadBottom = std::max(
		0,
		panelBottom - actionTop - options_controls_screen_detail::kActionRowH);
	const int contentHeight = panelHeight
	                        - options_controls_screen_detail::kPanelPadTop
	                        - panelPadBottom;
	visibleRowCapacity_ = std::min<int>(
		(int)Action::Count,
		KeybindListVisibleRowsForContentHeight(contentHeight));
	scrollPosition = std::max(0, std::min(MaxScroll(), scrollPosition));

	keybindListView_ = KeybindListView{};
	keybindListView_.presetText = !ctx.keymap.label.empty() ? ctx.keymap.label
	                            : !ctx.keymap.name.empty() ? ctx.keymap.name
	                            : std::string(Config::GetInstance().active_keybind_profile);
	keybindListView_.titleOffsetY = static_cast<float>(
		std::max(0,
		         options_controls_screen_detail::ScaleLegacyPx(
		             options_controls_screen_detail::kTitleTextY,
		             layoutHeight,
		             options_controls_screen_detail::kLegacyViewportH)
		         - framePadTop));
	keybindListView_.rows.resize(static_cast<size_t>(visibleRowCapacity_));
	for(int i = 0; i < visibleRowCapacity_; i++){
		int row = scrollPosition + i;
		if(row >= (int)Action::Count) break;
		Action action = ACTION_TABLE[row].action;
		LegacyBindingView v = ViewLegacy(ctx.keymap, action);
		KeybindRowView & out = keybindListView_.rows[i];
		out.actionLabel = std::string(GetActionInfo(action).label) + ":";
		out.primaryLabel = GetBindingLabel(ctx, action, 0);
		out.secondaryLabel = GetBindingLabel(ctx, action, 1);
		out.operatorLabel = v.and_ ? "AND" : "OR";
		out.rebindingPrimary = (rebindRow == row && rebindSlot == 0);
		out.rebindingSecondary = (rebindRow == row && rebindSlot == 1);
		keybindListView_.visibleRowCount = i + 1;
	}

	CLAY({ .id = CLAY_ID("OptionsControlsRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { static_cast<uint16_t>(framePadLeft),
	                        static_cast<uint16_t>(framePadRight),
	                        static_cast<uint16_t>(framePadTop),
	                        static_cast<uint16_t>(framePadBottom) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       },
	       .image = { .imageData = PackImageStretch(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsControlsPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(options_controls_screen_detail::kPanelMinW),
		                       CLAY_SIZING_GROW(options_controls_screen_detail::kPanelMinH) },
		           .padding = { options_controls_screen_detail::kPanelPadX, options_controls_screen_detail::kPanelPadX,
		                        options_controls_screen_detail::kPanelPadTop, static_cast<uint16_t>(panelPadBottom) },
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImageStretch(7, 7) } }) {
			BuildKeybindListBody(keybindListView_, interactions);
		}
	}
}

void OptionsControlsScreen::Destroy(ScreenContext & ctx) {
	(void)ctx;
}
