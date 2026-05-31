#pragma once

#include "client/ui/views/HudView.h"
#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>
#include <string>

class Resources;

namespace silencer::client_ui {

struct InGameHudContextValue {
	const HudView * view = nullptr;
	const Resources * resources = nullptr;
	std::function<void(const std::string&)> on_chat_text_change = {};
	std::function<void(const std::string&)> on_chat_submit = {};
	std::function<void()> on_chat_channel_toggle = {};
	std::function<void(int)> on_buy_tech_focus = {};
	std::function<void(int)> on_buy_tech_activate = {};
	Uint8 animationPhase = 0;
};

extern ::ReactContext InGameHudContext;

const InGameHudContextValue& UseInGameHud();

struct InGameHudViewProps {
	const char * key = nullptr;
	const InGameHudContextValue * value = nullptr;
};

::ui::UiElement InGameHudView(const InGameHudViewProps& props);

}  // namespace silencer::client_ui
