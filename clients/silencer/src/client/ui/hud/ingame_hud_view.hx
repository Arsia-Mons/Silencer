#pragma once

#include "client/ui/views/HudView.h"
#include "ui/components/common.h"
#include "ui/runtime/react.h"

class Resources;

namespace silencer::client_ui {

struct InGameHudContextValue {
	const HudView * view = nullptr;
	const Resources * resources = nullptr;
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
