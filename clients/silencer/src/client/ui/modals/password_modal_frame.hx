#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct PasswordModalFrameProps {
	const char * key = nullptr;
	const char * password_display = nullptr;
};

::ui::UiElement PasswordModalFrame(const PasswordModalFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
