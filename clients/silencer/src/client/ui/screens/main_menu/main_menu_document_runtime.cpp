#include "main_menu_document_runtime.h"

namespace silencer::client_ui::main_menu {

bool IsMainMenuComponent(const std::string& component) {
	return component == kMainMenuLogoComponent;
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
	if(binding != kClientVersionBinding) return false;
	out = versionText;
	return true;
}

}  // namespace silencer::client_ui::main_menu
