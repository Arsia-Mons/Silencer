#include "ui/game_ui_frame_provider.h"

#include "runtime/react.h"

namespace silencer {
namespace game_ui {

namespace {
ReactContext g_gameUiFrameContext = {};

void WithGameUiFrameProvider(const GameUiFrame& frame,
                             const GameUiFrameBuild& build) {
	REACT_PROVIDER_ENTER("GameUiFrameProvider");
	PROVIDE(&g_gameUiFrameContext, const_cast<GameUiFrame *>(&frame)) {
		if(build) build();
	}
	REACT_PROVIDER_EXIT();
}
}

void WithPreparedGameUiFrame(const silencer::ui::UiInputState& input,
                             int surfaceWidth,
                             int surfaceHeight,
                             const GameUiFrameBuild& build) {
	GameUiFrame frame;
	frame.input = input;
	frame.layout = Clay_Dimensions{
		static_cast<float>(input.width),
		static_cast<float>(input.height),
	};
	frame.pointer = Clay_Vector2{input.pointer.x, input.pointer.y};
	frame.surfaceWidth = surfaceWidth;
	frame.surfaceHeight = surfaceHeight;
	WithGameUiFrameProvider(frame, build);
}

const GameUiFrame * UseGameUiFrame() {
	return static_cast<const GameUiFrame *>(use_context(&g_gameUiFrameContext));
}

}  // namespace game_ui
}  // namespace silencer
