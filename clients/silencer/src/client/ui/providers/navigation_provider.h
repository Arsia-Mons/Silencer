#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

#include <functional>
#include <memory>

class Screen;

namespace client::ui {

class ClientUi;

struct NavigationProviderValue {
  ClientUi *client_ui = nullptr;
  UiScreenEntryId current_entry_id = 0;
  bool is_top = false;
};

::ui::UiElement NavigationProvider(const NavigationProviderValue &value,
                                   ::ui::UiChildren children,
                                   const char *key = nullptr);

} // namespace client::ui

namespace silencer {
namespace client_ui {

struct NavigationProviderValue {
	std::function<Screen *(std::unique_ptr<Screen>)> push = {};
	std::function<void(std::unique_ptr<Screen>)> reset_to = {};
	std::function<void()> pop_current = {};
	std::function<void()> pop_top = {};
};

class NavigationProviderScope {
public:
	explicit NavigationProviderScope(NavigationProviderValue value);
	~NavigationProviderScope();

	NavigationProviderScope(const NavigationProviderScope&) = delete;
	NavigationProviderScope& operator=(const NavigationProviderScope&) = delete;

private:
	NavigationProviderValue value_;
	NavigationProviderValue * previous_;
};

}  // namespace client_ui
}  // namespace silencer
