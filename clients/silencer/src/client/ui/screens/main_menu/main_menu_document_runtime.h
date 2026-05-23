#ifndef MAIN_MENU_DOCUMENT_RUNTIME_H
#define MAIN_MENU_DOCUMENT_RUNTIME_H

#include "components/silencer_logo.h"
#include "layout/ui_document_renderer.h"
#include "ui_editor_preview_model.h"
#include "ui_layout_contract.generated.h"

#include <string>

class Resources;

namespace silencer::client_ui::main_menu {

constexpr const char * kMainMenuSurface =
	silencer::net::ui_layout_contract::kUiSurfaceMainMenu;
constexpr const char * kMainMenuLogoComponent =
	silencer::net::ui_layout_contract::kUiComponentMainMenuLogo;
constexpr const char * kClientVersionBinding =
	silencer::net::ui_layout_contract::kUiTextBindingClientVersion;
constexpr const char * kActionTutorial =
	silencer::net::ui_layout_contract::kUiActionMainMenuTutorial;
constexpr const char * kActionLobby =
	silencer::net::ui_layout_contract::kUiActionMainMenuLobby;
constexpr const char * kActionOptions =
	silencer::net::ui_layout_contract::kUiActionMainMenuOptions;
constexpr const char * kActionExit =
	silencer::net::ui_layout_contract::kUiActionMainMenuExit;

bool IsMainMenuComponent(const std::string& component);
bool IsMainMenuTextBinding(const std::string& binding);
bool IsMainMenuAction(const std::string& action);
bool BuildMainMenuComponent(const silencer::ui::UiEditorNode& node,
                            Resources& resources,
                            SilencerLogo& logo);
bool ResolveMainMenuTextBinding(const std::string& binding,
                                const std::string& versionText,
                                std::string& out);
void ApplyMainMenuRuntimeHandlers(UiDocumentRendererOptions& options,
                                  Resources * resources = nullptr,
                                  SilencerLogo * logo = nullptr,
                                  const std::string * versionText = nullptr);

}  // namespace silencer::client_ui::main_menu

#endif
