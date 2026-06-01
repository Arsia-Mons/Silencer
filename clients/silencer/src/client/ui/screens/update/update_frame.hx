#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct UpdateFrameProps {
	const char * key = nullptr;
	const char * status_text = nullptr;
	const char * progress_text = nullptr;
	bool can_update = false;
	bool can_cancel = false;
	bool can_retry = false;
	bool can_download = false;
};

::ui::UiElement UpdateFrame(const UpdateFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
