#include "client/ui/ClientUi.h"

#include "client/ui/hud/HudPayloadArena.h"
#include "screen.h"
#include "screen_context.h"
#include "runtime/UiInputRouter.h"

#ifndef SILENCER_TEST_BUILD
#include "audio.h"
#include "gasloader.h"
#include "world.h"
#endif

#include <utility>

namespace silencer {
namespace client_ui {

namespace clientui_detail {

bool IsAudibleInteractable(const silencer::ui::UiInteractable& widget) {
	if(widget.inactive) return false;
	return widget.kind == silencer::ui::UiInteractableKind::Button ||
	       widget.kind == silencer::ui::UiInteractableKind::Toggle;
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
#ifdef SILENCER_TEST_BUILD
	(void)ctx;
#else
	Audio& audio = Audio::GetInstance();
	if(!audio.enabled) return;
	const std::string& sound = GASLoader::Get().player.soundUIClick;
	auto it = ctx.world.resources.soundbank.find(sound);
	if(it == ctx.world.resources.soundbank.end() || !it->second) return;
	audio.Play(it->second);
#endif
}

}  // namespace clientui_detail

ClientUi::ClientUi(silencer::ui::ClayService& clay)
	: clay_(clay) {}

ClientUi::~ClientUi() = default;

void ClientUi::BeginFrame(const silencer::ui::UiInputState& input) {
	frameCtx_.BeginFrame();
	silencer::client_ui::HudPayloadBeginFrame();
	clay_.BeginFrame(input, interactions_);
}

std::vector<silencer::ui::UiRenderCommand> ClientUi::EndFrame() {
	return clay_.EndFrame();
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
	screens_.BuildVisible(ctx, dst, frametime, interactions_);
}

}  // namespace client_ui
}  // namespace silencer
