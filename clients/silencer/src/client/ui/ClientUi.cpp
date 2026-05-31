#include "client/ui/ClientUi.h"

#include "client/ui/hud/ingame_hud_view.h"
#include "client/ui/hud/hud_retained_payloads.h"
#include "client/ui/views/HudView.h"
#include "screen.h"
#include "screen_context.h"
#include "retained_surface_renderer.h"
#include "runtime/UiInputRouter.h"
#include "ui/runtime/draw_command_builder.h"
#include "ui/runtime/react.h"
#include "ui/runtime/yoga_flex_layout.h"

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

void QueueAction(silencer::ui::UiInteractionRegistry& interactions,
                 silencer::ui::UiActionKind kind,
                 const char * id,
                 const std::string& value = std::string(),
                 int index = -1) {
	if(!id) return;
	silencer::ui::UiAction action;
	action.kind = kind;
	action.id = id;
	action.value = value;
	action.index = index;
	interactions.QueueAction(std::move(action));
}

std::string BuyTechActionId(int index) {
	return "ingame.buytech.row." + std::to_string(index);
}

::ui::InputFrame ToRetainedInput(const silencer::ui::UiInputState& input) {
	::ui::InputFrame out{};
	out.pointer_pressed = input.pointer.pressed;
	out.pointer_down = input.pointer.down;
	out.pointer_released = input.pointer.released;
	out.pointer_valid = input.HasWindow();
	out.pointer_x = input.pointer.x;
	out.pointer_y = input.pointer.y;
	out.source = (input.pointer.pressed || input.pointer.down || input.pointer.released)
		? ::ui::FocusSource::Mouse
		: ::ui::FocusSource::Keyboard;
	for(silencer::ui::UiNavAction action : input.navActions){
		switch(action){
			case silencer::ui::UiNavAction::FocusNext:
			case silencer::ui::UiNavAction::Down:
			case silencer::ui::UiNavAction::NextSection:
				out.nav_down = true;
				break;
			case silencer::ui::UiNavAction::FocusPrevious:
			case silencer::ui::UiNavAction::Up:
			case silencer::ui::UiNavAction::PreviousSection:
				out.nav_up = true;
				break;
			case silencer::ui::UiNavAction::Left:
				out.nav_left = true;
				break;
			case silencer::ui::UiNavAction::Right:
				out.nav_right = true;
				break;
			case silencer::ui::UiNavAction::Confirm:
				out.confirm_pressed = true;
				break;
			case silencer::ui::UiNavAction::Backspace:
				if(out.key_event_count < ::ui::UI_INPUT_MAX_KEY_EVENTS){
					out.key_events[out.key_event_count++] = {
						.key = ::ui::UiKey::Backspace,
						.modifiers = ::ui::UI_KEY_MOD_NONE,
						.repeat = false,
					};
				}
				break;
			case silencer::ui::UiNavAction::Cancel:
				break;
		}
	}
	if(!input.textInput.empty()){
		::ui::UiInputFrame textFrame{};
		::ui::ui_input_push_text(textFrame, input.textInput.c_str());
		out.text_event_count = textFrame.text_event_count;
		for(int i = 0; i < out.text_event_count; ++i){
			out.text_events[i] = textFrame.text_events[i];
		}
	}
	return out;
}

}  // namespace clientui_detail

ClientUi::ClientUi()
	: retainedLayout_(::ui::make_yoga_flex_layout_adapter()) {
	::ui::focus_init(&retainedFocus_);
}

ClientUi::~ClientUi() = default;

void ClientUi::BeginFrame(const silencer::ui::UiInputState& input) {
	silencer::client_ui::HudRetainedPayloadBeginFrame();
	interactions_.BeginFrame();
	retainedElementFrame_.reset();
	retainedCommands_.reset();
	retainedInput_ = clientui_detail::ToRetainedInput(input);
	retainedViewport_ = {
		static_cast<float>(input.width > 0 ? input.width : 1),
		static_cast<float>(input.height > 0 ? input.height : 1),
	};
	::react_begin_frame();
	retainedTree_.begin_frame(retainedViewport_.width, retainedViewport_.height);
	retainedFrameOpen_ = true;
}

