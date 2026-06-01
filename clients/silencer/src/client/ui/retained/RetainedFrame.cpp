#include "client/ui/retained/RetainedFrame.h"

#include "ui/runtime/draw_command_builder.h"
#include "ui/runtime/react.h"
#include "ui/runtime/yoga_flex_layout.h"

#include <cstring>
#include <mutex>
#include <string>
#include <utility>

namespace silencer {
namespace client_ui {

namespace {

silencer::ui::UiInteractableKind InteractableKindFor(::ui::NodeSnapshot node) {
	if(node.role == ::ui::NodeRole::Input ||
	   node.semantic_role == ::ui::SemanticRole::TextBox){
		return silencer::ui::UiInteractableKind::TextInput;
	}
	if(node.semantic_role == ::ui::SemanticRole::Checkbox ||
	   node.role == ::ui::NodeRole::Checkbox){
		return silencer::ui::UiInteractableKind::Toggle;
	}
	if(node.semantic_role == ::ui::SemanticRole::ListRow){
		return silencer::ui::UiInteractableKind::ListRow;
	}
	return silencer::ui::UiInteractableKind::Button;
}

silencer::ui::UiElementKind ElementKindFor(::ui::NodeSnapshot node) {
	if(node.role == ::ui::NodeRole::Text){
		return silencer::ui::UiElementKind::Text;
	}
	if(node.role == ::ui::NodeRole::Input ||
	   node.semantic_role == ::ui::SemanticRole::TextBox){
		return silencer::ui::UiElementKind::TextField;
	}
	if(node.semantic_role == ::ui::SemanticRole::Button ||
	   node.role == ::ui::NodeRole::Button){
		return silencer::ui::UiElementKind::Button;
	}
	if(node.semantic_role == ::ui::SemanticRole::Checkbox ||
	   node.role == ::ui::NodeRole::Checkbox){
		return silencer::ui::UiElementKind::Button;
	}
	if(node.semantic_role == ::ui::SemanticRole::ListRow){
		return silencer::ui::UiElementKind::ListItem;
	}
	return silencer::ui::UiElementKind::Container;
}

const char * LabelFor(::ui::NodeSnapshot node) {
	if(node.accessibility_label && *node.accessibility_label){
		return node.accessibility_label;
	}
	if(node.value && *node.value){
		return node.value;
	}
	return "";
}

bool IsInteractive(::ui::NodeSnapshot node) {
	return node.interaction.focusable ||
	       node.role == ::ui::NodeRole::Button ||
	       node.role == ::ui::NodeRole::Input ||
	       node.role == ::ui::NodeRole::Checkbox ||
	       node.semantic_role == ::ui::SemanticRole::Button ||
	       node.semantic_role == ::ui::SemanticRole::TextBox ||
	       node.semantic_role == ::ui::SemanticRole::Checkbox ||
	       node.semantic_role == ::ui::SemanticRole::ListRow;
}

void EnsureReactRuntime() {
	static std::once_flag once;
	std::call_once(once, []() { react_init_runtime(); });
}

}  // namespace

bool RetainedFrame::Build(BuildRoot buildRoot,
                          int width,
                          int height,
                          silencer::ui::UiInteractionRegistry& interactions) {
	EnsureReactRuntime();
	if(width < 1) width = 1;
	if(height < 1) height = 1;
	if(!layout_.compute){
		layout_ = ::ui::make_yoga_flex_layout_adapter();
	}

	commands_.reset();
	elementFrame_.reset();
	react_begin_frame();
	tree_.begin_frame(static_cast<float>(width), static_cast<float>(height));
	{
		::ui::UiElementFrameScope frameScope(elementFrame_);
		::ui::UiElement root = buildRoot ? buildRoot() : ::ui::empty();
		::ui::ReconcileResult result =
			::ui::commit_retained_elements(tree_, elementFrame_, root);
		if(!result.ok){
			react_report_error(
				"client/ui: retained screen commit failed (errors=%d)\n",
				result.error_count);
		}
	}

	bool treeOk = tree_.end_frame();
	if(!treeOk){
		react_report_error("client/ui: retained screen tree frame failed\n");
	}
	if(treeOk){
		bool layoutOk = ::ui::compute_flex_layout(
			layout_, tree_,
			::ui::LayoutViewport{static_cast<float>(width),
			                     static_cast<float>(height)});
		if(!layoutOk){
			react_report_error("client/ui: retained screen layout failed\n");
		}
	}
	RegisterAutomation(tree_.root_id(), interactions);
	bool commandsOk = ::ui::build_draw_command_list(tree_, &commands_, 0);
	if(!commandsOk){
		react_report_error(
			"client/ui: retained screen draw-list build failed (errors=%d)\n",
			commands_.error_count);
	}
	react_end_frame();
	return treeOk && commandsOk;
}

bool RetainedFrame::HandleUiIntent(
	const silencer::ui::UiAction& action) const {
	if(action.kind != silencer::ui::UiActionKind::Activate &&
	   action.kind != silencer::ui::UiActionKind::Navigate &&
	   action.kind != silencer::ui::UiActionKind::Select &&
	   action.kind != silencer::ui::UiActionKind::SetText &&
	   action.kind != silencer::ui::UiActionKind::SubmitText &&
	   action.kind != silencer::ui::UiActionKind::Scroll){
		return false;
	}
	if(action.kind != silencer::ui::UiActionKind::Scroll && action.id.empty()){
		return false;
	}
	return InvokeActionForNode(tree_.root_id(), action);
}

bool RetainedFrame::InvokeActionForNode(
	::ui::NodeId id,
	const silencer::ui::UiAction& action) const {
	::ui::NodeSnapshot node = {};
	if(!tree_.snapshot(id, &node)) return false;

	const bool matches =
		node.control_id && action.id == node.control_id;
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		if(action.id.empty() || matches){
			if(tree_.invoke_scroll(
				   node.id, action.amount, action.value.c_str())){
				return true;
			}
		}
	}else if(matches && !node.interaction.disabled){
		if(action.kind == silencer::ui::UiActionKind::SetText){
			if(tree_.invoke_text_change(node.id, action.value.c_str())){
				return true;
			}
		}else if(action.kind == silencer::ui::UiActionKind::SubmitText){
			if(tree_.invoke_text_submit(node.id, action.value.c_str())){
				return true;
			}
		}else if(action.kind == silencer::ui::UiActionKind::Navigate){
			if(tree_.invoke_focus(node.id)){
				return true;
			}
		}else if(tree_.invoke_activate(node.id)){
			return true;
		}
	}

