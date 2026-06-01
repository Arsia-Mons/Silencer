#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct OptionsFrameProps {
	const char * key = nullptr;
};

::ui::UiElement OptionsFrame(const OptionsFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
