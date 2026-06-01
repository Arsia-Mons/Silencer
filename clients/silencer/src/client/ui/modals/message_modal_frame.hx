#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct MessageModalFrameProps {
	const char * key = nullptr;
	const char * message = nullptr;
	bool show_ok = true;
};

::ui::UiElement MessageModalFrame(const MessageModalFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
