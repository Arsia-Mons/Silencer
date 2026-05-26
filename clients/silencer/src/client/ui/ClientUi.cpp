#include "client/ui/ClientUi.h"

#include "client/ui/hud/HudPayloadArena.h"
#include "screen.h"
#include "screen_context.h"
#include "runtime/UiInputRouter.h"
#include "runtime/react.h"

#ifndef SILENCER_TEST_BUILD
#include "audio.h"
#include "gasloader.h"
#include "world.h"
#endif

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

bool IsAudibleInteractable(const silencer::ui::UiInteractable& widget) {
	if(widget.inactive) return false;
	// Only buttons emit hover/activate audio. Legacy toggles (e.g. the
	// lobby agency icons) were silent on both hover and click; treating
	// them as audible was a migration regression.
	return widget.kind == silencer::ui::UiInteractableKind::Button;
}

bool PointIn(const silencer::ui::UiInteractable& widget, int x, int y) {
	return x >= widget.x && y >= widget.y
	    && x < widget.x + widget.w && y < widget.y + widget.h;
}

std::string InteractableAudioId(const silencer::ui::UiInteractable& widget) {
	if(!widget.id.empty()) return widget.id;
	if(widget.uid >= 0) return std::to_string(widget.uid);
	return widget.labelText;
}

const silencer::ui::UiInteractable * HitAudibleInteractable(
	const silencer::ui::UiInteractionRegistry& interactions,
	const silencer::ui::UiInputState& input) {
	const int x = static_cast<int>(input.pointer.x);
	const int y = static_cast<int>(input.pointer.y);
	const auto& widgets = interactions.Interactables();
	for(auto it = widgets.rbegin(); it != widgets.rend(); ++it){
		if(IsAudibleInteractable(*it) && PointIn(*it, x, y)) return &*it;
	}
	return nullptr;
}

bool ActionTargetsAudibleInteractable(const silencer::ui::UiInteractionRegistry& interactions,
                                      const silencer::ui::UiAction& action) {
	if(action.kind != silencer::ui::UiActionKind::Activate &&
	   action.kind != silencer::ui::UiActionKind::Navigate){
		return false;
	}
	const auto * widget = interactions.FindInteractableById(action.id);
	return widget && IsAudibleInteractable(*widget);
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
	for(silencer::ui::UiNavAction action : input.navActions){
		switch(action){
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

void PlayMenuButtonSound(ScreenContext& ctx) {
#ifdef SILENCER_TEST_BUILD
	(void)ctx;
#else
	Audio& audio = Audio::GetInstance();
	if(!audio.enabled) return;
	const std::string& sound = GASLoader::Get().player.soundUIClick;
	auto it = ctx.world.resources.soundbank.find(sound);
	if(it == ctx.world.resources.soundbank.end() || !it->second) return;
	audio.PlayUI(it->second);
#endif
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
}

Clay_RenderCommandArray ClientUi::EndFrame() {
	Clay_RenderCommandArray commands = clay_.EndPreparedLayout();
	silencer::ui::ui_focus_set_current(&focus_);
	silencer::ui::ui_focus_end_layout(focusInput_);
	clay_.EndPreparedFrame();
	return commands;
}

std::vector<silencer::ui::UiAction> ClientUi::DispatchInput(
	ScreenContext& ctx,
	const silencer::ui::UiInputState& input) {
	Screen * top = screens_.Top();
	silencer::ui::UiInputRouter router(interactions_);
	std::vector<silencer::ui::UiAction> actions = router.Route(input);
	bool playedFeedback = false;
	const silencer::ui::UiInteractable * hovered =
		clientui_detail::HitAudibleInteractable(interactions_, input);
	std::string hoveredId = hovered ? clientui_detail::InteractableAudioId(*hovered) : std::string();
	if(!hoveredId.empty() && hoveredId != hoveredAudioInteractableId_){
		clientui_detail::PlayMenuButtonSound(ctx);
		playedFeedback = true;
	}
	hoveredAudioInteractableId_ = hoveredId;
	for(const silencer::ui::UiAction& action : actions){
		if(!playedFeedback && clientui_detail::ActionTargetsAudibleInteractable(interactions_, action)){
			clientui_detail::PlayMenuButtonSound(ctx);
			playedFeedback = true;
		}
	}
	if(!top) return actions;
	std::vector<silencer::ui::UiAction> unhandled;
	for(const silencer::ui::UiAction& action : actions){
		if(top && top->HandleUiIntent(ctx, action)){
			if(action.kind == silencer::ui::UiActionKind::CaptureBinding){
				return std::vector<silencer::ui::UiAction>();
			}
			continue;
		}
		unhandled.push_back(action);
	}
	return unhandled;
}

std::vector<silencer::ui::UiAction> ClientUi::DrainActions() {
	return interactions_.DrainActions();
}

void ClientUi::PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	screens_.Push(std::move(screen), ctx);
}

void ClientUi::PopScreen(ScreenContext& ctx) {
	screens_.Pop(ctx);
}

void ClientUi::ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	screens_.Replace(std::move(screen), ctx);
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
	if(writeCount_ >= CLIENT_UI_MAX_WRITES) return false;
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
void ClientUi::PushBuiltScreenForTest(std::unique_ptr<Screen> screen) {
	screens_.PushBuiltForTest(std::move(screen));
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
