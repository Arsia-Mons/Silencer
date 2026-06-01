#pragma once

#include "client/ui/screens/screen.h"

#include <functional>
#include <memory>

namespace silencer {
namespace client_ui {

struct Navigation {
	UiScreenEntryId current_entry_id = 0;
	bool is_top = false;
	std::function<void(std::unique_ptr<Screen>)> push = {};
	std::function<void(std::unique_ptr<Screen>)> replace = {};
	std::function<void(std::unique_ptr<Screen>)> reset_to = {};
	std::function<void()> pop_current = {};
	std::function<void()> pop_top = {};
};

Navigation use_navigation();

}  // namespace client_ui
}  // namespace silencer
