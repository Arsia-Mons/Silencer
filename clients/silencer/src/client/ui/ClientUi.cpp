#include "client/ui/ClientUi.h"

#include "client/ui/hooks/use_app.h"
#include "client/ui/hooks/use_match.h"
#include "client/ui/hud/ingame_overlay_frame.h"
#include "client/ui/hud/ingame_overlay_frame_state.h"
#include "client/ui/hud/ingame_overlay_interaction.h"
#include "game.h"
#include "lobby_screen.h"
#include "main_menu_screen.h"
#include "mission_summary_screen.h"
#include "screen.h"
#include "screen_context.h"
#include "runtime/UiInputRouter.h"
#include "world.h"

#include <utility>

namespace silencer {
namespace client_ui {

namespace clientui_detail {

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

void PlayMenuButtonSound(ScreenContext& ctx) {
	use_app(MakeAppProvider(ctx)).audio.play_ui_click();
}

}  // namespace clientui_detail

ClientUi::ClientUi(silencer::ui::ClayService& clay)
	: clay_(clay) {}

ClientUi::~ClientUi() = default;

NavigationProviderValue ClientUi::MakeNavigationProvider(ScreenContext& ctx) {
	return NavigationProviderValue{
		[this, &ctx](std::unique_ptr<Screen> screen) {
			return RequestPushScreen(std::move(screen), ctx);
		},
		[this, &ctx](std::unique_ptr<Screen> screen) {
			RequestResetToScreen(std::move(screen), ctx);
		},
		[this, &ctx]() {
			RequestPopScreen(ctx);
		},
		[this, &ctx]() {
			RequestPopScreen(ctx);
		},
	};
}

void ClientUi::BeginFrame(const silencer::ui::UiInputState& input) {
	frameCtx_.BeginFrame(input.animationDeltaSeconds, input.animationStepSeconds);
	clay_.BeginFrame(input, interactions_);
}

std::vector<silencer::ui::UiRenderCommand> ClientUi::EndFrame() {
	return clay_.EndFrame();
}

std::vector<silencer::ui::UiAction> ClientUi::DispatchInput(
	ScreenContext& ctx,
	const silencer::ui::UiInputState& input) {
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
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
	if(!top){
		MatchModel match = use_match(MakeMatchProvider(ctx));
		if(match.active()){
			ApplyInGameOverlayIntents(
				match,
				inGameOverlayFrameActive_ ? &inGameOverlayFrame_ : nullptr,
				actions,
				interactions_);
			FlushNavigationRequests(ctx);
			return std::vector<silencer::ui::UiAction>();
		}
		return actions;
	}
	std::vector<silencer::ui::UiAction> unhandled;
	deferNavigationRequests_ = true;
	bool suppressUnhandled = false;
	for(const silencer::ui::UiAction& action : actions){
		if(top && top->HandleUiIntent(ctx, action)){
			if(action.kind == silencer::ui::UiActionKind::CaptureBinding){
				suppressUnhandled = true;
				break;
			}
			continue;
		}
		unhandled.push_back(action);
	}
	deferNavigationRequests_ = false;
	FlushNavigationRequests(ctx);
	if(suppressUnhandled) return std::vector<silencer::ui::UiAction>();
	return unhandled;
}

std::vector<silencer::ui::UiAction> ClientUi::DrainActions() {
	return interactions_.DrainActions();
}

bool ClientUi::HasInputTarget(ScreenContext& ctx) const {
	if(TopScreen()) return true;
	MatchModel match = use_match(MakeMatchProvider(ctx));
	if(!match.active()) return false;
	return match.hud.has_input_target();
}

void ClientUi::EnsureDefaultScreen(ScreenContext& ctx) {
	if(HasScreens()) return;
	PushScreen(std::make_unique<MainMenuScreen>(), ctx);
}

bool ClientUi::HandleBack(ScreenContext& ctx) {
	Screen * top = screens_.Top();
	if(!top) return false;
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	return top->HandleBack(ctx);
}

void ClientUi::RunNavigationRequests(ScreenContext& ctx, std::vector<std::function<void()>> requests) {
	if(requests.empty()) return;
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	for(auto& request : requests){
		if(request) request();
	}
}

Screen * ClientUi::PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	return screens_.Push(std::move(screen), ctx);
}

void ClientUi::PopScreen(ScreenContext& ctx) {
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	screens_.Pop(ctx);
}

void ClientUi::ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	screens_.Replace(std::move(screen), ctx);
}

Screen * ClientUi::ResetToScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	return screens_.ResetTo(std::move(screen), ctx);
}

Screen * ClientUi::RequestPushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return nullptr;
	if(deferNavigationRequests_){
		QueueNavigationRequest(NavigationRequest{
			NavigationRequestKind::Push,
			std::move(screen),
		});
		return nullptr;
	}
	return PushScreen(std::move(screen), ctx);
}

