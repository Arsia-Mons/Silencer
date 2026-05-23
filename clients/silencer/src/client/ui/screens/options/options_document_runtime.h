#ifndef OPTIONS_DOCUMENT_RUNTIME_H
#define OPTIONS_DOCUMENT_RUNTIME_H

#include "ui_layout_contract.generated.h"

namespace silencer::client_ui::options_menu {

constexpr const char * kOptionsSurface =
	silencer::net::ui_layout_contract::kUiSurfaceOptions;
constexpr const char * kActionControls =
	silencer::net::ui_layout_contract::kUiActionOptionsControls;
constexpr const char * kActionDisplay =
	silencer::net::ui_layout_contract::kUiActionOptionsDisplay;
constexpr const char * kActionAudio =
	silencer::net::ui_layout_contract::kUiActionOptionsAudio;
constexpr const char * kActionBack =
	silencer::net::ui_layout_contract::kUiActionOptionsBack;

}  // namespace silencer::client_ui::options_menu

#endif
