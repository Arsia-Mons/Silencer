#include "options_controls_screen.h"

#include "client/ui/hooks/use_navigation.h"
#include "client/ui/hooks/use_options.h"
#include "client/ui/screens/options/options_controls_frame.h"
#include "controls_keybind_list.h"

#include "screen_context.h"
#include "game.h"
#include "surface.h"

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

void OptionsControlsScreen::Tick(ScreenContext & ctx) {
	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));

	if(scrollDelta != 0){
		scrollPosition = std::max(0, std::min(MaxScroll(), scrollPosition + scrollDelta));
		scrollDelta = 0;
	}
	if(rebindRow >= 0){
		const int tick = options.controls.tick_count();
		if(optionscontrolstick == 0){
			optionscontrolstick = tick;
		}
		if(rebindRow >= 0 &&
		   tick - optionscontrolstick > options_controls_screen_detail::REBIND_TIMEOUT_TICKS){
			options.controls.finish_keyboard_rebind(rebindRow, rebindSlot, SDL_SCANCODE_UNKNOWN);
		}
	}else{
		optionscontrolstick = 0;
	}
}

bool OptionsControlsScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) {
	using namespace silencer::client_ui::options;
	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));

	if(action.kind == silencer::ui::UiActionKind::CaptureBinding){
		if(rebindRow < 0) return false;
		options.controls.finish_binding_rebind(rebindRow, rebindSlot, action.binding);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		options.controls.cancel();
		silencer::client_ui::use_navigation().pop_top();
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		if(action.id.empty() || action.id == kKeybindListScrollId){
			scrollDelta += action.amount;
			return true;
		}
		return false;
	}
	return retainedFrame_.HandleUiIntent(action);
}

void OptionsControlsScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, const silencer::ui::UiInputState&, silencer::ui::UiInteractionRegistry& interactions) {
	(void)frametime;
	using namespace silencer::client_ui::options;
	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();

	const float uiScale = silencer::clay_bridge::UiScale();
	const int layoutWidth = std::max(1, static_cast<int>(dst.w / uiScale));
	const int layoutHeight = std::max(1, static_cast<int>(dst.h / uiScale));
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
	keybindListView_.presetText = options.controls.profile_label();
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
		silencer::client_ui::OptionsBindingView v = options.controls.binding(action);
		KeybindRowView & out = keybindListView_.rows[i];
		out.actionLabel = std::string(GetActionInfo(action).label) + ":";
		out.primaryLabel = options.controls.binding_label(action, 0);
		out.secondaryLabel = options.controls.binding_label(action, 1);
		out.operatorLabel = v.and_ ? "AND" : "OR";
		out.rebindingPrimary = (rebindRow == row && rebindSlot == 0);
		out.rebindingSecondary = (rebindRow == row && rebindSlot == 1);
		keybindListView_.visibleRowCount = i + 1;
	}

	silencer::client_ui::OptionsControlsFrameProps props{
		.key = "options-controls",
		.view = &keybindListView_,
		.frame_pad_left = framePadLeft,
		.frame_pad_right = framePadRight,
		.frame_pad_top = framePadTop,
		.frame_pad_bottom = framePadBottom,
		.panel_pad_x = panelPadX,
		.panel_pad_bottom = panelPadBottom,
		.cycle_preset = [controls = options.controls]() {
			controls.cycle_preset();
		},
		.begin_rebind = [this](int row, int slot) {
			BeginRebindFromVisibleRow(row, slot);
		},
		.toggle_operator = [this, controls = options.controls](int row) {
			int absolute = scrollPosition + row;
			if(absolute < 0 || absolute >= (int)Action::Count) return;
			controls.toggle_operator(ACTION_TABLE[absolute].action);
		},
		.save = [controls = options.controls, navigation]() {
			controls.save();
			navigation.pop_top();
		},
		.cancel = [controls = options.controls, navigation]() {
			controls.cancel();
			navigation.pop_top();
		},
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::OptionsControlsFrame(props);
	                     },
	                     layoutWidth,
	                     layoutHeight,
	                     interactions);
}

void OptionsControlsScreen::Destroy(ScreenContext & ctx) {
	(void)ctx;
}

const ::ui::DrawCommandList * OptionsControlsScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