	for(int i = 0; i < tree_.child_count(id); ++i){
		if(InvokeActionForNode(tree_.child_at(id, i), action)){
			return true;
		}
	}
	return false;
}

void RetainedFrame::RegisterAutomation(
	::ui::NodeId id,
	silencer::ui::UiInteractionRegistry& interactions) const {
	::ui::NodeSnapshot node = {};
	if(!tree_.snapshot(id, &node)) return;

	const char * controlId =
		(node.control_id && *node.control_id) ? node.control_id : "";
	const char * label = LabelFor(node);
	if(*controlId || *label){
		const char * nodeValue = node.value ? node.value : "";
		const char * inputValue =
			(node.text_edit.has_input_value && node.text_edit.input_value)
				? node.text_edit.input_value
				: nodeValue;
		silencer::ui::UiElementSnapshot element;
		element.id = controlId;
		element.kind = ElementKindFor(node);
		element.label = label;
		element.value = node.text_edit.password
			? std::string(std::strlen(inputValue), '*')
			: nodeValue;
		element.bounds = silencer::ui::UiRect{
			node.layout.x,
			node.layout.y,
			node.layout.width,
			node.layout.height,
		};
		element.enabled = !node.interaction.disabled;
		element.focused = false;
		element.selected = node.interaction.checked;
		interactions.Register(std::move(element));
	}

	if(IsInteractive(node)){
		const char * nodeValue = node.value ? node.value : "";
		const char * inputValue =
			(node.text_edit.has_input_value && node.text_edit.input_value)
				? node.text_edit.input_value
				: nodeValue;
		silencer::ui::UiInteractable widget;
		widget.id = controlId;
		widget.labelText = label;
		widget.kind = InteractableKindFor(node);
		widget.uid =
			node.control_offset ? static_cast<int>(node.control_offset) : -1;
		if(node.semantic_role == ::ui::SemanticRole::ListRow &&
		   node.control_offset > 0){
			widget.index = static_cast<int>(node.control_offset - 1);
		}
		widget.x = static_cast<int>(node.layout.x);
		widget.y = static_cast<int>(node.layout.y);
		widget.w = static_cast<int>(node.layout.width);
		widget.h = static_cast<int>(node.layout.height);
		widget.selected = node.interaction.checked;
		widget.value = inputValue;
		widget.inactive = node.interaction.disabled;
		widget.cancelOnEscape = node.interaction.cancel_on_escape;
		if(widget.kind == silencer::ui::UiInteractableKind::TextInput){
			widget.maxLength = node.text_edit.max_length;
			widget.numbersOnly = node.text_edit.numbers_only;
			widget.isPassword = node.text_edit.password;
		}
		const std::string focusId = widget.id;
		const int focusUid = widget.uid;
		const bool wantsInitialFocus =
			node.interaction.initial_focus && !interactions.HasFocus();
		interactions.RegisterInteractable(std::move(widget));
		if(wantsInitialFocus){
			if(!focusId.empty()){
				interactions.FocusInteractableById(focusId);
			}else if(focusUid >= 0){
				interactions.FocusTextInputByUid(focusUid);
			}
		}
	}

	for(int i = 0; i < tree_.child_count(id); ++i){
		RegisterAutomation(tree_.child_at(id, i), interactions);
	}
}

}  // namespace client_ui
}  // namespace silencer
