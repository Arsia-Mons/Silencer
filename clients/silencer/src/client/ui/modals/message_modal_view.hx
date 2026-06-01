#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>

namespace silencer::client_ui {

struct MessageModalDialog {
	const char * message = "";
	bool show_ok = true;
	std::function<void()> close = {};
};

const MessageModalDialog& UseMessageModalDialog();

struct MessageModalFrameProps {
	const char * key = nullptr;
};

::ui::UiElement MessageModalFrame(const MessageModalFrameProps& props);

struct MessageModalViewProps {
	const char * key = nullptr;
	const MessageModalDialog * dialog = nullptr;
};

::ui::UiElement MessageModalView(const MessageModalViewProps& props);

}  // namespace silencer::client_ui
