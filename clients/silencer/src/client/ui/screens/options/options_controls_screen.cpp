#include "options_controls_screen.h"

#include "controls_keybind_list.h"
#include "options_document_runtime.h"
#include "controls_rebind_capture.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "config.h"
#include "surface.h"

#include "layout/ui_document_renderer.h"
#include "layout/ui_document_runtime_registry.h"
#include "runtime/UiInteractionRegistry.h"
#include "ui_document_assets.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace options_controls_screen_detail {

constexpr int REBIND_TIMEOUT_TICKS = 72;
constexpr int kLegacyViewportW = 640;

bool IsBuiltinKeybindProfile(const std::string & name) {
	return name == "default" || name == "wasd" || name == "gamepad";
}

const silencer::ui::UiEditorNode * FindNodeByComponent(
	const silencer::ui::UiEditorNode& node,
	const char * component) {
	if(node.component == component) return &node;
	for(const silencer::ui::UiEditorNode& child : node.children){
		const silencer::ui::UiEditorNode * found =
			FindNodeByComponent(child, component);
		if(found) return found;
	}
	return nullptr;
}

int FixedSizeOr(const silencer::ui::UiEditorSize& size, int fallback) {
	if(size.mode != silencer::ui::UiEditorSize::Mode::Fixed) return fallback;
	return std::max(1, static_cast<int>(size.value + 0.5f));
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
	layoutLoaded_ = silencer::net::LoadUiDocumentAsset(
		silencer::client_ui::options_controls::kOptionsControlsSurface,
		layoutDocument_,
		layoutLoadError_);
	if(!layoutLoaded_){
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
		return;
	}
	silencer::client_ui::UiDocumentRendererOptions validationOptions =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::options_controls::kOptionsControlsSurface);
	if(!silencer::client_ui::ValidateUiDocumentRuntimeTokens(
		   layoutDocument_, validationOptions, layoutLoadError_)){
		layoutLoaded_ = false;
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
	}
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
			scrollDelta += action.amount;
			return true;
		}
		return false;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	return silencer::client_ui::options_controls::HandleOptionsControlsAction(
		action.id,
		[this](const silencer::client_ui::options_controls::OptionsControlsAction& controlsAction) {
			using Kind = silencer::client_ui::options_controls::OptionsControlsAction::Kind;
			switch(controlsAction.kind){
				case Kind::Preset:
					presetClicked = true;
					return true;
				case Kind::Save:
					saveClicked = true;
					return true;
				case Kind::Cancel:
					cancelClicked = true;
					return true;
				case Kind::Primary:
					BeginRebindFromVisibleRow(controlsAction.row, 0);
					return true;
				case Kind::Secondary:
					BeginRebindFromVisibleRow(controlsAction.row, 1);
					return true;
				case Kind::Operator:
					ToggleOperatorFromVisibleRow(controlsAction.row);
					return true;
			}
			return false;
		});
}

void OptionsControlsScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) {
	(void)frametime;
	(void)dst;
	using namespace silencer::client_ui::options;

	if(!layoutLoaded_) return;

	const silencer::ui::UiInputState & input = ctx.game.CurrentUiInput();
	const int layoutWidth = std::max(1, input.width);
	const silencer::ui::UiEditorNode * keybindRowsNode =
		options_controls_screen_detail::FindNodeByComponent(
			layoutDocument_.root,
			silencer::client_ui::options_controls::kComponentKeybindRows);
	const int keybindRowsWidth = keybindRowsNode
		? options_controls_screen_detail::FixedSizeOr(
			keybindRowsNode->style.width,
			kKeybindRowsDefaultWidth)
		: kKeybindRowsDefaultWidth;
	const int keybindRowsHeight = keybindRowsNode
		? options_controls_screen_detail::FixedSizeOr(
			keybindRowsNode->style.height,
			kKeybindRowsDefaultHeight)
		: kKeybindRowsDefaultHeight;
	visibleRowCapacity_ = std::min<int>(
		(int)Action::Count,
		KeybindRowsVisibleRowsForHeight(keybindRowsHeight));
	scrollPosition = std::max(0, std::min(MaxScroll(), scrollPosition));

	// The keybind list's interior widths are hardcoded legacy pixels (designed
	// for the 640-wide viewport). Scale them down with the window so the panel
	// interior never overflows a narrow window horizontally (issue #179
	// follow-up). Capped at 1.0 so large windows keep the design width.
	const float keybindHScale = std::min(
		1.0f,
		static_cast<float>(layoutWidth)
			/ static_cast<float>(options_controls_screen_detail::kLegacyViewportW));

	keybindListView_ = KeybindListView{};
	keybindListView_.contentWidth = keybindRowsWidth;
	keybindListView_.viewportHeight = keybindRowsHeight;
	keybindListView_.hScale = keybindHScale;
	keybindListView_.presetText = !ctx.keymap.label.empty() ? ctx.keymap.label
	                            : !ctx.keymap.name.empty() ? ctx.keymap.name
	                            : std::string(Config::GetInstance().active_keybind_profile);
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

	silencer::client_ui::UiDocumentRendererOptions options =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::options_controls::kOptionsControlsSurface);
	silencer::client_ui::options_controls::ApplyOptionsControlsRuntimeHandlers(
		options,
		silencer::client_ui::options_controls::OptionsControlsLiveRuntimeContext(
			keybindListView_));
	silencer::client_ui::BuildUiDocument(layoutDocument_, interactions, options);
}

void OptionsControlsScreen::Destroy(ScreenContext & ctx) {
	(void)ctx;
}
