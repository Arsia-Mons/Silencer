#pragma once

#include "client/ui/views/HudView.h"
#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>
#include <string>

class Resources;

namespace silencer::client_ui {

struct InGameHudContextValue {
	struct State {
		const HudView * view = nullptr;
		const Resources * resources = nullptr;
		Uint8 animationPhase = 0;
	};

	struct Actions {
		std::function<void(const std::string&)> set_chat_text = {};
		std::function<void(const std::string&)> submit_chat_text = {};
		std::function<void()> toggle_chat_channel = {};
		std::function<void(int)> focus_buy_tech = {};
		std::function<void(int)> activate_buy_tech = {};
	};

	State state = {};
	Actions actions = {};
};

const InGameHudContextValue& UseInGameHud();

struct InGameHudViewProps {
	const char * key = nullptr;
	const InGameHudContextValue * value = nullptr;
};

::ui::UiElement InGameHudView(const InGameHudViewProps& props);

}  // namespace silencer::client_ui
