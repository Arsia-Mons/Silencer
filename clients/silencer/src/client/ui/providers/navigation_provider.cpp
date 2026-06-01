#include "client/ui/providers/navigation_provider.h"

#include "client/ui/ClientUi.h"
#include "client/ui/hooks/use_navigation.h"
#include "client/ui/screens/screen_context.h"
#include "ui/runtime/react.h"

#include <utility>

namespace silencer {
namespace client_ui {

namespace {

::ReactContext NavigationContext = {};

NavigationProviderValue * use_navigation_provider_value(const char * hook) {
	auto * value = static_cast<NavigationProviderValue *>(::use_context(&NavigationContext));
	if(!value || !value->client_ui){
		::react_report_error("client/ui: missing NavigationProvider for %s\n", hook);
		return nullptr;
	}
	return value;
}

}  // namespace

::ui::UiElement NavigationProvider(const NavigationProviderValue& value,
                                   ::ui::UiChildren children,
                                   const char * key) {
	const NavigationProviderValue * stored = ::ui::copy_value(value);
	if(!stored){
		::react_report_error("client/ui: failed to store navigation context\n");
		return ::ui::empty();
	}
	return ::ui::provider(
		"NavigationProvider",
		&NavigationContext,
		const_cast<NavigationProviderValue *>(stored),
		children,
		key);
}

Navigation use_navigation() {
	NavigationProviderValue * value = use_navigation_provider_value("use_navigation");
	if(!value) return {};
	ClientUi * client_ui = value->client_ui;
	UiScreenEntryId entry_id = value->current_entry_id;
	return Navigation{
		.current_entry_id = entry_id,
		.is_top = value->is_top,
		.push = [client_ui](std::unique_ptr<Screen> screen) {
			client_ui->QueuePushScreen(std::move(screen));
		},
		.replace = [client_ui](std::unique_ptr<Screen> screen) {
			client_ui->QueueReplaceScreen(std::move(screen));
		},
		.reset_to = [client_ui](std::unique_ptr<Screen> screen) {
			client_ui->QueueResetToScreen(std::move(screen));
		},
		.pop_current = [client_ui, entry_id]() {
			client_ui->QueuePopCurrent(entry_id);
		},
		.pop_top = [client_ui]() {
			client_ui->QueuePopTop();
		},
		.go_to_state = [client_ui](Uint8 state) {
			client_ui->QueueDeferredMutation([state](ScreenContext& ctx) {
				ctx.GoToState(state);
			});
		},
		.go_back = [client_ui]() {
			client_ui->QueueDeferredMutation([](ScreenContext& ctx) {
				ctx.GoBack();
			});
		},
		.request_quit = [client_ui]() {
			client_ui->QueueDeferredMutation([](ScreenContext& ctx) {
				ctx.RequestQuit();
			});
		},
	};
}

}  // namespace client_ui
}  // namespace silencer
