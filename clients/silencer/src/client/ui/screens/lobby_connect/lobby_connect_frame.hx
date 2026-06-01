#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct LobbyConnectFrameProps {
	const char * key = nullptr;
	const char * log_text = nullptr;
	const char * username_display = nullptr;
	const char * password_display = nullptr;
	bool inactive = false;
};

::ui::UiElement LobbyConnectFrame(const LobbyConnectFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
