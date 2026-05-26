#include "ui/game_ui_frame_provider.h"

#include "runtime/react.h"

namespace silencer {
namespace game_ui {

namespace {
ReactContext g_gameUiFrameContext = {};
}

void WithGameUiFrameProvider(const GameUiFrame& frame,
                             const GameUiFrameBuild& build) {
	REACT_PROVIDER_ENTER("GameUiFrameProvider");
	PROVIDE(&g_gameUiFrameContext, const_cast<GameUiFrame *>(&frame)) {
		if(build) build();
	}
	REACT_PROVIDER_EXIT();
}

const GameUiFrame * UseGameUiFrame() {
	return static_cast<const GameUiFrame *>(use_context(&g_gameUiFrameContext));
}

}  // namespace game_ui
}  // namespace silencer
