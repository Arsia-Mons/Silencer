#pragma once

#include "client/ui/screens/screen.h"
#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

class ClientUi;

struct NavigationProviderValue {
	ClientUi * client_ui = nullptr;
	UiScreenEntryId current_entry_id = 0;
	bool is_top = false;
};

::ui::UiElement NavigationProvider(const NavigationProviderValue& value,
                                   ::ui::UiChildren children,
                                   const char * key = nullptr);

}  // namespace client_ui
}  // namespace silencer
