#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>

namespace silencer::client_ui {

struct MessageModalState {
	const char * message = "";
	bool show_ok = true;
};

struct MessageModalActions {
	std::function<void()> close = {};
};

struct MessageModalContextValue {
	MessageModalState state = {};
	MessageModalActions actions = {};
};

const MessageModalContextValue& UseMessageModal();

struct MessageModalFrameProps {
	const char * key = nullptr;
};

::ui::UiElement MessageModalFrame(const MessageModalFrameProps& props);

struct MessageModalViewProps {
	const char * key = nullptr;
	const MessageModalContextValue * value = nullptr;
};

::ui::UiElement MessageModalView(const MessageModalViewProps& props);

}  // namespace silencer::client_ui
