#include "client/ui/ClientUi.h"

#include "client/ui/hud/ingame_hud_view.h"
#include "client/ui/hud/hud_retained_payloads.h"
#include "client/ui/navigation/NavigationProvider.h"
#include "client/ui/views/HudView.h"
#include "screen.h"
#include "screen_context.h"
#include "retained_surface_renderer.h"
#include "runtime/UiInputRouter.h"
#include "ui/runtime/draw_command_builder.h"
#include "ui/runtime/react.h"
#include "ui/runtime/yoga_flex_layout.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

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

bool HasText(const char * value) {
	return value && value[0] != '\0';
}

bool IsHiddenNode(const ::ui::NodeSnapshot& node) {
	return node.visual.hidden ||
	       node.style.display == ::ui::Display::None ||
	       node.layout.width <= 0.0f ||
	       node.layout.height <= 0.0f;
}

silencer::ui::UiElementKind ElementKindForNode(const ::ui::NodeSnapshot& node) {
	switch(node.role){
		case ::ui::NodeRole::Button:
			return silencer::ui::UiElementKind::Button;
		case ::ui::NodeRole::Text:
			return silencer::ui::UiElementKind::Text;
		case ::ui::NodeRole::Input:
			return silencer::ui::UiElementKind::TextField;
		case ::ui::NodeRole::Checkbox:
			return silencer::ui::UiElementKind::Button;
		case ::ui::NodeRole::Dialog:
		case ::ui::NodeRole::Box:
		case ::ui::NodeRole::Generic:
			break;
	}
	switch(node.semantic_role){
		case ::ui::SemanticRole::Button:
		case ::ui::SemanticRole::Checkbox:
			return silencer::ui::UiElementKind::Button;
		case ::ui::SemanticRole::TextBox:
			return silencer::ui::UiElementKind::TextField;
		case ::ui::SemanticRole::Tab:
			return silencer::ui::UiElementKind::Tab;
		case ::ui::SemanticRole::Dialog:
		case ::ui::SemanticRole::Auto:
			break;
	}
	return silencer::ui::UiElementKind::Container;
}

bool IsTextInputNode(const ::ui::NodeSnapshot& node) {
	return node.role == ::ui::NodeRole::Input ||
	       node.semantic_role == ::ui::SemanticRole::TextBox;
}

bool IsListRowNode(const ::ui::NodeSnapshot& node) {
	if(!HasText(node.control_id)) return false;
	return std::strcmp(node.control_id, "lobby.game_select.row") == 0 ||
	       std::strcmp(node.control_id, "lobby.game_create.map") == 0 ||
	       std::strcmp(node.control_id, "lobby.game_tech.toggle") == 0;
}

silencer::ui::UiInteractableKind InteractableKindForNode(const ::ui::NodeSnapshot& node) {
	if(IsTextInputNode(node)) return silencer::ui::UiInteractableKind::TextInput;
	if(node.role == ::ui::NodeRole::Checkbox ||
	   node.semantic_role == ::ui::SemanticRole::Checkbox){
		return silencer::ui::UiInteractableKind::Toggle;
	}
	if(IsListRowNode(node)) return silencer::ui::UiInteractableKind::ListRow;
	return silencer::ui::UiInteractableKind::Button;
}

std::string LabelForNode(const ::ui::NodeSnapshot& node) {
	if(HasText(node.accessibility_label)) return node.accessibility_label;
	if(HasText(node.value)) return node.value;
	if(HasText(node.control_id)) return node.control_id;
	return std::string();
}

silencer::ui::UiRect BoundsForNode(const ::ui::NodeSnapshot& node) {
	return silencer::ui::UiRect{
		node.layout.x,
		node.layout.y,
		node.layout.width,
		node.layout.height,
	};
}

void FindLastModalNode(const ::ui::UiTree& tree, ::ui::NodeId id, ::ui::NodeId& out) {
	::ui::NodeSnapshot node{};
	if(!tree.snapshot(id, &node)) return;
	if(!IsHiddenNode(node) && node.interaction.modal) out = id;
	for(int i = 0; i < tree.child_count(id); ++i){
		FindLastModalNode(tree, tree.child_at(id, i), out);
	}
}

