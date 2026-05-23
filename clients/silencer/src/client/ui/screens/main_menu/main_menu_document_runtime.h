#ifndef MAIN_MENU_DOCUMENT_RUNTIME_H
#define MAIN_MENU_DOCUMENT_RUNTIME_H

#include <string>

namespace silencer::client_ui::main_menu {

constexpr const char * kMainMenuSurface = "main-menu";
constexpr const char * kMainMenuLogoComponent = "main-menu.logo";
constexpr const char * kClientVersionBinding = "client.version";
constexpr const char * kActionTutorial = "main_menu.tutorial";
constexpr const char * kActionLobby = "main_menu.lobby";
constexpr const char * kActionOptions = "main_menu.options";
constexpr const char * kActionExit = "main_menu.exit";

bool IsMainMenuComponent(const std::string& component);
bool IsMainMenuTextBinding(const std::string& binding);
bool IsMainMenuAction(const std::string& action);
bool ResolveMainMenuTextBinding(const std::string& binding,
                                const std::string& versionText,
                                std::string& out);

}  // namespace silencer::client_ui::main_menu

#endif
