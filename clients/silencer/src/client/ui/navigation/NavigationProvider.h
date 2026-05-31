#pragma once

#include "client/ui/navigation/ScreenStack.h"
#include "ui/runtime/element.h"

#include <SDL3/SDL_stdinc.h>

#include <functional>

class ScreenContext;

namespace silencer {
namespace client_ui {

class ClientUi;

struct NavigationProviderValue {
	ClientUi * clientUi = nullptr;
	UiScreenEntryId currentEntryId = 0;
	bool isTop = false;
};

struct Navigation {
	UiScreenEntryId currentEntryId = 0;
	bool isTop = false;
	std::function<void(Uint8)> goToState = {};
	std::function<void()> goBack = {};
	std::function<void()> requestQuit = {};
};

::ui::UiElement NavigationProvider(const NavigationProviderValue& value,
                                   ::ui::UiChildren children,
                                   const char * key = nullptr);
Navigation UseNavigation();

}  // namespace client_ui
}  // namespace silencer
