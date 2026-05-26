#pragma once

#include "ui/runtime/UiInputState.h"

#include <functional>

namespace silencer {
namespace game_ui {

struct GameUiFrame {
	silencer::ui::UiInputState input;
	int surfaceWidth = 0;
	int surfaceHeight = 0;
};

using GameUiFrameBuild = std::function<void()>;

void WithGameUiFrameProvider(const GameUiFrame& frame,
                             const GameUiFrameBuild& build);
const GameUiFrame * UseGameUiFrame();

}  // namespace game_ui
}  // namespace silencer
