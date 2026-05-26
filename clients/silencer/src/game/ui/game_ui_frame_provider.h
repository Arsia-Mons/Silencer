#pragma once

#include "clay/clay.h"
#include "ui/runtime/UiInputState.h"

#include <functional>

namespace silencer {
namespace game_ui {

struct GameUiFrame {
	silencer::ui::UiInputState input;
	Clay_Dimensions layout = {};
	Clay_Vector2 pointer = {};
	int surfaceWidth = 0;
	int surfaceHeight = 0;
};

using GameUiFrameBuild = std::function<void()>;

void WithPreparedGameUiFrame(const silencer::ui::UiInputState& input,
                             int surfaceWidth,
                             int surfaceHeight,
                             const GameUiFrameBuild& build);
const GameUiFrame * UseGameUiFrame();
const GameUiFrame& RequireGameUiFrame();

}  // namespace game_ui
}  // namespace silencer
