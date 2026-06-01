#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct OptionsAudioFrameProps {
	const char * key = nullptr;
	bool music_enabled = false;
};

::ui::UiElement OptionsAudioFrame(const OptionsAudioFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
