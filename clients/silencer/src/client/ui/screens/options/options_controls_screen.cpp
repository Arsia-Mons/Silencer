#include "options_controls_screen.h"

#include "client/ui/ClientUi.h"
#include "controls_keybind_list.h"
#include "controls_rebind_capture.h"

#include "action_catalog.h"
#include "screen_context.h"
#include "game_state.h"
#include "config.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "ui/game_ui_frame_provider.h"
#include "runtime/UiInteractionRegistry.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
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

template <typename Text>
bool StartsWith(const Text & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

template <typename Text>
int SuffixInt(const Text & value, const char * prefix) {
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

std::function<void()> UseQueuedAction(std::function<void()> write)
{
	auto queueWrite = silencer::client_ui::UseUiWriteQueue();
	if(!queueWrite) return {};
	return [queueWrite, write]() {
		queueWrite(write);
	};
}

void Invoke(const std::function<void()> & action)
{
	if(action) action();
}

}  // namespace options_controls_screen_detail

int OptionsControlsScreen::MaxScroll() const {
	int max = (int)Action::Count - visibleRowCapacity_;
	return max < 0 ? 0 : max;
}

void OptionsControlsScreen::Build(ScreenContext & ctx) {
	ctx.ResetMenuPresentation(1);
	scrollPosition = 0;
	rebindRow = -1;
	rebindSlot = -1;
	cyclePreset = {};
	save = {};
	cancel = {};
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
	if(operatorClickedRow >= 0 && operatorClickedRow < (int)Action::Count){
		Action a = ACTION_TABLE[operatorClickedRow].action;
		ScreenContext::LegacyKeyBindingSlots v = ctx.LegacyKeyBinding(a);
		v.and_ = !v.and_;
		ctx.WriteLegacyKeyBinding(a, v.key1, v.key2, v.and_);
		operatorClickedRow = -1;
	}
	if(rebindRow >= 0){
		const Uint32 tickCount = ctx.WorldTickCount();
		if(optionscontrolstick == 0){
			optionscontrolstick = tickCount;
		}
		if(rebindRow >= 0 &&
		   tickCount - optionscontrolstick > options_controls_screen_detail::REBIND_TIMEOUT_TICKS){
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
		options_controls_screen_detail::Invoke(cancel);
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
		options_controls_screen_detail::Invoke(cyclePreset);
		return true;
	}
	if(action.id == options_controls_screen_detail::kActionSave){
		options_controls_screen_detail::Invoke(save);
		return true;
	}
	if(action.id == options_controls_screen_detail::kActionCancel){
		options_controls_screen_detail::Invoke(cancel);
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

	cyclePreset = options_controls_screen_detail::UseQueuedAction([&ctx]() {
		ctx.CycleKeybindPreset();
	});
	save = options_controls_screen_detail::UseQueuedAction([&ctx]() {
		ctx.SaveActiveKeybindProfileIfCustom();
		Config::GetInstance().Save();
		ctx.GoToState(GameState::OPTIONS);
	});
	cancel = options_controls_screen_detail::UseQueuedAction([&ctx]() {
		ctx.ReloadActiveKeymap();
		Config::GetInstance().Load();
		ctx.GoToState(GameState::OPTIONS);
	});

	const silencer::ui::UiInputState & input =
		silencer::game_ui::RequireGameUiFrame().input;
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
	keybindListView_.presetText = ctx.KeybindPresetText();
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
		ScreenContext::LegacyKeyBindingSlots v = ctx.LegacyKeyBinding(action);
		KeybindRowView & out = keybindListView_.rows[i];
		out.actionLabel = std::string(GetActionInfo(action).label) + ":";
		out.primaryLabel = ctx.KeyBindingSlotLabel(action, 0);
		out.secondaryLabel = ctx.KeyBindingSlotLabel(action, 1);
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
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_GROW(options_controls_screen_detail::kPanelMinH) },
		           .padding = { static_cast<uint16_t>(panelPadX), static_cast<uint16_t>(panelPadX),
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
	cyclePreset = {};
	save = {};
	cancel = {};
}
