#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace silencer {
namespace client_ui {

struct OptionsAudioFrameProps {
	const char * key = nullptr;
	bool music_enabled = false;
	std::function<void()> toggle_music = {};
	std::function<void()> save = {};
	std::function<void()> cancel = {};
};

::ui::UiElement OptionsAudioFrame(const OptionsAudioFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