void ClientUi::EndFrame() {
	if(retainedFrameOpen_){
		bool treeEnded = retainedTree_.end_frame();
		if(treeEnded &&
		   ::ui::compute_flex_layout(retainedLayout_, retainedTree_, retainedViewport_) &&
		   ::ui::focus_update(&retainedFocus_, retainedTree_, retainedInput_)){
			::ui::NodeId blurred = ::ui::focus_blurred_id(retainedFocus_);
			if(blurred != 0) retainedTree_.invoke_blur(blurred);
			::ui::NodeId focused = ::ui::focus_changed_id(retainedFocus_);
			if(focused != 0) retainedTree_.invoke_focus(focused);
			::ui::NodeId confirmed = ::ui::focus_confirmed_id(retainedFocus_);
			if(confirmed != 0) retainedTree_.invoke_activate(confirmed);

			::ui::NodeId active = ::ui::focus_focused_id(retainedFocus_);
			for(int i = 0; i < retainedInput_.key_event_count; ++i){
				retainedTree_.invoke_key(active, retainedInput_.key_events[i]);
			}
			for(int i = 0; i < retainedInput_.text_event_count; ++i){
				retainedTree_.invoke_text_input(active, retainedInput_.text_events[i]);
			}
			for(int i = 0; i < retainedInput_.editing_event_count; ++i){
				retainedTree_.invoke_text_editing(active, retainedInput_.editing_events[i]);
			}

			auto fiberOf = [&](::ui::NodeId id) -> ::ReactFiberId {
				::ui::NodeSnapshot snapshot{};
				return id != 0 && retainedTree_.snapshot(id, &snapshot)
					? snapshot.fiber_id
					: 0;
			};
			retainedInteractionSnapshot_ = {
				.focused_fiber = fiberOf(::ui::focus_focused_id(retainedFocus_)),
				.hovered_fiber = fiberOf(::ui::focus_hovered_id(retainedFocus_)),
				.pressed_fiber = fiberOf(::ui::focus_pressed_id(retainedFocus_)),
				.source = ::ui::focus_source(retainedFocus_),
			};
			if(!::ui::build_draw_command_list(retainedTree_, &retainedCommands_, active)){
				retainedCommands_.reset();
			}
		}else{
			retainedCommands_.reset();
		}
		::react_end_frame();
		retainedFrameOpen_ = false;
	}
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

bool ClientUi::HasTextInputFocus() const {
	if(interactions_.HasTextInputFocus()) return true;
	const ::ui::NodeId focused = ::ui::focus_focused_id(retainedFocus_);
	if(focused == 0) return false;
	::ui::NodeSnapshot snapshot{};
	if(!retainedTree_.snapshot(focused, &snapshot)) return false;
	return snapshot.role == ::ui::NodeRole::Input ||
	       snapshot.semantic_role == ::ui::SemanticRole::TextBox;
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

void ClientUi::BuildRetainedUi(ScreenContext& ctx,
                               const HudView * view,
                               const Resources * resources,
                               Uint8 animationPhase) {
	retainedElementFrame_.reset();
	::ui::UiElementFrameScope scope(retainedElementFrame_);
	::ui::Span<Screen *> visible = screens_.VisibleScreens();
	std::array<::ui::UiElement, CLIENT_UI_MAX_SCREENS + 1> roots = {};
	int rootCount = 0;

	if(view && resources && view->mapLoaded){
		const InGameHudContextValue context{
			.view = view,
			.resources = resources,
			.on_chat_text_change = [this](const std::string& value) {
				clientui_detail::QueueAction(
					interactions_, silencer::ui::UiActionKind::SetText, "ingame.chat", value);
			},
			.on_chat_submit = [this](const std::string& value) {
				clientui_detail::QueueAction(
					interactions_, silencer::ui::UiActionKind::SubmitText, "ingame.chat", value);
			},
			.on_chat_channel_toggle = [this]() {
				clientui_detail::QueueAction(
					interactions_, silencer::ui::UiActionKind::Activate, "ingame.chat.channel");
			},
			.on_buy_tech_focus = [this](int index) {
				clientui_detail::QueueAction(
					interactions_,
					silencer::ui::UiActionKind::Navigate,
					clientui_detail::BuyTechActionId(index).c_str(),
					"focus",
					index);
			},
			.on_buy_tech_activate = [this](int index) {
				clientui_detail::QueueAction(
					interactions_,
					silencer::ui::UiActionKind::Select,
					clientui_detail::BuyTechActionId(index).c_str(),
					"activate",
					index);
			},
			.animationPhase = animationPhase,
		};
		const auto * stored = ::ui::copy_value(context);
		if(!stored){
			retainedCommands_.reset();
			return;
		}
		roots[rootCount++] = InGameHudView(InGameHudViewProps{
			.key = "in-game-hud",
			.value = stored,
		});
	}

	if(visible.count <= 0 && rootCount <= 0){
		retainedCommands_.reset();
		return;
	}
	for(int i = 0; i < visible.count; ++i){
		::ui::UiElement root{};
		if(!visible[i]->BuildElement(ctx, &root)){
			::react_report_error("client/ui: screen did not return retained UI\n");
			retainedCommands_.reset();
			return;
		}
		roots[rootCount++] = root;
	}
	::ui::UiElement root = retainedElementFrame_.fragment(
		retainedElementFrame_.children(roots.data(), rootCount));
	::ui::UiElement provider = ::ui::provider(
		"InteractionProvider",
		&::ui::InteractionContext,
		&retainedInteractionSnapshot_,
		::ui::children({ root }));
	::ui::ReconcileResult result =
		::ui::commit_retained_elements(retainedTree_, retainedElementFrame_, provider);
	if(!result.ok){
		::react_report_error("client/ui: failed to commit retained UI (errors=%d)\n",
		                     result.error_count);
		retainedCommands_.reset();
		return;
	}
}

void ClientUi::RenderRetainedScreens(Renderer& renderer, const Resources& resources, Surface& dst) {
	silencer::client_ui::RenderRetainedDrawCommands(renderer, resources, dst, retainedCommands_);
}

}  // namespace client_ui
}  // namespace silencer
