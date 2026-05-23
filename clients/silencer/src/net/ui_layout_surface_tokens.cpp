#include "ui_layout_surface_tokens.h"

namespace silencer::net {

namespace {

bool Contains(const char * const * values,
              std::size_t valueCount,
              const std::string& value) {
	for(std::size_t i = 0; i < valueCount; ++i){
		if(value == values[i]) return true;
	}
	return false;
}

bool NodeHasSurfaceTokens(const silencer::ui::UiEditorNode& node) {
	if(node.kind == ui_layout_contract::kNodeKindComponent ||
	   node.kind == ui_layout_contract::kNodeKindButton ||
	   !node.textBinding.empty()){
		return true;
	}
	for(const silencer::ui::UiEditorNode& child : node.children){
		if(NodeHasSurfaceTokens(child)) return true;
	}
	return false;
}

bool ValidateSurfaceNodeTokens(const silencer::ui::UiEditorNode& node,
                               const ui_layout_contract::SurfaceTokens& tokens,
                               std::string& error) {
	if(node.kind == ui_layout_contract::kNodeKindComponent &&
	   !Contains(tokens.components, tokens.componentCount, node.component)){
		error = "UI layout references unknown component " + node.component +
		        " at node " + node.id;
		return false;
	}
	if(!node.textBinding.empty() &&
	   !Contains(tokens.textBindings, tokens.textBindingCount, node.textBinding)){
		error = "UI layout references unknown text binding " + node.textBinding +
		        " at node " + node.id;
		return false;
	}
	if(node.kind == ui_layout_contract::kNodeKindButton &&
	   !Contains(tokens.actions, tokens.actionCount, node.action)){
		error = "UI layout references unknown action " + node.action +
		        " at node " + node.id;
		return false;
	}
	for(const silencer::ui::UiEditorNode& child : node.children){
		if(!ValidateSurfaceNodeTokens(child, tokens, error)) return false;
	}
	return true;
}

}  // namespace

const ui_layout_contract::SurfaceTokens * FindUiLayoutSurfaceTokens(
	const std::string& surface) {
	for(std::size_t i = 0; i < ui_layout_contract::kSurfaceTokensCount; ++i){
		const auto& candidate = ui_layout_contract::kSurfaceTokens[i];
		if(surface == candidate.surface) return &candidate;
	}
	return nullptr;
}

bool UiLayoutSurfaceAllowsComponent(const std::string& surface,
                                    const std::string& component) {
	const ui_layout_contract::SurfaceTokens * tokens =
		FindUiLayoutSurfaceTokens(surface);
	return tokens &&
	       Contains(tokens->components, tokens->componentCount, component);
}

bool UiLayoutSurfaceAllowsTextBinding(const std::string& surface,
                                      const std::string& binding) {
	const ui_layout_contract::SurfaceTokens * tokens =
		FindUiLayoutSurfaceTokens(surface);
	return tokens &&
	       Contains(tokens->textBindings, tokens->textBindingCount, binding);
}

bool UiLayoutSurfaceAllowsAction(const std::string& surface,
                                 const std::string& action) {
	const ui_layout_contract::SurfaceTokens * tokens =
		FindUiLayoutSurfaceTokens(surface);
	return tokens && Contains(tokens->actions, tokens->actionCount, action);
}

bool ValidateUiDocumentGeneratedSurfaceTokens(
	const silencer::ui::UiEditorPreviewDocument& document,
	std::string& error) {
	const ui_layout_contract::SurfaceTokens * tokens =
		FindUiLayoutSurfaceTokens(document.surface);
	if(!tokens){
		if(NodeHasSurfaceTokens(document.root)){
			error = "UI layout surface " + document.surface +
			        " needs generated UI surface tokens";
			return false;
		}
		return true;
	}
	return ValidateSurfaceNodeTokens(document.root, *tokens, error);
}

}  // namespace silencer::net
