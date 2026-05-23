#include "main_menu_document_runtime.h"

namespace silencer::client_ui::main_menu {

bool IsMainMenuComponent(const std::string& component) {
	return component == kMainMenuLogoComponent;
}

bool IsMainMenuTextBinding(const std::string& binding) {
	return binding == kClientVersionBinding;
}

bool IsMainMenuAction(const std::string& action) {
	return action == kActionTutorial ||
	       action == kActionLobby ||
	       action == kActionOptions ||
	       action == kActionExit;
}

bool BuildMainMenuComponent(const silencer::ui::UiEditorNode& node,
                            Resources& resources,
                            SilencerLogo& logo) {
	if(!IsMainMenuComponent(node.component)) return false;
	logo.Build(resources);
	return true;
}

bool ResolveMainMenuTextBinding(const std::string& binding,
                                const std::string& versionText,
                                std::string& out) {
	if(!IsMainMenuTextBinding(binding)) return false;
	out = versionText;
	return true;
}

void ApplyMainMenuRuntimeHandlers(UiDocumentRendererOptions& options,
                                  Resources * resources,
                                  SilencerLogo * logo,
                                  const std::string * versionText) {
	options.canBuildComponent = IsMainMenuComponent;
	options.canResolveTextBinding = IsMainMenuTextBinding;
	options.canHandleAction = IsMainMenuAction;
	if(resources && logo){
		options.buildComponent = [resources, logo](const silencer::ui::UiEditorNode& node) {
			return BuildMainMenuComponent(node, *resources, *logo);
		};
	}
	if(versionText){
		options.resolveTextBinding = [versionText](const std::string& binding, std::string& out) {
			return ResolveMainMenuTextBinding(binding, *versionText, out);
		};
	}
}

}  // namespace silencer::client_ui::main_menu
