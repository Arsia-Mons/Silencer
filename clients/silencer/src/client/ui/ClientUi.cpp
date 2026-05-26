#include "client/ui/ClientUi.h"

#include "client/ui/hud/HudPayloadArena.h"
#include "screen.h"
#include "screen_context.h"
#include "runtime/UiInputRouter.h"
#include "runtime/react.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <utility>

namespace silencer {
namespace client_ui {

namespace clientui_detail {

struct ScreenProviderContext {
	ClientUi * clientUi = nullptr;
	UiScreenEntryId currentEntryId = 0;
};

static ReactContext g_screenContextValue = {};

bool InteractableRequestsFeedback(const silencer::ui::UiInteractable& widget) {
	if(widget.inactive) return false;
	// Only buttons request hover/activate feedback. Legacy toggles (e.g. the
	// lobby agency icons) were silent on both hover and click; treating
	// them as feedback targets was a migration regression.
	return widget.kind == silencer::ui::UiInteractableKind::Button;
}

bool PointIn(const silencer::ui::UiInteractable& widget, int x, int y) {
	return x >= widget.x && y >= widget.y
	    && x < widget.x + widget.w && y < widget.y + widget.h;
}

bool SameActionId(const silencer::ui::UiActionId& lhs,
                  const silencer::ui::UiActionId& rhs) {
	return lhs.size() == rhs.size() &&
	       std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

void AssignInteractableFeedbackId(silencer::ui::UiActionId& id,
                                  const silencer::ui::UiInteractable& widget) {
	if(!widget.id.empty()){
		id.Assign(widget.id.data(), widget.id.size());
		return;
	}
	if(widget.uid >= 0){
		char uidText[16] = {};
		const int n = std::snprintf(uidText, sizeof(uidText), "%d", widget.uid);
		if(n > 0){
			const std::size_t len = n < static_cast<int>(sizeof(uidText))
				? static_cast<std::size_t>(n)
				: sizeof(uidText) - 1;
			id.Assign(uidText, len);
			return;
		}
	}
	const char * label = silencer::ui::UiInteractableLabel(widget);
	id.Assign(label);
}

const silencer::ui::UiInteractable * HitFeedbackInteractable(
	const silencer::ui::UiInteractionRegistry& interactions,
	const silencer::ui::UiInputState& input) {
	const int x = static_cast<int>(input.pointer.x);
	const int y = static_cast<int>(input.pointer.y);
	const auto& widgets = interactions.Interactables();
	for(auto it = widgets.rbegin(); it != widgets.rend(); ++it){
		if(InteractableRequestsFeedback(*it) && PointIn(*it, x, y)) return &*it;
	}
	return nullptr;
}

bool ActionTargetsFeedbackInteractable(const silencer::ui::UiInteractionRegistry& interactions,
                                       const silencer::ui::UiAction& action) {
	if(action.kind != silencer::ui::UiActionKind::Activate &&
	   action.kind != silencer::ui::UiActionKind::Navigate){
		return false;
	}
	const auto * widget = interactions.FindInteractableById(action.id.data(), action.id.size());
	return widget && InteractableRequestsFeedback(*widget);
}

bool FocusRuntimeRoutedThisFrame(const silencer::ui::UiFocusRuntime& focus) {
	for(int i = 0; i < focus.scopeCount; ++i){
		const silencer::ui::UiFocusScope& scope = focus.scopes[i];
		if(scope.declaredFrame == focus.frame && scope.layoutCount > 0){
			return true;
		}
	}
	return false;
}

bool FocusRuntimeHasDeclaredFocusThisFrame(const silencer::ui::UiFocusRuntime& focus) {
	for(int i = 0; i < focus.scopeCount; ++i){
		const silencer::ui::UiFocusScope& scope = focus.scopes[i];
		if(scope.declaredFrame == focus.frame &&
		   scope.layoutCount > 0 &&
		   scope.focusedId.id != 0){
			return true;
		}
	}
	return false;
}

silencer::ui::UiFocusInputFrame FocusInputFrom(
	const silencer::ui::UiInputState& input) {
	silencer::ui::UiFocusInputFrame out;
	out.source = input.source == silencer::ui::UiFocusSource::None
		? silencer::ui::UiFocusSource::Keyboard
		: input.source;
	out.pointerPressed = input.pointer.pressed;
	out.pointerDown = input.pointer.down;
	out.pointerReleased = input.pointer.released;
	out.pointerMoved = input.pointer.moved;
	out.pointerX = input.pointer.x;
	out.pointerY = input.pointer.y;
	for(silencer::ui::UiNavAction action : input.navActions){
		switch(action){
			case silencer::ui::UiNavAction::FocusPrevious:
			case silencer::ui::UiNavAction::PreviousSection:
				out.navUp = true;
				break;
			case silencer::ui::UiNavAction::FocusNext:
			case silencer::ui::UiNavAction::NextSection:
				out.navDown = true;
				break;
			case silencer::ui::UiNavAction::Up:
				out.navUp = true;
				break;
			case silencer::ui::UiNavAction::Down:
				out.navDown = true;
				break;
			case silencer::ui::UiNavAction::Left:
				out.navLeft = true;
				break;
			case silencer::ui::UiNavAction::Right:
				out.navRight = true;
				break;
			case silencer::ui::UiNavAction::Confirm:
				out.confirmPressed = true;
				out.confirmDown = true;
				break;
			case silencer::ui::UiNavAction::Cancel:
				out.cancelPressed = true;
				out.cancelDown = true;
				break;
			default:
				break;
		}
	}
	return out;
}

bool RouteAfterFocusRuntime(silencer::ui::UiNavAction action,
                            bool keepConfirmForTextFocus) {
	switch(action){
		case silencer::ui::UiNavAction::Backspace:
		case silencer::ui::UiNavAction::Cancel:
			return true;
		case silencer::ui::UiNavAction::Confirm:
			return keepConfirmForTextFocus;
		case silencer::ui::UiNavAction::FocusNext:
		case silencer::ui::UiNavAction::FocusPrevious:
		case silencer::ui::UiNavAction::Up:
		case silencer::ui::UiNavAction::Down:
		case silencer::ui::UiNavAction::Left:
		case silencer::ui::UiNavAction::Right:
		case silencer::ui::UiNavAction::NextSection:
		case silencer::ui::UiNavAction::PreviousSection:
			return false;
	}
	return false;
}

silencer::ui::UiInputState InputForLegacyRouterAfterFocusRuntime(
	const silencer::ui::UiInputState& input,
	bool keepConfirmForTextFocus) {
	silencer::ui::UiInputState out = input;
	out.pointer.pressed = false;
	out.pointer.released = false;
	out.pointer.moved = false;
	out.navActions.clear();
	for(silencer::ui::UiNavAction action : input.navActions){
		if(RouteAfterFocusRuntime(action, keepConfirmForTextFocus)){
			out.navActions.push_back(action);
		}
	}
	out.controlCommands.clear();
	for(const silencer::ui::UiControlCommand& command : input.controlCommands){
		if(command.kind == silencer::ui::UiControlCommandKind::Action){
			out.controlCommands.push_back(command);
		}
	}
	return out;
}

}  // namespace clientui_detail

ClientUi::ClientUi(silencer::ui::ClayService& clay)
	: clay_(clay) {
	silencer::ui::ui_focus_init(&focus_);
}

ClientUi::~ClientUi() = default;

void ClientUi::BeginFrame(const silencer::ui::UiInputState& input) {
	frameCtx_.BeginFrame(input.animationDeltaSeconds, input.animationStepSeconds);
	silencer::client_ui::HudPayloadBeginFrame();
	ClearWrites();
	focusInput_ = clientui_detail::FocusInputFrom(input);
	clay_.PrepareFrame(input, interactions_);
	silencer::ui::ui_focus_set_current(&focus_);
	silencer::ui::ui_focus_begin_frame(focusInput_);
	clay_.BeginPreparedLayout();
	silencer::ui::ui_focus_push_scope({
		CLAY_ID("ClientUiFocusScope"),
		false,
		true,
	});
	focusScopeOpen_ = true;
}

Clay_RenderCommandArray ClientUi::EndFrame() {
	if(focusScopeOpen_){
		silencer::ui::ui_focus_pop_scope();
		focusScopeOpen_ = false;
	}
	Clay_RenderCommandArray commands = clay_.EndPreparedLayout();
	silencer::ui::ui_focus_set_current(&focus_);
	silencer::ui::ui_focus_end_layout(focusInput_);
	if(clientui_detail::FocusRuntimeRoutedThisFrame(focus_) &&
	   !clientui_detail::FocusRuntimeHasDeclaredFocusThisFrame(focus_)){
		interactions_.ClearFocus();
	}
	clay_.EndPreparedFrame();
	return commands;
}

UiDispatchResult ClientUi::DispatchInput(
	ScreenContext * ctx,
	const silencer::ui::UiInputState& input) {
	UiDispatchResult result;
	Screen * top = screens_.Top();
	silencer::ui::UiInputRouter router(interactions_);
	const bool focusRouted = clientui_detail::FocusRuntimeRoutedThisFrame(focus_);
	const silencer::ui::UiInputState routedInput = focusRouted
		? clientui_detail::InputForLegacyRouterAfterFocusRuntime(
			input, interactions_.HasTextInputFocus())
		: input;
	silencer::ui::UiActionList actions = router.Route(routedInput);
	const silencer::ui::UiInteractable * hovered =
		clientui_detail::HitFeedbackInteractable(interactions_, input);
	silencer::ui::UiActionId hoveredId;
	if(hovered) clientui_detail::AssignInteractableFeedbackId(hoveredId, *hovered);
	if(!hoveredId.empty() &&
	   !clientui_detail::SameActionId(hoveredId, hoveredFeedbackInteractableId_)){
		result.feedbackRequested = true;
	}
	hoveredFeedbackInteractableId_ = hoveredId;
	for(const silencer::ui::UiAction& action : actions){
		if(!result.feedbackRequested &&
		   clientui_detail::ActionTargetsFeedbackInteractable(interactions_, action)){
			result.feedbackRequested = true;
		}
	}
	if(!top || !ctx){
		result.unhandledActions = actions;
		return result;
	}
	for(const silencer::ui::UiAction& action : actions){
		if(top && top->HandleUiIntent(*ctx, action)){
			if(action.kind == silencer::ui::UiActionKind::CaptureBinding){
				result.unhandledActions.Clear();
				return result;
			}
			continue;
		}
		result.unhandledActions.Push(action);
	}
	return result;
}

silencer::ui::UiActionList ClientUi::DrainActions() {
	return interactions_.DrainActions();
}

bool ClientUi::PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	return screens_.Push(std::move(screen), ctx);
}

bool ClientUi::PopScreen(ScreenContext& ctx) {
	return screens_.Pop(ctx);
}

bool ClientUi::ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	return screens_.Replace(std::move(screen), ctx);
}

bool ClientUi::QueuePushScreen(std::unique_ptr<Screen> screen) {
	if(!screen) return false;
	QueuedWrite write;
	write.kind = WriteKind::Push;
	write.screen = std::move(screen);
	return QueueWrite(std::move(write));
}

bool ClientUi::QueuePopCurrent(UiScreenEntryId entryId) {
	if(entryId == 0) return false;
	QueuedWrite write;
	write.kind = WriteKind::PopCurrent;
	write.entryId = entryId;
	return QueueWrite(std::move(write));
}

bool ClientUi::QueuePopTop() {
	QueuedWrite write;
	write.kind = WriteKind::PopTop;
	return QueueWrite(std::move(write));
}

bool ClientUi::QueueDeferredWrite(UiDeferredWrite write) {
	if(!write) return false;
	QueuedWrite queued;
	queued.kind = WriteKind::Deferred;
	queued.deferred = std::move(write);
	return QueueWrite(std::move(queued));
}

bool ClientUi::QueueWrite(QueuedWrite write) {
	if(writeCount_ >= CLIENT_UI_MAX_WRITES) {
		++writeOverflowCount_;
		return false;
	}
	writes_[writeCount_++] = std::move(write);
	return true;
}

void ClientUi::DrainWrites(ScreenContext& ctx) {
	for(int i = 0; i < writeCount_; ++i){
		QueuedWrite& write = writes_[i];
		switch(write.kind){
			case WriteKind::Push:
				screens_.Push(std::move(write.screen), ctx);
				break;
			case WriteKind::PopCurrent:
				screens_.PopEntry(write.entryId, ctx);
				break;
			case WriteKind::PopTop:
				screens_.Pop(ctx);
				break;
			case WriteKind::Deferred:
				if(write.deferred) write.deferred();
				break;
		}
	}
	ClearWrites();
}

void ClientUi::ClearWrites() {
	for(int i = 0; i < writeCount_; ++i){
		writes_[i] = {};
	}
	writeCount_ = 0;
}

void ClientUi::WithScreenProvider(UiScreenEntryId entryId,
                                  const std::function<void()>& build) {
	clientui_detail::ScreenProviderContext provider{this, entryId};
	REACT_PROVIDER_ENTER_KEY("ScreenProvider", entryId);
	PROVIDE(&clientui_detail::g_screenContextValue, &provider) {
		if(build) build();
	}
	REACT_PROVIDER_EXIT();
}

void ClientUi::BuildVisibleScreenFrame(UiScreenEntryId entryId,
                                       bool overlay,
                                       int visibleIndex,
                                       const std::function<void()>& build) {
	if(!build) return;
	if(!overlay){
		CLAY({
			.id = CLAY_IDI("ClientUiScreenFrame", entryId),
			.layout = {
				.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
			},
		}) {
			build();
		}
		return;
	}

	const int16_t zIndex = static_cast<int16_t>(100 + visibleIndex);
	CLAY({
		.id = CLAY_IDI("ClientUiOverlayScreenFrame", entryId),
		.layout = {
			.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = { 0, 0, 0, 160 },
		.floating = {
			.zIndex = zIndex,
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
			.attachTo = CLAY_ATTACH_TO_ROOT,
		},
	}) {
		build();
	}
}

void ClientUi::RequestClearScreens() {
	screens_.RequestClear();
}

void ClientUi::ClearScreensIfRequested(ScreenContext& ctx) {
	screens_.ClearIfRequested(ctx);
}

void ClientUi::TickVisibleScreens(ScreenContext& ctx) {
	screens_.TickVisible(ctx);
}

void ClientUi::BuildVisibleScreens(ScreenContext& ctx, Surface& dst, float frametime) {
	const VisibleScreenSpan visible = screens_.VisibleScreens();
	for(const VisibleScreen& entry : visible){
		if(!entry.screen) continue;
		if(entry.visibleIndex > 0 && entry.overlay) {
			interactions_.BeginFrame();
		}
		BuildVisibleScreenFrame(entry.entryId, entry.overlay, entry.visibleIndex, [&] {
			WithScreenProvider(entry.entryId, [&] {
				entry.screen->BuildUi(ctx, dst, frametime, interactions_);
			});
		});
	}
}

#ifdef SILENCER_TEST_BUILD
bool ClientUi::PushBuiltScreenForTest(std::unique_ptr<Screen> screen) {
	return screens_.PushBuiltForTest(std::move(screen));
}

void ClientUi::BuildVisibleScreenProvidersForTest(
	const std::function<void(UiScreenEntryId entryId, Screen& screen)>& buildScreen) {
	const VisibleScreenSpan visible = screens_.VisibleScreens();
	for(const VisibleScreen& entry : visible){
		if(!entry.screen) continue;
		WithScreenProvider(entry.entryId, [&] {
			if(buildScreen) buildScreen(entry.entryId, *entry.screen);
		});
	}
}

void ClientUi::BuildVisibleScreenFramesForTest(
	const std::function<void(UiScreenEntryId entryId, Screen& screen, bool overlay)>& buildScreen) {
	const VisibleScreenSpan visible = screens_.VisibleScreens();
	for(const VisibleScreen& entry : visible){
		if(!entry.screen) continue;
		if(entry.visibleIndex > 0 && entry.overlay) {
			interactions_.BeginFrame();
		}
		BuildVisibleScreenFrame(entry.entryId, entry.overlay, entry.visibleIndex, [&] {
			WithScreenProvider(entry.entryId, [&] {
				if(buildScreen) buildScreen(entry.entryId, *entry.screen, entry.overlay);
			});
		});
	}
}

void ClientUi::DrainWritesForTest() {
	for(int i = 0; i < writeCount_; ++i){
		QueuedWrite& write = writes_[i];
		switch(write.kind){
			case WriteKind::Push:
				screens_.PushBuiltForTest(std::move(write.screen));
				break;
			case WriteKind::PopCurrent:
				screens_.PopEntryForTest(write.entryId);
				break;
			case WriteKind::PopTop:
				screens_.PopForTest();
				break;
			case WriteKind::Deferred:
				if(write.deferred) write.deferred();
				break;
		}
	}
	ClearWrites();
}
#endif

ScreenNavigator UseScreenNavigator() {
	auto * context = static_cast<clientui_detail::ScreenProviderContext *>(
		use_context(&clientui_detail::g_screenContextValue));
	if(!context || !context->clientUi) return {};

	ClientUi * clientUi = context->clientUi;
	UiScreenEntryId entryId = context->currentEntryId;
	ScreenNavigator navigator;
	navigator.currentEntryId = entryId;
	navigator.push = [clientUi](std::unique_ptr<Screen> screen) {
		clientUi->QueuePushScreen(std::move(screen));
	};
	navigator.popCurrent = [clientUi, entryId]() {
		clientUi->QueuePopCurrent(entryId);
	};
	navigator.popTop = [clientUi]() {
		clientUi->QueuePopTop();
	};
	return navigator;
}

QueueUiWrite UseUiWriteQueue() {
	auto * context = static_cast<clientui_detail::ScreenProviderContext *>(
		use_context(&clientui_detail::g_screenContextValue));
	if(!context || !context->clientUi) return {};

	ClientUi * clientUi = context->clientUi;
	return [clientUi](UiDeferredWrite write) {
		clientUi->QueueDeferredWrite(std::move(write));
	};
}

}  // namespace client_ui
}  // namespace silencer
