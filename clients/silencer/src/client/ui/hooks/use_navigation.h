#pragma once

#include "client/ui/screens/screen.h"

#include <SDL3/SDL_stdinc.h>

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
	std::function<void(Uint8)> go_to_state = {};
	std::function<void()> go_back = {};
	std::function<void()> request_quit = {};
};

Navigation use_navigation();

}  // namespace client_ui
}  // namespace silencer
