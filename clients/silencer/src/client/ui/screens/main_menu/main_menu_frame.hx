#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace silencer {
namespace client_ui {

struct MainMenuFrameProps {
	const char * key = nullptr;
	const char * version = nullptr;
	int viewport_width = 0;
	int viewport_height = 0;
	std::function<void()> start_tutorial = {};
	std::function<void()> open_lobby = {};
	std::function<void()> open_options = {};
	std::function<void()> quit = {};
};

::ui::UiElement MainMenuFrame(const MainMenuFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