void ClientUi::RequestResetToScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return;
	if(deferNavigationRequests_){
		QueueNavigationRequest(NavigationRequest{
			NavigationRequestKind::ResetTo,
			std::move(screen),
		});
		return;
	}
	ResetToScreen(std::move(screen), ctx);
}

void ClientUi::RequestPopScreen(ScreenContext& ctx) {
	if(deferNavigationRequests_){
		QueueNavigationRequest(NavigationRequest{NavigationRequestKind::PopTop});
		return;
	}
	PopScreen(ctx);
}

void ClientUi::QueueNavigationRequest(NavigationRequest request) {
	deferredNavigationRequests_.push_back(std::move(request));
}

void ClientUi::FlushNavigationRequests(ScreenContext& ctx) {
	if(deferredNavigationRequests_.empty()) return;
	std::vector<NavigationRequest> requests;
	requests.swap(deferredNavigationRequests_);
	const bool previousDefer = deferNavigationRequests_;
	deferNavigationRequests_ = false;
	for(auto& request : requests){
		switch(request.kind){
			case NavigationRequestKind::Push:
				PushScreen(std::move(request.screen), ctx);
				break;
			case NavigationRequestKind::ResetTo:
				ResetToScreen(std::move(request.screen), ctx);
				break;
			case NavigationRequestKind::PopTop:
				PopScreen(ctx);
				break;
		}
	}
	deferNavigationRequests_ = previousDefer;
}

Screen * ClientUi::ShowMainMenu(ScreenContext& ctx) {
	return ResetToScreen(std::make_unique<MainMenuScreen>(), ctx);
}

Screen * ClientUi::ShowLobby(ScreenContext& ctx) {
	return ResetToScreen(std::make_unique<LobbyScreen>(), ctx);
}

Screen * ClientUi::ShowMissionSummary(ScreenContext& ctx) {
	return ResetToScreen(std::make_unique<MissionSummaryScreen>(), ctx);
}

void ClientUi::RequestMainMenuAfterClear() {
	RequestScreenAfterClear(&ClientUi::ShowMainMenu);
}

void ClientUi::RequestLobbyAfterClear() {
	RequestScreenAfterClear(&ClientUi::ShowLobby);
}

void ClientUi::RequestMissionSummaryAfterClear() {
	RequestScreenAfterClear(&ClientUi::ShowMissionSummary);
}

void ClientUi::RequestScreenAfterClear(ScreenRequest request) {
	screenAfterClear_ = request;
	hasScreenAfterClear_ = request != nullptr;
}

void ClientUi::RequestClearScreens() {
	screens_.RequestClear();
}

void ClientUi::ClearScreensIfRequested(ScreenContext& ctx) {
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	if(screens_.ClearIfRequested(ctx) && hasScreenAfterClear_){
		pendingScreenRequest_ = screenAfterClear_;
		hasPendingScreenRequest_ = true;
		screenAfterClear_ = nullptr;
		hasScreenAfterClear_ = false;
	}
}

void ClientUi::RunPendingScreenRequest(ScreenContext& ctx) {
	if(!hasPendingScreenRequest_) return;
	ScreenRequest request = pendingScreenRequest_;
	pendingScreenRequest_ = nullptr;
	hasPendingScreenRequest_ = false;
	if(request) (this->*request)(ctx);
}

void ClientUi::TickVisibleScreens(ScreenContext& ctx) {
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	screens_.TickVisible(ctx);
	MatchModel match = use_match(MakeMatchProvider(ctx));
	if(match.active()){
		match.hud.update_overlay_state();
	}
}

void ClientUi::BuildVisibleScreens(ScreenContext& ctx,
                                   Surface& dst,
                                   float frametime,
                                   const silencer::ui::UiInputState& input,
                                   Uint8 hudPhase) {
	inGameOverlayFrameActive_ = false;
	NavigationProviderScope navigationScope(MakeNavigationProvider(ctx));
	screens_.BuildVisible(ctx, dst, frametime, interactions_);
	MatchModel match = use_match(MakeMatchProvider(ctx));
	if(match.active()){
		HudView hudView = match.hud.snapshot();
		const InGameOverlayFrameState overlayFrame = MakeInGameOverlayFrameState(
			hudView,
			match,
			input.width,
			input.height,
			hudPhase);
		inGameOverlayFrameActive_ = overlayFrame.visible;
		if(inGameOverlayFrameActive_){
			inGameOverlayFrame_.Build([&]() {
				                          return InGameOverlayFrame(overlayFrame.props);
			                          },
			                          input.width,
			                          input.height,
			                          interactions_);
			if(overlayFrame.focus_station_row){
				FocusSelectedInGameStationRow(hudView, interactions_);
			}
		}
	}
}

std::vector<const ::ui::DrawCommandList *> ClientUi::RetainedDrawCommands() const {
	std::vector<const ::ui::DrawCommandList *> commands =
		screens_.RetainedDrawCommands();
	if(inGameOverlayFrameActive_){
		commands.push_back(&inGameOverlayFrame_.Commands());
	}
	return commands;
}

}  // namespace client_ui
}  // namespace silencer
