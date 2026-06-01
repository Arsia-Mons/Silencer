#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "client/ui/providers/navigation_provider.h"

#include <functional>
#include <memory>

class Screen;

namespace client::ui {

struct Navigation {
  UiScreenEntryId current_entry_id = 0;
  bool is_top = false;
  std::function<void(std::unique_ptr<UiScreen>)> push = {};
  std::function<void(std::unique_ptr<UiScreen>)> reset_to = {};
  std::function<void()> pop_current = {};
  std::function<void()> pop_top = {};
};

Navigation use_navigation();

} // namespace client::ui

namespace silencer {
namespace client_ui {

struct Navigation {
	Screen * push(std::unique_ptr<Screen> screen) const;
	void reset_to(std::unique_ptr<Screen> screen) const;
	void pop_current() const;
	void pop_top() const;

private:
	friend Navigation use_navigation();
	explicit Navigation(NavigationProviderValue provider);

	NavigationProviderValue provider_;
};

Navigation use_navigation();

}  // namespace client_ui
}  // namespace silencer
