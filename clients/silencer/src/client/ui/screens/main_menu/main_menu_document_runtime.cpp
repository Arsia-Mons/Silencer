#include "main_menu_document_runtime.h"

namespace silencer::client_ui::main_menu {

bool IsMainMenuComponent(const std::string& component) {
	return component == kMainMenuLogoComponent;
}

bool IsMainMenuTextBinding(const std::string& binding) {
	return binding == kClientVersionBinding;
}

bool IsMainMenuAction(const std::string& action) {
	return action == kActionTutorial || action == kActionLobby ||
	       action == kActionOptions || action == kActionExit;
}

bool ResolveMainMenuTextBinding(const std::string& binding,
                                const std::string& versionText,
                                std::string& out) {
	if(!IsMainMenuTextBinding(binding)) return false;
	out = versionText;
	return true;
}

}  // namespace silencer::client_ui::main_menu