void RegisterRetainedNode(const ::ui::UiTree& tree,
                          ::ui::NodeId id,
                          silencer::ui::UiInteractionRegistry& interactions,
                          int& nextUid,
                          ::ui::NodeId focusedNode) {
	::ui::NodeSnapshot node{};
	if(!tree.snapshot(id, &node)) return;
	if(IsHiddenNode(node)) return;

	const std::string label = LabelForNode(node);
	const bool hasId = HasText(node.control_id);
	const bool hasLabel = !label.empty();
	const bool hasValue = HasText(node.value);
	if(hasId || hasLabel || hasValue){
		silencer::ui::UiElementSnapshot element;
		if(hasId) element.id = node.control_id;
		element.kind = ElementKindForNode(node);
		element.label = label;
		if(IsTextInputNode(node)){
			element.value = node.password
				? std::string(std::strlen(node.value), '*')
				: std::string(node.value ? node.value : "");
		}else if(HasText(node.accessibility_description)){
			element.value = node.accessibility_description;
		}else if(hasValue){
			element.value = node.value;
		}
		element.bounds = BoundsForNode(node);
		element.enabled = !node.interaction.disabled;
		element.focused = node.id == focusedNode;
		element.selected = node.interaction.selected || node.interaction.checked;
		interactions.Register(std::move(element));
	}

	if(node.interaction.focusable &&
	   (node.role == ::ui::NodeRole::Button ||
	    node.role == ::ui::NodeRole::Input ||
	    node.role == ::ui::NodeRole::Checkbox ||
	    node.semantic_role == ::ui::SemanticRole::Button ||
	    node.semantic_role == ::ui::SemanticRole::TextBox ||
	    node.semantic_role == ::ui::SemanticRole::Checkbox)){
		silencer::ui::UiInteractable widget;
		widget.id = hasId ? node.control_id : std::string();
		widget.labelText = label;
		widget.kind = InteractableKindForNode(node);
		widget.uid = nextUid++;
		widget.x = static_cast<int>(std::floor(node.layout.x));
		widget.y = static_cast<int>(std::floor(node.layout.y));
		widget.w = static_cast<int>(std::ceil(node.layout.width));
		widget.h = static_cast<int>(std::ceil(node.layout.height));
		widget.index = widget.kind == silencer::ui::UiInteractableKind::ListRow
			? node.control_offset
			: -1;
		widget.selected = node.interaction.selected || node.interaction.checked;
		widget.value = node.value ? node.value : "";
		widget.isPassword = node.password;
		widget.inactive = node.interaction.disabled;
		interactions.RegisterInteractable(std::move(widget));
	}

	for(int i = 0; i < tree.child_count(id); ++i){
		RegisterRetainedNode(tree, tree.child_at(id, i), interactions, nextUid, focusedNode);
	}
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

const char * ScreenProviderKey(UiScreenEntryId entryId) {
	return ::ui::copy_string(("screen-provider-" + std::to_string(entryId)).c_str());
}

::ui::InputFrame ToRetainedInput(const silencer::ui::UiInputState& input) {
	::ui::InputFrame out{};
	out.pointer_pressed = input.pointer.pressed;
	out.pointer_down = input.pointer.down;
	out.pointer_released = input.pointer.released;
	out.pointer_valid = input.HasWindow();
	out.pointer_x = input.pointer.x;
	out.pointer_y = input.pointer.y;
	bool controlPointer = false;
	for(const silencer::ui::UiControlCommand& command : input.controlCommands){
		if(command.kind == silencer::ui::UiControlCommandKind::PointerPress ||
		   command.kind == silencer::ui::UiControlCommandKind::PointerHover){
			controlPointer = true;
			break;
		}
	}
	out.source = (controlPointer || input.pointer.pressed || input.pointer.down || input.pointer.released)
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
	::react_init_runtime();
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
			RefreshRetainedInteractions();
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

void ClientUi::RefreshRetainedInteractions() {
	if(!retainedTree_.contains(retainedTree_.root_id())) return;
	::ui::NodeId root = retainedTree_.root_id();
	::ui::NodeId modal = 0;
	clientui_detail::FindLastModalNode(retainedTree_, root, modal);
	int nextUid = 1;
	clientui_detail::RegisterRetainedNode(
		retainedTree_,
		modal != 0 ? modal : root,
		interactions_,
		nextUid,
		::ui::focus_focused_id(retainedFocus_));
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
	if(!screen || screens_.Size() >= CLIENT_UI_MAX_SCREENS) return;
	screen->Build(ctx);
	screens_.Push(std::move(screen));
}

void ClientUi::PopScreen(ScreenContext& ctx) {
	Screen * top = screens_.Top();
	if(!top) return;
	top->Destroy(ctx);
	screens_.Pop();
}

void ClientUi::ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return;
	Screen * top = screens_.Top();
	if(!top){
		PushScreen(std::move(screen), ctx);
		return;
	}
	top->Destroy(ctx);
	screen->Build(ctx);
	screens_.Replace(std::move(screen));
}

bool ClientUi::QueueDeferredMutation(DeferredUiMutation mutation) {
	if(!mutation) return false;
	deferredMutations_.push_back(std::move(mutation));
	return true;
}

void ClientUi::DrainDeferredMutations(ScreenContext& ctx) {
	if(deferredMutations_.empty()) return;
	std::vector<DeferredUiMutation> mutations;
	mutations.swap(deferredMutations_);
	for(DeferredUiMutation& mutation : mutations){
		if(mutation) mutation(ctx);
	}
}

void ClientUi::RequestClearScreens() {
	screens_.RequestClear();
}

void ClientUi::ClearScreensIfRequested(ScreenContext& ctx) {
	if(!screens_.ConsumeClearRequest()) return;
	while(Screen * top = screens_.Top()){
		top->Destroy(ctx);
		screens_.Pop();
	}
}

void ClientUi::TickVisibleScreens(ScreenContext& ctx) {
	::ui::Span<Screen *> visible = screens_.VisibleScreens();
	for(int i = 0; i < visible.count; ++i) {
		Screen * screen = visible[i];
		if(!screen) continue;
		const int stackSize = screens_.Size();
		const UiScreenEntryId entryId = screen->EntryId();
		screen->Tick(ctx);
		if(screens_.Size() != stackSize || !screens_.ContainsEntry(entryId)){
			break;
		}
	}
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
			.state = {
				.view = view,
				.resources = resources,
				.animationPhase = animationPhase,
			},
			.actions = {
				.set_chat_text = [this](const std::string& value) {
					clientui_detail::QueueAction(
						interactions_, silencer::ui::UiActionKind::SetText, "ingame.chat", value);
				},
				.submit_chat_text = [this](const std::string& value) {
					clientui_detail::QueueAction(
						interactions_, silencer::ui::UiActionKind::SubmitText, "ingame.chat", value);
				},
				.toggle_chat_channel = [this]() {
					clientui_detail::QueueAction(
						interactions_, silencer::ui::UiActionKind::Activate, "ingame.chat.channel");
				},
				.focus_buy_tech = [this](int index) {
					clientui_detail::QueueAction(
						interactions_,
						silencer::ui::UiActionKind::Navigate,
						clientui_detail::BuyTechActionId(index).c_str(),
						"focus",
						index);
				},
				.activate_buy_tech = [this](int index) {
					clientui_detail::QueueAction(
						interactions_,
						silencer::ui::UiActionKind::Select,
						clientui_detail::BuyTechActionId(index).c_str(),
						"activate",
						index);
				},
			},
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
		NavigationProviderValue navigation{
			.clientUi = this,
			.currentEntryId = visible[i]->EntryId(),
			.isTop = i == visible.count - 1,
		};
		roots[rootCount++] = NavigationProvider(
			navigation,
			::ui::children({ root }),
			clientui_detail::ScreenProviderKey(visible[i]->EntryId()));
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
