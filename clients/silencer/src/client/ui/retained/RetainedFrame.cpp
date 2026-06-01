#include "client/ui/retained/RetainedFrame.h"

#include "ui/runtime/draw_command_builder.h"
#include "ui/runtime/react.h"
#include "ui/runtime/yoga_flex_layout.h"

#include <mutex>
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
	   action.kind != silencer::ui::UiActionKind::Select){
		return false;
	}
	if(action.id.empty()) return false;
	return InvokeActionForNode(tree_.root_id(), action);
}

bool RetainedFrame::InvokeActionForNode(
	::ui::NodeId id,
	const silencer::ui::UiAction& action) const {
	::ui::NodeSnapshot node = {};
	if(!tree_.snapshot(id, &node)) return false;

	const bool matches =
		node.control_id && action.id == node.control_id;
	if(matches && !node.interaction.disabled &&
	   tree_.invoke_activate(node.id)){
		return true;
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
		silencer::ui::UiElementSnapshot element;
		element.id = controlId;
		element.kind = ElementKindFor(node);
		element.label = label;
		element.value = node.value ? node.value : "";
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
		widget.value = node.value ? node.value : "";
		widget.inactive = node.interaction.disabled;
		interactions.RegisterInteractable(std::move(widget));
	}

	for(int i = 0; i < tree_.child_count(id); ++i){
		RegisterAutomation(tree_.child_at(id, i), interactions);
	}
}

}  // namespace client_ui
}  // namespace silencer
