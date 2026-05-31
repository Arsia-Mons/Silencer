#include "options_controls_screen.h"

#include "controls_keybind_list.h"
#include "controls_rebind_capture.h"
#include "client/ui/screens/options/options_controls_view.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "config.h"

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
constexpr uint16_t kPanelMinH = 420;
constexpr uint16_t kPanelPadX = 48;
constexpr uint16_t kPanelPadTop = 70;
constexpr const char * kActionPreset = "options_controls.preset";
constexpr const char * kActionSave = "options_controls.save";
constexpr const char * kActionCancel = "options_controls.cancel";
constexpr const char * kActionPrimaryPrefix = "options_controls.primary.";
constexpr const char * kActionSecondaryPrefix = "options_controls.secondary.";
constexpr const char * kActionOperatorPrefix = "options_controls.operator.";

static_assert(static_cast<int>(Action::Count) == silencer::client_ui::kOptionsControlsMaxRows,
              "OptionsControlsView must expose one JSX row per keybind action");

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
	scrollDelta = 0;
}

void OptionsControlsScreen::BeginRebindFromVisibleRow(int row, int slot) {
	int absolute = scrollPosition + row;
	if(absolute < 0 || absolute >= (int)Action::Count) return;
	rebindRow = absolute;
	rebindSlot = slot;
}

void OptionsControlsScreen::CyclePreset(ScreenContext & ctx) {
	CycleKeybindPreset(ctx.keymap);
}

void OptionsControlsScreen::ToggleOperatorFromVisibleRow(ScreenContext & ctx, int row) {
	using namespace silencer::client_ui::options;

	int absolute = scrollPosition + row;
	if(absolute < 0 || absolute >= (int)Action::Count) return;
	Action a = ACTION_TABLE[absolute].action;
	LegacyBindingView v = ViewLegacy(ctx.keymap, a);
	v.and_ = !v.and_;
	ForkActiveProfileIfBuiltin(ctx.keymap);
	WriteLegacy(ctx.keymap, a, v.key1, v.key2, v.and_);
}

void OptionsControlsScreen::SaveControls(ScreenContext & ctx) {
	using namespace silencer::client_ui::options;

	const std::string active = Config::GetInstance().active_keybind_profile;
	if(!options_controls_screen_detail::IsBuiltinKeybindProfile(active)){
		ctx.keymap.SaveFile(WritableProfilePath(active));
	}
	Config::GetInstance().Save();
}

void OptionsControlsScreen::CancelControls(ScreenContext & ctx) {
	using namespace silencer::client_ui::options;

	LoadActiveKeymap(ctx.keymap);
	Config::GetInstance().Load();
}

void OptionsControlsScreen::Tick(ScreenContext & ctx) {
	using namespace silencer::client_ui::options;

	if(scrollDelta != 0){
		scrollPosition = std::max(0, std::min(MaxScroll(), scrollPosition + scrollDelta));
		scrollDelta = 0;
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
}

bool OptionsControlsScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) {
	using namespace silencer::client_ui::options;

	if(action.kind == silencer::ui::UiActionKind::CaptureBinding){
		if(rebindRow < 0) return false;
		FinishBindingRebind(ctx, rebindRow, rebindSlot, action.binding);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		CancelControls(ctx);
		ctx.GoToState(GameState::OPTIONS);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		if(action.id.empty() || action.id == kKeybindListScrollId){
			scrollDelta += action.amount;
			return true;
		}
		return false;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_controls_screen_detail::kActionPreset){
		CyclePreset(ctx);
		return true;
	}
	if(action.id == options_controls_screen_detail::kActionSave){
		SaveControls(ctx);
		ctx.GoToState(GameState::OPTIONS);
		return true;
	}
	if(action.id == options_controls_screen_detail::kActionCancel){
		CancelControls(ctx);
		ctx.GoToState(GameState::OPTIONS);
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
		ToggleOperatorFromVisibleRow(ctx, row);
		return true;
	}
	return false;
}

bool OptionsControlsScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out) {
	using namespace silencer::client_ui::options;
	if(!out) return false;

	const silencer::ui::UiInputState & input = ctx.game.CurrentUiInput();
	const int layoutWidth = std::max(1, input.width);
	const int layoutHeight = std::max(1, input.height);
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

	// The keybind list's interior widths are hardcoded legacy pixels (designed
	// for the 640-wide viewport). Scale them down with the window so the panel
	// interior never overflows a narrow window horizontally (issue #179
	// follow-up). Capped at 1.0 so large windows keep the design width.
	const float keybindHScale = std::min(
		1.0f,
		static_cast<float>(layoutWidth)
			/ static_cast<float>(options_controls_screen_detail::kLegacyViewportW));
	const int panelPadX = std::max(
		1,
		static_cast<int>(options_controls_screen_detail::kPanelPadX * keybindHScale + 0.5f));

	keybindListView_ = KeybindListView{};
	keybindListView_.hScale = keybindHScale;
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

	const silencer::client_ui::OptionsControlsContextValue context{
		.keybinds = &keybindListView_,
		.frame_pad_left = framePadLeft,
		.frame_pad_right = framePadRight,
		.frame_pad_top = framePadTop,
		.frame_pad_bottom = framePadBottom,
		.panel_pad_x = panelPadX,
		.panel_pad_bottom = panelPadBottom,
		.cycle_preset = [this, screenContext = &ctx]() {
			CyclePreset(*screenContext);
		},
		.begin_rebind = [this](int row, int slot) {
			BeginRebindFromVisibleRow(row, slot);
		},
		.toggle_operator = [this, screenContext = &ctx](int row) {
			ToggleOperatorFromVisibleRow(*screenContext, row);
		},
		.save = [this, screenContext = &ctx]() {
			SaveControls(*screenContext);
		},
		.cancel = [this, screenContext = &ctx]() {
			CancelControls(*screenContext);
		},
	};
	const auto * stored = ::ui::copy_value(context);
	if(!stored) return false;
	*out = silencer::client_ui::OptionsControlsView(
		silencer::client_ui::OptionsControlsViewProps{
			.key = "options-controls",
			.value = stored,
		});
	return true;
}

void OptionsControlsScreen::Destroy(ScreenContext & ctx) {
	(void)ctx;
}
